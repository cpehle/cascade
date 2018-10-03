/*
Copyright 2007, D. E. Shaw Research.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions, and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of D. E. Shaw Research nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//////////////////////////////////////////////////////////////////////
//
// Interface.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include <descore/Coverage.hpp>
#include <descore/StringTable.hpp>

BEGIN_NAMESPACE_CASCADE

// String table used to create individual port names for static named port arrays
static descore::StringTable g_namedPortArrays;

////////////////////////////////////////////////////////////////////////
//
// InterfaceDescriptor()
//
////////////////////////////////////////////////////////////////////////
InterfaceDescriptor::InterfaceDescriptor (PreConstructFunction preConstruct, const char *name, const char *className, uint32 maxOffset) :
m_name(name), 
m_className(className),
m_vtable(NULL),
m_preConstruct(preConstruct),
m_maxOffset(maxOffset),
m_depth(1),
m_validateIndex(-1), 
m_validateClockIndex(-1),
m_containsArray(false),
m_containsArrayWithClock(false),
m_entryNameIndex(-1)
{
}

/////////////////////////////////////////////////////////////////
//
// addPortName()
//
/////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addPortName (uint32 offset, const char *name, int arrayLen, int stride)
{
    if (m_entryNameIndex == -1)
    {
        // Check for static array
        if (!arrayLen)
        {
            EntryName entryName = {offset, 0, name};
            m_entryNames.push(entryName);
        }
        else
        {
            offset -= arrayLen * stride;
            const char *ch = strchr(name, '[');
            if (!ch)
                ch = name + strlen(name);
            else
                assert(!strchr(ch+1, '['), "Invalid port array %s: multi-dimensional arrays are not supported", name);

            // Construct the base name
            int len = ch - name;
            char *name0 = new char[len + 1];
            memcpy(name0, name, len);
            name0[len] = 0;

            // Construct the individual element names
            for (int i = 0 ; i < arrayLen ; i++)
            {
                strbuff s("%s[%d]", name0, i);
                EntryName entryName = {offset + i * stride, i ? 1u : 0u, g_namedPortArrays.insert(s)};
                m_entryNames.push(entryName);
            }

            delete[] name0;
        }
    }
}

////////////////////////////////////////////////////////////////////////
//
// beginInterface()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::beginInterface (void *_interface)
{
    (*m_preConstruct)(_interface, this);
    m_entryNameIndex = 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// addSignal()
//
////////////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addSignal (uint32 offset, const char *name, int arrayLen, int stride, const PortInfo *info)
{
    InterfaceEntry entry;
    entry.offset = offset;
    entry.id = -1;
    entry.flags = 0;
    entry.direction = PORT_SIGNAL;
    entry.name = name;
    entry.portInfo = info;

    if (!arrayLen)
        addEntry(entry, false);
    else
    {
        entry.offset -= arrayLen * stride;
        const char *ch = strchr(name, '[');
        if (!ch)
            ch = name + strlen(name);

        // Construct the base name
        int len = ch - name;
        char *name0 = new char[len + 1];
        memcpy(name0, name, len);
        name0[len] = 0;

        // Construct the individual element names
        for (int i = 0 ; i < arrayLen ; i++, entry.offset += stride)
        {
            strbuff s("%s[%d]", name0, i);
            entry.name = g_namedPortArrays.insert(s);
            addEntry(entry, false);
        }

        delete[] name0;
    }
}

////////////////////////////////////////////////////////////////////////
//
// addPort()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addPort (PortDirection dir, uint32 offset, const PortInfo *port, uint16 id, PortWrapper *wrapper)
{
    InterfaceEntry entry;
    entry.offset = offset;
    entry.id = id;
    entry.flags = 0;
    entry.direction = dir;
    entry.name = NULL;
    entry.portInfo = port;
    addEntry(entry);
    if (wrapper)
        wrapper->arrayInternal = entry.arrayInternal;

    // Update list of clocks
    if (dir == PORT_CLOCK)
        addClock(entry.offset);

    // Reset ports should only ever be declared with the Reset() macro that
    // supplies the reset port name (so that the name can be matched to the
    // Verilog reset port name).
    if (dir == PORT_RESET)
        CascadeValidate(entry.name, "Failed to obtain name for reset port");
}

////////////////////////////////////////////////////////////////////////
//
// addPortArray()
//
////////////////////////////////////////////////////////////////////////
bool InterfaceDescriptor::addPortArray (PortDirection dir, uint32 offset, const PortInfo *port, int stride)
{
    CascadeValidate((unsigned) stride < 31, "Port array stride out of bounds");
    InterfaceEntry entry;
    entry.offset = offset;
    entry.id = -1;
    entry.flags = 0;
    entry.isArray = 1;
    entry.direction = dir;
    entry.name = NULL;
    entry.portInfo = port;
    entry.stride = stride;
    bool ret = (m_entryNameIndex < m_entryNames.size()) && (entry.offset == m_entryNames[m_entryNameIndex].offset);
    addEntry(entry);
    m_containsArrayWithClock |= (dir == PORT_CLOCK);
    return ret;
}

////////////////////////////////////////////////////////////////////////
//
// addInterface()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addInterface (uint32 offset, bool isBase, InterfaceDescriptor *descriptor)
{
    int i;
    InterfaceEntry entry;
    entry.offset = offset;
    entry.id = -1;
    entry.flags = 0;
    entry.isInterface = 1;
    entry.isBase = isBase ? 1 : 0;
    int size = (m_validateIndex >= 0) ? m_validateIndex : m_entries.size();
    for (i = size - 1 ; i >= 0 ; i--)
    {
        if ((m_entries[i].isInterface) && (m_entries[i].descriptor->m_name == descriptor->m_name))
        {
            if (m_entries[i].id == -1)
                m_entries[i].id = 0;
            entry.id = m_entries[i].id + 1;
            break;
        }
    }
    entry.name = NULL;
    entry.descriptor = descriptor;
    addEntry(entry);
    m_containsArray |= descriptor->m_containsArray;
    m_containsArrayWithClock |= descriptor->m_containsArrayWithClock;
    if (descriptor->m_depth >= m_depth)
        m_depth = descriptor->m_depth + 1;

    // Update list of clocks
    for (i = 0 ; i < descriptor->m_clockOffsets.size() ; i++)
        addClock(descriptor->m_clockOffsets[i] + entry.offset);
}

////////////////////////////////////////////////////////////////////////
//
// addInterfaceArray()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addInterfaceArray (uint32 offset, InterfaceDescriptor *descriptor, const char *arrayName)
{
    InterfaceEntry entry;
    entry.offset = offset;
    entry.id = -1;
    entry.flags = 0;
    entry.isInterface = 1;
    entry.isArray = 1;
    entry.name = arrayName ? arrayName : descriptor->getName();
    entry.descriptor = descriptor;
    addEntry(entry);
    if (descriptor->m_depth >= m_depth)
        m_depth = descriptor->m_depth + 1;
    if (descriptor->m_clockOffsets.size())
        m_containsArrayWithClock = true;
}

/////////////////////////////////////////////////////////////////
//
// addClock()
//
/////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addClock (uint32 offset)
{
    if (m_validateClockIndex == -1)
        m_clockOffsets.push(offset);
    else
    {
        CascadeValidate(m_clockOffsets[m_validateClockIndex] == offset, "Clock offset validation error");
        m_validateClockIndex++;
    }
}

////////////////////////////////////////////////////////////////////////
//
// endInterface()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::endInterface (const void *_interface)
{
    if (m_validateIndex == -1)
        m_vtable = *(const void **)_interface;
    else
        CascadeValidate(*(const void **)_interface == m_vtable, "Interface vtable validation error");

    m_validateIndex = 0;
    m_validateClockIndex = 0;
}

////////////////////////////////////////////////////////////////////////
//
// reset()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::reset (void *_interface, int level)
{
    // Reset all interfaces and FIFOs
    for (int i = 0 ; i < m_entries.size() ; i++)
    {
        const InterfaceEntry &entry = m_entries[i];
        if (entry.isInterface)
        {
            void *address = ((byte*)_interface) + entry.offset;
            if (entry.isArray)
            {
                GenericArray &array = *(GenericArray *)address;
                for (int i = 0 ; i < array.size() ; i++)
                    entry.descriptor->reset(&array[i], level);
            }
            else
                entry.descriptor->reset(address, level);
        }
        else if (entry.direction == PORT_INFIFO || entry.direction == PORT_OUTFIFO)
        {
            void *address = ((byte*)_interface) + entry.offset;
            if (entry.isArray)
            {
                GenericPortArray &array = *(GenericPortArray *)address;
                byte *base = (byte *) &array[0];
                for (int i = 0 ; i < array.size() ; i++)
                    ((FifoPort<byte> *) (base + i * entry.stride))->fifo->reset();
            }
            else
                ((FifoPort<byte> *) address)->fifo->reset();
        }
    }

    // Reset the current component/interface
    if (m_vtable)
    {
        const void *temp = *(const void**)_interface;
        *(const void**)_interface = m_vtable;
        ((InterfaceBase*)_interface)->reset();
        ((InterfaceBase*)_interface)->reset(level);
        *(const void**)_interface = temp;
    }
}

////////////////////////////////////////////////////////////////////////
//
// addEntry()
//
////////////////////////////////////////////////////////////////////////
void InterfaceDescriptor::addEntry (InterfaceEntry &entry, bool checkNames)
{
    // Set entry name
    if (checkNames && (m_entryNameIndex < m_entryNames.size()))
    {
        CascadeValidate(entry.offset <= m_entryNames[m_entryNameIndex].offset,
            "No matching port for port name at offset %d", m_entries[m_entryNameIndex].offset);
        if (entry.offset == m_entryNames[m_entryNameIndex].offset)
        {
            entry.arrayInternal = m_entryNames[m_entryNameIndex].arrayInternal;
            entry.name = m_entryNames[m_entryNameIndex++].name;
            entry.id = -1;
        }
    }

    if (entry.isArray)
        m_containsArray = true;

    if (m_validateIndex == -1)
        m_entries.push(entry);
    else
    {
        const InterfaceEntry &existing = m_entries[m_validateIndex];
        entry.arrayInternal = existing.arrayInternal;
        if (entry.isInterface && (existing.id == 0))
        {
            CascadeValidate(entry.id == -1, "Validating first occurrence of interface but id is not -1");
            entry.id = 0;
        }
        CascadeValidate(!memcmp(&entry, &existing, sizeof(InterfaceEntry)), "addEntry validation failure");
        m_validateIndex++;
    }
}

BEGIN_COVERAGE("PortIterator");

////////////////////////////////////////////////////////////////////////
//
// PortIterator::PortIterator()
//
////////////////////////////////////////////////////////////////////////
PortIterator::PortIterator (const PortSet &ports) :
m_type(ports.type)
{
    coverLine();
    initialize(ports.descriptor, ports._interface);
}

////////////////////////////////////////////////////////////////////////
//
// PortIterator::PortIterator()
//
////////////////////////////////////////////////////////////////////////
PortIterator::PortIterator (PortSet::PortSetType type, const Component *component) :
m_type(type)
{
    coverRange(type, 0, PortSet::Everything);
    initialize(component->getInterfaceDescriptor(), component);
}

////////////////////////////////////////////////////////////////////////
//
// PortIterator::PortIterator()
//
////////////////////////////////////////////////////////////////////////
PortIterator::PortIterator (PortSet::PortSetType type, const InterfaceDescriptor *descriptor, const void *_interface) :
m_type(type)
{
    coverRange(type, 0, PortSet::Everything);
    initialize(descriptor, _interface);
}

////////////////////////////////////////////////////////////////////////
//
// PortIterator::~PortIterator()
//
////////////////////////////////////////////////////////////////////////
PortIterator::~PortIterator ()
{
    coverLine();
    delete[] m_stack;
}

/////////////////////////////////////////////////////////////////
//
// PortIterator::arraySize()
//
/////////////////////////////////////////////////////////////////
int PortIterator::arraySize () const
{
    if (!m_state->entry->isArray)
        return 0;
    const byte *parray = BADDR(m_state->_interface) + m_state->entry->offset;
    if (m_state->entry->isInterface)
        return ((const GenericArray *) parray)->size();
    return ((const GenericPortArray *) parray)->size();
}

////////////////////////////////////////////////////////////////////////
//
// PortIterator::initialize()
//
////////////////////////////////////////////////////////////////////////
void PortIterator::initialize (const InterfaceDescriptor *descriptor, const void *_interface)
{
    coverBool(descriptor->size() == 0);
    if (descriptor->size())
    {
        m_valid = true;
        m_state = m_stack = new State[descriptor->depth()];
        m_state->_interface = _interface;
        m_state->entry = descriptor->getEntry(0);
        m_state->numEntries = descriptor->size();
        m_state->entryIndex = 0;
        m_state->arrayIndex = 0;

        // Skip any initial size-0 arrays
        while ((m_state->entryIndex < m_state->numEntries) && m_state->entry->isArray && !arraySize())
        {
            coverLine();
            m_state->entry++;
            m_state->entryIndex++;
        }

        // We're done if there are no more entries
        if (m_state->entryIndex == m_state->numEntries)
        {
            coverLine();
            m_valid = false;
            m_stack = NULL;
            return;
        }

        // The increment operator assumes that if it gets called from within
        // an array of ports, then it can just move to the next entry in the
        // array.  In this case, if array is not a match, we must force
        // the increment operator to move to the next entry by setting the
        // array size to 0.
        if (m_state->entry->isInterface || PortSet::match(m_type, m_state->entry->direction))
        {
            coverLine();
            m_state->arraySize = arraySize();
        }
        else
        {
            coverLine();
            m_state->arraySize = 0;
        }

        // If the first entry is not a match then call the increment operator
        bool doInc = m_state->entry->isInterface || !PortSet::match(m_type, m_state->entry->direction);
        coverBool(doInc);
        if (doInc)
            (*this)++;
    }
    else
    {
        m_valid = false;
        m_stack = NULL;
    }
}

////////////////////////////////////////////////////////////////////////
//
// PortIterator::operator++()
//
////////////////////////////////////////////////////////////////////////
void PortIterator::operator++ (int)
{
    // We generally want to immediately move past the current entry.  The only
    // exception is when the current entry is an interface, which can only 
    // happen when this is called from initialize().
    if (!m_state->entry->isInterface)
    {
        // If we're inside an array of ports then they all must match, so just
        // move to the next array element.
        if (++m_state->arrayIndex < m_state->arraySize)
        {
            coverLine();
            return;
        }

        // Not an array, or reached the end of the array - advance to the next element
        advance();
    }

    while (1)
    {
        // Check to see if we've reached the end of the current descriptor
        if (m_state->entryIndex < m_state->numEntries)
        {
            // We're done if there's a match
            if (!m_state->entry->isInterface && PortSet::match(m_type, m_state->entry->direction))
                return;

            // If this is a non-empty sub-interface then step into it and put it
            // on the stack, otherwise advance to the next entry.
            if (m_state->entry->isInterface && m_state->entry->descriptor->size())
            {
                // Create the new stack entry
                m_state[1]._interface = BADDR(m_state->_interface) + m_state->entry->offset;
                coverBool(m_state->entry->isArray);
                if (m_state->entry->isArray)
                {
                    const GenericArray &array = *((const GenericArray *) m_state[1]._interface);
                    m_state[1]._interface = &array[m_state->arrayIndex];
                }
                m_state[1].entry = m_state->entry->descriptor->getEntry(0) - 1;
                m_state[1].entryIndex = (unsigned) -1;
                m_state[1].numEntries = m_state->entry->descriptor->size();
                m_state++; 
                advance();
            }
            else
                advance();
        }
        else if (m_state > m_stack)
        {
            // We've reached the end of the current descriptor - pop up the stack
            m_state--;
            bool doAdvance = (++m_state->arrayIndex >= m_state->arraySize);
            coverBool(doAdvance);
            if (doAdvance)
                advance();
        }
        else
        {
            // We've reached the end of the top-most descriptor - game over
            m_valid = false;
            return;
        }
    }
}

void PortIterator::advance ()
{
    while (1)
    {
        m_state->entryIndex++;
        coverBool(m_state->entryIndex < m_state->numEntries);
        if (m_state->entryIndex < m_state->numEntries)
        {
            m_state->entry++;
            coverBool(m_state->entry->isArray);
            m_state->arraySize = arraySize();
            if (m_state->entry->isArray)
            {
                coverBool(m_state->arraySize == 0);
                if (!m_state->arraySize)
                    continue;
            }
            m_state->arrayIndex = 0;
        }
        break;
    }
}

void *PortIterator::arrayAddress (void *array, unsigned index) const
{
    coverBool(index == 0);
    byte *base = (byte *) &(*((GenericPortArray *) array))[0];
    return base + (m_state->entry->stride * index);
}

/////////////////////////////////////////////////////////////////
//
// PortIterator::formatName()
//
/////////////////////////////////////////////////////////////////
void PortIterator::formatName (strbuff &s) const
{
    // Containing interfaces
    int i;
    for (i = 0 ; i < depth() - 1 ; i++)
    {
        const InterfaceEntry *entry = m_stack[i].entry;
        if (entry->isArray)
            s.puts(entry->name);
        else
            s.puts(entry->descriptor->getName());
        if (entry->isArray)
            ((const GenericArray *) address(i))->formatChildId(s, m_stack[i].arrayIndex);
        else if (entry->id != -1)
            s.append("%d", entry->id);
        s.putch('.');
    }

    // Port
    const InterfaceEntry *entry = m_state->entry;
    if (entry->name)
        s.puts(entry->name);
    else
        s.puts(PortName::portName[entry->direction]);
    if (entry->isArray)
        s.append("[%d]", m_state->arrayIndex);
    else if (entry->id != -1)
        s.append("%d", entry->id);
}

/////////////////////////////////////////////////////////////////
//
// ClockIterator::ClockIterator()
//
/////////////////////////////////////////////////////////////////
ClockIterator::ClockIterator (const Component *component)
{
    InterfaceDescriptor *descriptor = component->getInterfaceDescriptor();
    m_fastIterator = !descriptor->containsArrayWithClock();
    coverBool(m_fastIterator);
    if (m_fastIterator)
    {
        m_clockIndex = descriptor->numClocks() ? 0 : -1;
        m_component = component;
    }
    else
    {
        m_pit = new PortIterator(PortSet::Clocks, descriptor, component);
    }
}

/////////////////////////////////////////////////////////////////
//
// ClockIterator::~ClockIterator()
//
/////////////////////////////////////////////////////////////////
ClockIterator::~ClockIterator ()
{
    coverBool(m_fastIterator);
    if (!m_fastIterator)
        delete m_pit;
}

/////////////////////////////////////////////////////////////////
//
// ClockIterator::operator++()
//
/////////////////////////////////////////////////////////////////
void ClockIterator::operator++ (int)
{
    coverBool(m_fastIterator);
    if (m_fastIterator)
    {
        m_clockIndex++;
        if (m_clockIndex == m_component->getInterfaceDescriptor()->numClocks())
            m_clockIndex = -1;
    }
    else
        (*m_pit)++;
}

/////////////////////////////////////////////////////////////////
//
// ClockIterator::operator*()
//
/////////////////////////////////////////////////////////////////
Clock *ClockIterator::operator* ()
{
    coverBool(m_fastIterator);
    if (m_fastIterator)
        return m_component->getInterfaceDescriptor()->getClock(m_component, m_clockIndex);
    else
        return (Clock*) m_pit->address();
}

END_COVERAGE();

////////////////////////////////////////////////////////////////////////
//
// connect
//
////////////////////////////////////////////////////////////////////////
static void connect (const PortSet &ports1, const PortSet &ports2, int delay)
{
    PortIterator it1(ports1);
    PortIterator it2(ports2);

    for ( ; it1 || it2 ; it1++, it2++)
    {
        const char *error = NULL;
        if (!it1 || !it2)
            error = "Unmatched port";
        else if (it1.entry()->portInfo != it2.entry()->portInfo)
            error = "Port type mismatch";
        else
            it1.wrapper()->connect(it2.wrapper(), delay);

        assert_always(!error,
            "Error connecting %s in interface %s\n    to %s in interface %s:\n        %s",
            !it1 ? "<unmatched>" : it1.entry()->name ? it1.entry()->name : *it1.entry()->portInfo->typeName, 
            ports1.descriptor->getClassName(),
            !it2 ? "<unmatched>" : it2.entry()->name ? it2.entry()->name : *it2.entry()->portInfo->typeName, 
            ports2.descriptor->getClassName(),
            error);        
    }
}

////////////////////////////////////////////////////////////////////////
//
// connectPorts
//
////////////////////////////////////////////////////////////////////////
void connectPorts (const PortSet &ports1, const PortSet &ports2, bool chain, int delay)
{
    const char *error = NULL;

    if (chain && (ports1.type != ports2.type))
        error = "Only ports of the same type can be chained";
    else if (!chain && (ports1.type == ports2.type))
        error = "Only ports of opposite type can be connected";
    else if (!ports1._interface || !ports2._interface)
        error = "Attempted to connect NULL interface";

    assert_always(!error,
        "Error connecting interface %s to interface %s:\n"
        "        %s",
        ports1.descriptor->getClassName(),
        ports2.descriptor->getClassName(),
        error);                

    if (ports1.type == PortSet::Inputs)
        connect(ports1, ports2, delay);
    else
        connect(ports2, ports1, delay);
}

/////////////////////////////////////////////////////////////////
//
// getInterfaceName()
//
/////////////////////////////////////////////////////////////////
strbuff InterfaceName::getInterfaceName (const void *address, const Component *c, const InterfaceDescriptor *type)
{
    strbuff s;
    stack<SearchState> searchStack;
    const void *_interface = c;
    const InterfaceDescriptor *descriptor = c->getInterfaceDescriptor();
    CascadeValidate(formatInterfaceName(s, address, type, _interface, descriptor, searchStack),
        "Could not find interface at address %p\n"
        "    (This can be caused by an invalid interface pointer cast)", address);

    // Found it!
    c->formatName(s);

    // Stack of interfaces
    for (int i = searchStack.size() - 1 ; i >= 0 ; i--)
    {
        const InterfaceEntry *entry = descriptor->getEntry(searchStack[i].entryIndex);

        // Get the new descriptor and interface address
        descriptor = entry->descriptor;
        _interface = BADDR(_interface) + entry->offset;

        // Add the descriptor or array name
        if (entry->isArray)
            s.puts(entry->name);
        else
            s.puts(descriptor->getName());

        // Format the entry id and fix the interface address if it's an array
        if (entry->isArray)
        {
            const GenericArray &array = *((const GenericArray *) _interface);
            array.formatChildId(s, searchStack[i].arrayIndex);
            _interface = &array[searchStack[i].arrayIndex];
        }
        else if (entry->id != -1)
            s.append("%d", entry->id);

        // Separator
        if (i)
            s.putch('.');
    }

    return s;
}

bool InterfaceName::formatInterfaceName (strbuff &s, const void *address, 
                                         const InterfaceDescriptor *type, const void *_interface, 
                                         const InterfaceDescriptor *descriptor, stack<SearchState> &searchStack)
{
    if ((address == _interface) && (type == descriptor))
        return true;

    // See if we can eliminate this interface based on address
    if (descriptor->containsArray() || (BDIFF(address, _interface) < descriptor->maxOffset()))
    {
        // Iterate over the sub-interfaces
        for (int i = 0 ; i < descriptor->size() ; i++)
        {
            const InterfaceEntry *entry = descriptor->getEntry(i);
            if (entry->isInterface)
            {
                if (entry->isArray)
                {
                    const GenericArray &array = *((const GenericArray *) (BADDR(_interface) + entry->offset));
                    for (int j = 0 ; j < array.size() ; j++)
                    {
                        if (formatInterfaceName(s, address, type, &array[j], entry->descriptor, searchStack))
                        {
                            searchStack.push(SearchState(i, j));
                            return true;
                        }
                    }
                }
                else if (formatInterfaceName(s, address, type, BADDR(_interface) + entry->offset, entry->descriptor, searchStack))
                {
                    searchStack.push(SearchState(i, 0));
                    return true;
                }
            }
        }
    }

    return false;
}

END_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////
//
// Interface()
//
////////////////////////////////////////////////////////////////////////
Interface::Interface ()
{
    Cascade::Hierarchy::setInterface(this);
}
