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
// Hierarchy.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include "Update.hpp"

BEGIN_NAMESPACE_CASCADE

static const char *frameType[2] = {"Component", "Interface"};

////////////////////////////////////////////////////////////////////////
//
// Construction frame allocation/deallocation
//
////////////////////////////////////////////////////////////////////////
ConstructionFrame *ConstructionFrame::alloc ()
{
    ConstructionFrame *frame;
    if (s_frames.size())
    {
        frame = s_frames.back();
        s_frames.pop();
    }
    else
        frame = new ConstructionFrame;
    frame->beginConstruction();
    return frame;
}

void ConstructionFrame::free (ConstructionFrame *frame)
{
    if (!descore::g_error)
        frame->endConstruction();
    s_frames.push(frame);
}

void ConstructionFrame::cleanupFrames ()
{
    for (int i = 0 ; i < s_frames.size() ; i++)
        delete s_frames[i];
    s_frames.clear();
}

// Static stack of reuseable construction frames
stack<ConstructionFrame *> ConstructionFrame::s_frames;

// Helper class to make sure the construction frames get cleaned up if
// the simulation doesn't exit gracefully.
static struct CleanupConstructionFrames
{
    ~CleanupConstructionFrames () { ConstructionFrame::cleanupFrames(); }
} s_cleanupConstructionFrames;

////////////////////////////////////////////////////////////////////////
//
// Static variables
//
////////////////////////////////////////////////////////////////////////

// When the frame stack is non-empty, it always consists of one or more
// component frames followed by zero or more interface frames.  currFrame
// is the top-most frame; currComponent is the top-most component frame.
ConstructionFrame *Hierarchy::currFrame = NULL;
ConstructionFrame *Hierarchy::currComponent = NULL;

////////////////////////////////////////////////////////////////////////
//
// beginConstruction()
//
////////////////////////////////////////////////////////////////////////
void ConstructionFrame::beginConstruction ()
{
    parent = NULL;
    _interface = NULL;
    memset(portID, 0, sizeof(portID));
}

////////////////////////////////////////////////////////////////////////
//
// endConstruction()
//
////////////////////////////////////////////////////////////////////////
void ConstructionFrame::endConstruction ()
{
    CascadeValidate(_interface, "Deleting construction frame that has no component or interface");

    // Make sure the interface descriptor gets finalized.
    // Don't call begin/end interface for an array frame; we'll call it from
    // the child frame corresponding to the actual component/interface.
    if (!array)
        descriptor->endInterface(_interface);

    // If this is a base component/interface or a member interface, then add it 
    // to the parent's interface descriptor.  Don't need to do this if there is 
    // no parent, or if this is an array (because Hierarchy::addInterfaceArray 
    // takes care of adding the descriptor entry), or the parent is an array 
    // (again, because the array would have taken care of adding descriptor entries 
    // to its parent).
    if (parent && !array && !parent->array && (isBase || (type == Hierarchy::INTERFACE)))
    {
        Hierarchy::validateAddress(parent, _interface, descriptor->getClassName());
        parent->descriptor->addInterface(BDIFF(_interface, parent->_interface), isBase, descriptor);
    }

    // For interfaces, set the parent component; for components this function validates
    // the component pointer and makes sure that Component is the first base class.
    // Don't call this for arrays.
    if (!array)
        _interface->setParentComponent((Component *) Hierarchy::currComponent->_interface);

    // Do some finalization if this was a most-derived non-array component
    if (type == Hierarchy::COMPONENT && !isBase && !array)
    {
        Component *component = (Component *) _interface;

        // Set the ID of the component by counting the number
        // of sibling components with the same class name
        Component *sibling = component->parentComponent ? component->parentComponent->childComponent : Sim::topLevelComponents;
        int16 id = -1;
        const char *name = component->getComponentName();
        for ( ; sibling && (sibling != component) ; sibling = sibling->nextComponent)
        {
            const char *sname = sibling->getComponentName();
            if ((!name && !sname) || (name && sname && !strcmp(name, sibling->getComponentName())))
            {
                if (id == -1)
                {
                    sibling->setComponentId(0);
                    id = 0;
                }
                id++;
            }
        }
        // Always set the id for the first element of an array to ensure that 
        // the indices get printed.
        if (id == -1 && parent && parent->array)
            id = 0;
        component->setComponentId(id);
        Sim::updateChecksum(name, id);
        UpdateFunctions::endComponent();
    }
}

////////////////////////////////////////////////////////////////////////
//
// beginConstruction()
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::beginConstruction (Type type, InterfaceDescriptor *descriptor, bool array)
{
    assert((Sim::state != Sim::SimInitialized) && (Sim::state != Sim::SimInitializing), 
        "You cannot construct new %s once the simulation has been initialized",
        type == COMPONENT ? "components" : "interfaces");
    Sim::state = Sim::SimConstruct;

    ConstructionFrame *frame = ConstructionFrame::alloc();
    frame->parent = currFrame;
    currFrame = frame;
    currFrame->type = type;
    currFrame->array = array;
    currFrame->descriptor = descriptor;

    // Determine if this is a base component/interface. Skip this step if there 
    // is no parent (which means that this must be a non-base component), or if 
    // this is an array (because nothing should derive from an array), or the 
    // parent is an array (because an array does not derive from anything).
    frame->isBase = false;
    if (frame->parent && !array && !frame->parent->array)
    {
        // This is a base component/interface if the parent's interface base not yet been set,
        // or if the parent's descriptor does not yet match the return value of 
        // interface->getInterfaceDescriptor().
        if (!frame->parent->_interface)
            frame->isBase = true;
        else
            frame->isBase = (frame->parent->_interface->getInterfaceDescriptor() != frame->parent->descriptor);
    }

    if (type == COMPONENT)
    {
        assert_always(currComponent == currFrame->parent, 
            "It is illegal for interface %s to %s component %s",
            currFrame->parent->descriptor->getClassName(), frame->isBase ? "inherit from" : "contain",
            descriptor->getClassName());
        currComponent = currFrame;
    }
    else if (type == INTERFACE)
    {
        assert_always(currComponent, "An interface must be contained within or inherited by a component");
        assert_always(currComponent->_interface, 
            "Component %s must inherit from Component (or another component class)\n"
            "    before inheriting from %s",
            currComponent->descriptor->getClassName(), descriptor->getClassName());
    }
}

////////////////////////////////////////////////////////////////////////
//
// endConstruction()
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::endConstruction ()
{
    CascadeValidate(currFrame, "endConstruction() called without construction frame");

    ConstructionFrame *frame = currFrame;
    currFrame = currFrame->parent;
    ConstructionFrame::free(frame);

    if (!descore::g_error)
    {
        if (frame == currComponent)
            currComponent = currFrame;
    }
    else if (!currFrame)
    {
        Sim::state = Sim::SimNone;
        currComponent = NULL;
    }
}

////////////////////////////////////////////////////////////////////////
//
// setComponent()
//
////////////////////////////////////////////////////////////////////////
Component *Hierarchy::setComponent (Component *component)
{
    assert_always(currFrame && currComponent && !currFrame->_interface, 
        "Constructing Component base class but no component is registered.\n"
        "    Did you forget COMPONENT_CTOR?");

    // Validation
    if (!currFrame->array)
    {
        assert_always(currComponent == currFrame,
            "setComponent() called for the interface %s.\n"
            "    Did you inherit from Component instead of Interface by mistake?",
            currFrame->descriptor->getClassName());
    }

    // Set the interface pointer, walking up the stack to make sure that we set the
    // pointer for all derived components.
    ConstructionFrame *frame = currFrame;
    for ( ; frame && !frame->_interface ; frame = frame->parent)
    {
        frame->_interface = component;

        // Don't call begin/end interface for an array frame; we'll call it from
        // the child frame corresponding to the actual component/interface
        if (!frame->array)
            frame->descriptor->beginInterface(component);
    }

    // Keep walking up the stack to find the parent component
    for ( ; frame && frame->isBase ; frame = frame->parent);
    return frame ? (Component *) frame->_interface : NULL;
}

////////////////////////////////////////////////////////////////////////
//
// getComponent()
//
////////////////////////////////////////////////////////////////////////
Component *Hierarchy::getComponent ()
{
    assert_always(currComponent && currComponent->_interface,
        "Ports must be contained within components or their interfaces\n");
    return (Component *) currComponent->_interface;
}

////////////////////////////////////////////////////////////////////////
//
// setInterface()
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::setInterface (InterfaceBase *_interface)
{
    assert_always(currFrame && !currFrame->_interface,
        "Constructing Interface base class but no interface is registered.\n"
        "    Did you forget DECLARE_INTERFACE or INTERFACE_CTOR?");
    assert_always(currComponent != currFrame,
        "setInterface() called for the component %s.\n"
        "    Did you inherit from Interface instead of Component by mistake?",
        currFrame->descriptor->getClassName());

    // Set the interface pointer, walking up the stack to make sure that we set the
    // pointer for all derived components/interfaces.
    for (ConstructionFrame *frame = currFrame ; frame && !frame->_interface ; frame = frame->parent)
    {
        frame->_interface = _interface;
        frame->descriptor->beginInterface(_interface);
    }
}

////////////////////////////////////////////////////////////////////////
//
// addPort() 
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::addPort (PortDirection dir, const void *address, const PortInfo *port, PortWrapper *wrapper)
{
    assert_always(currFrame, "Ports must be contained within components or their interfaces");
    if (currFrame->array)
        return;

    assert_always(currFrame->_interface,
        "%s base class was never constructed.\n"
        "    Did you forget to inherit from %s?",
        currFrame->descriptor->getClassName(), frameType[currFrame->type]);
    validateAddress(currFrame, address, PortName::portName[dir]);
    currFrame->descriptor->addPort(dir, BDIFF(address, currFrame->_interface), port, currFrame->portID[dir]++, wrapper);
}

////////////////////////////////////////////////////////////////////////
//
// addPortArray() 
//
////////////////////////////////////////////////////////////////////////
bool Hierarchy::addPortArray (PortDirection dir, const void *address, const PortInfo *port, int stride)
{
    assert_always(currFrame, "Ports must be contained within components or their interfaces");
    assert_always(currFrame->_interface,
        "%s base class was never constructed.\n"
        "    Did you forget to inherit from %s?",
        currFrame->descriptor->getClassName(), frameType[currFrame->type]);
    validateAddress(currFrame, address, "PortArray");
    return currFrame->descriptor->addPortArray(dir, BDIFF(address, currFrame->_interface), port, stride);
}

////////////////////////////////////////////////////////////////////////
//
// addInterfaceArray()
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::addInterfaceArray (const void *address, InterfaceDescriptor *_interface, const char *arrayName)
{
    CascadeValidate(Hierarchy::currFrame, "addInterfaceArray called but there is no construction frame");
    ConstructionFrame *parent = Hierarchy::currFrame->parent;
    assert_always(parent, "An array of interfaces must be contained within a component or interface");
    CascadeValidate(parent->_interface, "Construction frame is array but parent descriptor or interface is invalid");
    validateAddress(parent, address, "Array");
    currFrame->parent->descriptor->addInterfaceArray(BDIFF(address, currFrame->parent->_interface), _interface, arrayName);
}

////////////////////////////////////////////////////////////////////////
//
// validateAddress()
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::validateAddress (const ConstructionFrame *frame, const void *address, const char *type)
{
    uint32 offset = (uint32) (BDIFF(address, frame->_interface));
    assert_always(offset < frame->descriptor->maxOffset(), 
        "Dynamically allocated ports and interfaces are not allowed\n"
        "    (%s allocated a %s).  Use Array<> instead.",
        frame->descriptor->getClassName(), type);
}

////////////////////////////////////////////////////////////////////////
//
// dumpConstructionStack()
//
////////////////////////////////////////////////////////////////////////
void Hierarchy::dumpConstructionStack (descore::runtime_error &error)
{
    if (!currFrame)
        return;
    error.append("    Construction stack:\n");

    for (ConstructionFrame *frame = currFrame ; frame ; frame = frame->parent)
    {
        if (frame->array)
            error.append("        %s Array\n", frameType[frame->type]);
        else
            error.append("        %s: %s\n", frameType[frame->type], frame->descriptor->getClassName());
    }
}

END_NAMESPACE_CASCADE

