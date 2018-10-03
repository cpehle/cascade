/*
Copyright 2010, D. E. Shaw Research.
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

////////////////////////////////////////////////////////////////////////////////
//
// Waves.cpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 02/18/2010
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include <descore/Wildcard.hpp>
#include <descore/Iterators.hpp>
#include <descore/MapIterators.hpp>

BEGIN_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////////////
//
// State
//
////////////////////////////////////////////////////////////////////////////////

struct WavesDumpSpecifier
{
    // If pcomponent is NULL, then use the string to search for matches
    const Component *pcomponent;
    string component;

    string signals;
    int level;
};

static WavesComponent g_top;
static std::vector<WavesDumpSpecifier> g_dumpSpecifiers;
static WavesFile *g_file = NULL;
static bool g_dumping = false;
static WavesComponent *g_currComponent = NULL;

////////////////////////////////////////////////////////////////////////////////
//
// Functions
//
////////////////////////////////////////////////////////////////////////////////

// Walk the component hierearchy to decide which components to dump
static void initComponents (const Component *c, const WavesDumpSpecifier *dump, int dumpLevelCount = 0);

// Create the wave file and write the signal index
static void initWavesFile ();

////////////////////////////////////////////////////////////////////////////////
//
// Waves::dumpSignals()
//
////////////////////////////////////////////////////////////////////////////////
void Waves::dumpSignals (const char *wcComponent, const char *wcSignals, int level)
{
    assert_always(Sim::state <= Sim::SimConstruct, "Signals to dump can only be declared during construction");
    WavesDumpSpecifier w = { NULL, wcComponent, wcSignals, level };
    g_dumpSpecifiers.push_back(w);
}

void Waves::dumpSignals (const Component *component, const char *wcSignals, int level)
{
    assert_always(Sim::state <= Sim::SimConstruct, "Signals to dump can only be declared during construction");
    WavesDumpSpecifier w = { component, "", wcSignals, level };
    g_dumpSpecifiers.push_back(w);
}

////////////////////////////////////////////////////////////////////////////////
//
// Waves::archive()
//
////////////////////////////////////////////////////////////////////////////////
void Waves::archive ()
{
    if (!g_dumping)
        return;

    g_top.doAcross(&IWavesFunctions::archive);

    delete g_file;
    initWavesFile();
}

////////////////////////////////////////////////////////////////////////////////
//
// Waves::initialize()
//
////////////////////////////////////////////////////////////////////////////////
void Waves::initialize ()
{
    // Make sure we're in a clean state
    cleanup();

    // Process the dump specifiers to generate the components and waves
    for (int i = 0 ; i < (int) g_dumpSpecifiers.size() ; i++)
    {
        if (g_dumpSpecifiers[i].pcomponent)
            g_dumpSpecifiers[i].component = *g_dumpSpecifiers[i].pcomponent->getName();
        else
            g_dumpSpecifiers[i].pcomponent = Sim::topLevelComponents;
        initComponents(g_dumpSpecifiers[i].pcomponent, &g_dumpSpecifiers[i]);
    }
    g_dumpSpecifiers.clear();

    // Return if no signals were registered for dumping
    if (!g_dumping)
        return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Waves::resolveSignals()
//
////////////////////////////////////////////////////////////////////////////////
void Waves::resolveSignals ()
{
    if (!g_dumping)
        return;

    g_top.doAcross(&IWavesFunctions::resolve);

    // Initialize the wave file
    initWavesFile();
}

////////////////////////////////////////////////////////////////////////////////
//
// Waves::cleanup()
//
////////////////////////////////////////////////////////////////////////////////
void Waves::cleanup ()
{
    delete g_file;
    g_file = NULL;
    g_top.cleanup();
    g_dumping = false;
}

////////////////////////////////////////////////////////////////////////////////
//
// initWaves()
//
////////////////////////////////////////////////////////////////////////////////
static strbuff componentName (const Component *c)
{
    strbuff s = c->getName();
    return (s == "") ? "Top" : s;
}

void initComponents (const Component *c, const WavesDumpSpecifier *dump, int dumpLevelCount)
{
    for ( ; c ; c = c->nextComponent)
    {
        bool match = wildcardMatch(*dump->component, componentName(c));
        if (dumpLevelCount || match)
            Waves::initSignals(c, *dump->signals);
        int levelCount = match ? dump->level - 1 : dumpLevelCount ? dumpLevelCount - 1 : 0;
        initComponents(c->childComponent, dump, levelCount);
    }
}

void Waves::initSignals (const Component *c, const char *signals)
{
    WavesComponent *wc = NULL;

    // Add signals for this component
    PortIterator it (PortSet::Everything, c); 
    for (int index = 0 ; it ; it++, index += 4)
    {
        strbuff name = it.getName();
        if (wildcardMatch(name, signals))
        {
            if (!wc)
                wc = getWavesComponent(c);

            string sname = *name;
            if (wc->signals.find(sname) == wc->signals.end())
            {
                const InterfaceEntry *entry = it.entry();
                WavesSignal *s = NULL;

                if (entry->direction == PORT_CLOCK)
                {
                    Clock *clk = (Clock *) it.address();
                    ClockDomain *d = clk->resolveClockDomain();
                    s = new WavesSignal(WavesSignal::CLOCK, &d->m_numEdges, entry->portInfo, index);
                    d->addWavesClock(s);
                }
                else if (entry->direction == PORT_RESET)
                {
                    s = new WavesSignal(WavesSignal::RESET, it.address(), entry->portInfo, index);
                    ClockDomain::addGlobalWavesSignal(s);
                }
                else if (entry->direction == PORT_INFIFO || entry->direction == PORT_OUTFIFO)
                {
                    FifoPort<byte> *p = (FifoPort<byte> *) it.address();
                    WavesFifo *f = new WavesFifo(WavesSignal::FIFO, p, entry->portInfo, index, sname);
                    wc->fifos.push_back(f);

                    // Wait until resolve time to decide whether or not to add the credit signal
                    // because we can't always tell from the current wrapper if the FIFO has
                    // flow control (it might be disabled by an upstream wrapper).
                    wc->signals[sname] = &f->dataSignal;
                    wc->signals[sname + "_valid"] = &f->validSignal;
                }
                else if (entry->direction == PORT_SIGNAL)
                {
                    s = new WavesSignal(WavesSignal::SIGNAL, it.address(), entry->portInfo, index);
                }
                else if (entry->direction <= PORT_REGISTER)
                {
                    Port<byte> *p = (Port<byte> *) it.address();
                    s = new WavesSignal(WavesSignal::PORT, p, entry->portInfo, index);
                }
                if (s)
                    wc->signals[sname] = s;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// getWavesComponent()
//
////////////////////////////////////////////////////////////////////////////////
WavesComponent *Waves::getWavesComponent (const Component *c)
{
    strbuff _name = componentName(c);
    char *copy = new char[strlen(_name) + 1];
    strcpy(copy, _name);
    WavesComponent *wc = &g_top;
    char *name = copy;

    while (name)
    {
        char *children = strchr(name, '.');
        if (children)
            *children++ = 0;

        WavesComponent::ComponentMap::iterator it = wc->children.find(name);
        if (it == wc->children.end())
            wc = wc->children[name] = new WavesComponent(c->getClockDomain(false));
        else
            wc = it->second;
        name = children;
    }

    delete[] copy;
    return wc;
}

////////////////////////////////////////////////////////////////////////////////
//
// initWaveFile()
//
////////////////////////////////////////////////////////////////////////////////
void initWavesFile ()
{
    g_file = new VcdWavesFile;
    g_file->open(params.WavesFilename->c_str());
    g_top.writeIndex();
    g_file->endSignals();
    g_top.doAcross(&IWavesFunctions::dumpInitialValues);
}

////////////////////////////////////////////////////////////////////////////////
//
// WavesSignal
//
////////////////////////////////////////////////////////////////////////////////
WavesSignal::WavesSignal (SignalType type, const void *data, const PortInfo *info, int index)
: next(NULL),
m_info(info),
m_data((const byte *) data),
m_currVal(new byte[(info->sizeInBits + 7)/8]),
m_validValue(0),
m_currValid(0xff),
m_type(type),
m_id(0),
m_index(index)
{
    g_dumping = true;
    if (type == PORT && m_port->wrapper->getTerminalWrapper()->connection == PORT_SYNCHRONOUS)
        m_type = REGQ;
}

WavesSignal::~WavesSignal ()
{
    delete[] m_currVal;
}

void WavesSignal::resolve ()
{
    if (m_type == PORT || m_type == REGQ)
    {
#ifdef _DEBUG
        if (m_port->hasValidFlag)
            m_validValue = m_port->validValue;
#endif
        m_data = m_port->value;

        // Figure out which clock domain this signal belongs to.  Constants
        // don't need to be dumped after the initial dump, so don't add them to
        // a clock domain.
        if (!Constant::isConstant(m_data))
        {
            ClockDomain *c = ClockDomain::findOwner((const byte *) m_data);
            if (!c)
                c = g_currComponent->domain;
            if (!c)
                ClockDomain::addGlobalWavesSignal(this);
            else if (m_type == PORT)
                c->addWavesSignal(this);
            else
                c->addWavesRegQ(this);
        }
    }
    else if (m_type == SIGNAL)
    {
        ClockDomain *c = g_currComponent->domain;
        if (c)
            c->addWavesSignal(this);
        else
            ClockDomain::addGlobalWavesSignal(this);
    }
}

void WavesSignal::dumpInitialValues()
{
    if (m_type < FIFO_PRODUCER || m_type > FIFO_TRIGGER)
        dump();
}

void WavesSignal::archive ()
{
    m_currValid = 0xff;
}

void WavesSignal::writeIndex (string name)
{
    m_id = g_file->addSignal(*name, m_info->sizeInBits);
}

void WavesSignal::dump ()
{
    uint32 buff[CASCADE_MAX_PORT_SIZE / 32];
    assert(g_file);

    // Get the current validValue
    byte currValid = 0;
    if (m_validValue)
        currValid = m_data[-1];

    // Get the current value
    buff[m_info->sizeInBits / 32] = 0;
    m_info->bitmap->mapCtoV(buff, m_data);

    // Has something changed?
    int sizeInBytes = (m_info->sizeInBits + 7) / 8;
    if ((currValid != m_currValid) || memcmp(m_currVal, buff, sizeInBytes))
    {
        m_currValid = currValid;
        if (currValid == m_validValue)
            memcpy(m_currVal, buff, sizeInBytes);
        g_file->valueChange(m_id, m_currVal, m_currValid != m_validValue, m_info->sizeInBits);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// WavesFifo
//
////////////////////////////////////////////////////////////////////////////////
WavesFifo::WavesFifo (WavesSignal::SignalType type, FifoPort<byte> *port, const PortInfo *info, int index, string name)
: dataSignal(type, NULL, info, index),
validSignal(type, &m_valid, getPortInfo<bit>(), index + 1),
creditSignal(type, &m_credit, getPortInfo<bit>(), index + 2),
next(NULL),
m_valid(0),
m_credit(0),
m_head(0),
m_tail(0),
m_freeCount(0),
m_fullCount(0),
m_fifoPort(port),
m_name(name)
{
    if (m_fifoPort->wrapper->connectedTo)
        dataSignal.m_type = WavesSignal::FIFO_CONSUMER;
    else
        dataSignal.m_type = WavesSignal::FIFO_PRODUCER;
}

WavesFifo::~WavesFifo ()
{
    if (dataSignal.m_type == WavesSignal::FIFO_TRIGGER)
        delete m_trigger;
}

static string reversePortDirection (const string &name)
{
    const char *tag[4] = { "i_", "o_", "in_", "out_" };
    for (int i = 0 ; i < 4 ; i++)
    {
        size_t idx = name.find(tag[i]);
        if (idx != string::npos && (!idx || (*name)[idx-1] == '.'))
            return name.substr(0, idx) + tag[i^1] + name.substr(idx + strlen(tag[i]));
    }
    return name;
}

void WavesFifo::resolve ()
{
    m_fifo = m_fifoPort->fifo;

    // See if we need to insert a trigger proxy (if size == 0, or if we're a consumer
    // and the target is a trigger).
    if (!m_fifo->size || (dataSignal.m_type == WavesSignal::FIFO_CONSUMER && (m_fifo->target & TRIGGER_ITRIGGER)))
    {
        WavesFifoTriggerProxy *proxy = new WavesFifoTriggerProxy(m_fifo);
        dataSignal.m_data = proxy->m_data;
        if (dataSignal.m_type == WavesSignal::FIFO_PRODUCER && m_fifo->producerClockDomain)
            m_fifo->producerClockDomain->addWavesFifo(this);
        else
            m_fifo->consumerClockDomain->addWavesFifo(this);

        // Overwrite m_type and m_fifo
        dataSignal.m_type = WavesSignal::FIFO_TRIGGER;
        m_trigger = proxy;
        return;
    }

    // Add the credit signal if there's flow control
    if (!m_fifo->noflow)
    {
        g_currComponent->signals[reversePortDirection(m_name) + "_credit"] = &creditSignal;
    }

    dataSignal.m_data = m_fifo->data;
    m_freeCount = m_fifo->freeCount;
    m_fullCount = m_fifo->fullCount;

    if (dataSignal.m_type == WavesSignal::FIFO_PRODUCER && m_fifo->producerClockDomain)
        m_fifo->producerClockDomain->addWavesFifo(this);
    if (dataSignal.m_type == WavesSignal::FIFO_CONSUMER && m_fifo->consumerClockDomain)
        m_fifo->consumerClockDomain->addWavesFifo(this);
}

void WavesFifo::archive ()
{
    int numPendingPushes = m_fullCount;
    m_head = m_fifo->head;
    m_tail = m_fifo->tail;
    m_freeCount = m_fifo->freeCount;
    m_fullCount = m_fifo->fullCount;

    if (m_fifo->delay && dataSignal.m_type == WavesSignal::FIFO_CONSUMER)
    {
        m_tail += numPendingPushes * m_fifo->dataSize;
        if (m_tail >= m_fifo->size)
            m_tail -= m_fifo->size;
    }
}

void WavesFifo::archiveFullCount ()
{
    m_fullCount = m_fifo->fullCount;
}

void WavesFifo::tick ()
{
    if (dataSignal.m_type == WavesSignal::FIFO_TRIGGER)
        return;

    if (m_fifo->delay)
    {
        if (dataSignal.m_type == WavesSignal::FIFO_CONSUMER)
        {
            // Consumer (valid, data); delay > 0
            m_valid = (m_fullCount != m_fifo->fullCount);
            if (m_valid)
            {
                dataSignal.m_data = m_fifo->data + m_tail;
                if (!m_tail)
                    m_tail = m_fifo->size;
                m_tail -= m_fifo->dataSize;
            }
        }
        else
        {
            // Producer credit; delay > 0
            m_credit = (m_freeCount != m_fifo->freeCount);
        }
    }
}

void WavesFifo::update ()
{
    if (dataSignal.m_type == WavesSignal::FIFO_TRIGGER)
    {
        m_valid = m_trigger->m_triggered;
        m_trigger->m_triggered = 0;
        validSignal.dump();
        if (m_valid)
            dataSignal.dump();
        return;
    }

    m_fullCount = m_fifo->fullCount;
    m_freeCount = m_fifo->freeCount;

    // (valid, data)
    if (dataSignal.m_type == WavesSignal::FIFO_PRODUCER || !m_fifo->delay)
    {
        m_valid = (m_tail != m_fifo->tail);
        if (m_valid)
            dataSignal.m_data = m_fifo->data + m_tail;
        m_tail = m_fifo->tail;
    }

    // credit
    if (dataSignal.m_type == WavesSignal::FIFO_CONSUMER || !m_fifo->delay)
    {
        m_credit = (m_head != m_fifo->head);
        m_head = m_fifo->head;
    }

    // Dump
    validSignal.dump();
    if (m_valid)
        dataSignal.dump();
    if (!m_fifo->noflow)
        creditSignal.dump();
}

////////////////////////////////////////////////////////////////////////////////
//
// WavesFifoTriggerProxy
//
////////////////////////////////////////////////////////////////////////////////
WavesFifoTriggerProxy::WavesFifoTriggerProxy (Fifo<byte> *fifo) :
m_trigger((ITrigger<byte> *) (fifo->target & ~intptr_t(TRIGGER_ITRIGGER))),
m_data(new byte[fifo->dataSize]),
m_size(fifo->dataSize),
m_triggered(0)
{
    fifo->target = ((intptr_t) this) | (fifo->size ? TRIGGER_ITRIGGER : 0);
}

void WavesFifoTriggerProxy::trigger (const byte &data)
{
    memcpy(m_data, &data, m_size);
    m_triggered = 1;
    m_trigger->trigger(data);
}

////////////////////////////////////////////////////////////////////////////////
//
// WavesComponent
//
////////////////////////////////////////////////////////////////////////////////
void WavesComponent::doAcross (wavefunc f)
{
    g_currComponent = this;

    // FIFOs (before signals because classify might add a credit signal)
    for (int i = 0 ; i < (int) fifos.size() ; i++)
        (fifos[i]->*f)();

    // Signals
    for_map_values (WavesSignal *s, signals)
        (s->*f)();

    // Recurse
    for_map_values (WavesComponent *c, children)
        c->doAcross(f);

    g_currComponent = NULL;
}

WavesComponent::~WavesComponent ()
{
    cleanup();
}

void WavesComponent::cleanup ()
{
    for_map_values (WavesComponent *c, children)
        delete c;
    for (int i = 0 ; i < (int) fifos.size() ; i++)
    {
        signals.erase(fifos[i]->m_name);
        signals.erase(fifos[i]->m_name + "_valid");
        signals.erase(reversePortDirection(fifos[i]->m_name) + "_credit");
        delete fifos[i];
    }
    for_map_values (WavesSignal *s, signals)
        delete s;
    children.clear();
    signals.clear();
    fifos.clear();
}

void WavesComponent::writeIndex ()
{
    // Signals
    typedef std::map<int, Iterator<SignalMap> > OrderedSignalMap;
    OrderedSignalMap orderedSignals;
    for (Iterator<SignalMap> its(signals) ; its ; its++)
        orderedSignals[(*its)->m_index] = its;
    for_map_values (Iterator<SignalMap> &its, orderedSignals)
        (*its)->writeIndex(its.key());

    // Subcomponents
    for (MapItem<ComponentMap> itc : children)
    {
        g_file->beginComponent(*itc.key);
        itc.value->writeIndex();
        g_file->endComponent();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// WaveFile
//
////////////////////////////////////////////////////////////////////////////////
WavesFile::WavesFile () : m_file(NULL), m_currSimTime(uint64(-1))
{
}

WavesFile::~WavesFile ()
{
    if (m_file)
        fclose(m_file);
}

void WavesFile::open (const char *filename)
{
    m_file = fopen(filename, fmode());
    assert_always(m_file, "Could not open %s", filename);
    beginFile();
}

void WavesFile::close ()
{
    if (m_file)
    {
        if (Sim::simTime > m_currTime)
            dumpTime(Sim::simTime);
        fclose(m_file);
    }
    m_file = NULL;
}

void WavesFile::valueChange (uint32 id, const byte *value, bool undefined, int sizeInBits)
{
    // Whenever the simulation time changes, write out a new timestamp.  But the 
    // simulation time might not be monotonically increasing (due to manual clock 
    // domains), whereas the VCD file time must be.  Also, place a restriction 
    // (10 ps by default) on the minimum time granularity.  The net result is that 
    // if the requested increase in time is negative or less than 10 ps, then just 
    // increase the previous timestamp by 10 instead of taking on the new simulation time.
    if (m_currSimTime != Sim::simTime)
    {
        if (m_currSimTime == uint64(-1))
            m_currTime = Sim::simTime;
        else
        {
            if (m_currTime > Sim::simTime - params.WavesDT)
                m_currTime += params.WavesDT;
            else
                m_currTime = Sim::simTime;
        }
        m_currSimTime = Sim::simTime;
        dumpTime(m_currTime);
    }

    dumpValue(id, value, undefined, sizeInBits);
}

////////////////////////////////////////////////////////////////////////////////
//
// VcdWaveFile
//
////////////////////////////////////////////////////////////////////////////////
VcdWavesFile::~VcdWavesFile ()
{
    if (m_file && (Sim::simTime > m_currTime))
        dumpTime(Sim::simTime);
}

void VcdWavesFile::beginFile ()
{
    assert(m_file);

    // Date
    time_t ltime;
    char t[64];
    time(&ltime);
#if defined _MSC_VER
    ctime_s(t,sizeof(t),&ltime);
#else
    ctime_r(&ltime, t);
#endif
    t[strlen(t)-1] = '\0';
    fprintf(m_file, "$date      %s\n$end\n\n", t);

    // Version
    fprintf(m_file, "$version   Cascade version %s\n$end\n\n", CASCADE_VERSION);

    // Timescale
    fprintf(m_file, "$timescale %s\n$end\n\n", **params.WavesTimescale);
}

void VcdWavesFile::beginComponent (const char *name)
{
    assert(m_file);
    fprintf(m_file, "$scope module %s $end\n", name);
}

uint32 VcdWavesFile::addSignal (const char *name, int sizeInBits)
{
    assert(m_file);

    union
    {
        uint32 id32;
        byte   id[5];
    };
    id[4] = 0;
    uint32 ret = id32 = m_nextId;

    fprintf(m_file, "$var wire %d %s %s $end\n", sizeInBits, id, name);
    int i = 0;
    while (++id[i] == 127)
    {
        id[i++] = 33;
        if (!id[i])
            id[i] = 32;
    }
    assert_always(i < 4, "Too many signals for waves file");
    m_nextId = id32;

    return ret;
}

void VcdWavesFile::endComponent ()
{
    assert(m_file);
    fprintf(m_file, "$upscope $end\n");
}

void VcdWavesFile::endSignals ()
{
    assert(m_file);
    fprintf(m_file, "\n$enddefinitions $end\n\n");
}

void VcdWavesFile::dumpTime (uint64 time)
{
    assert(m_file);
    fprintf(m_file, "#%" PRId64 "\n", time);
}

void VcdWavesFile::dumpValue (uint32 id, const byte *value, bool undefined, int sizeInBits)
{
    assert(m_file);
    assert(id);

    union
    {
        uint32 id32;
        byte   id8[5];
    };
    id32 = id;
    id8[4] = 0;

    descore::ScopedSpinLock lock(m_lock);
    if (sizeInBits == 1)
        fputc(undefined ? 'x' : '0' + (*value & 1), m_file);
    else
    {
        fputc('b', m_file);
        for (int i = sizeInBits - 1 ; i >= 0 ; i--)
            fputc(undefined ? 'x' : '0' + ((value[i/8] >> (i&7)) & 1), m_file);
        fputc(' ', m_file);
    }
    fprintf(m_file, "%s\n", id8);
}

END_NAMESPACE_CASCADE
