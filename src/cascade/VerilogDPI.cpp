/*
Copyright 2008, D. E. Shaw Research.
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
// VerilogDPI.cpp
//
// Verilator-compatible DPI implementation for Cascade co-simulation.
// This is a standalone DPI-only version that doesn't require VCS.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef _VERILOG_DPI

#include "Cascade.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <cstdint>

// svBitVecVal is just uint32_t in the DPI standard
typedef uint32_t svBitVecVal;

BEGIN_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Simple Port Binding for DPI
//
// Uses the public interface accessor methods rather than
// accessing protected members directly.
//
/////////////////////////////////////////////////////////////////
class DPIPortBinding
{
public:
    DPIPortBinding(const InterfaceEntry *entry, void *address, const char *name)
        : m_direction((PortDirection)entry->direction),
          m_port(nullptr),
          m_name(name ? name : ""),
          m_next(nullptr),
          m_initialized(false)
    {
        // For normal ports, get the value pointer from the Port object
        if (m_direction != PORT_CLOCK && m_direction != PORT_RESET)
        {
            Port<byte> *port = (Port<byte> *)address;
            m_port = port;
        }
    }

    void updateIn(const uint32_t *value, int sizeInBits)
    {
        if (!m_initialized)
        {
            m_initialized = true;
        }

        if (!m_port)
            return;

        // Copy value from Verilog to C++ (access value member via friend)
        byte *dest = m_port->value;
        int numBytes = (sizeInBits + 7) / 8;
        // Debug: fprintf(stderr, "  updateIn: value=%08x dest=%p numBytes=%d\n", *value, (void*)dest, numBytes);
        memcpy(dest, value, numBytes);
    }

    void updateOut(uint32_t *value, int sizeInBits)
    {
        if (!m_initialized)
        {
            m_initialized = true;
        }

        if (!m_port)
            return;

        // Copy value from C++ to Verilog (access value member via friend)
        const byte *src = m_port->value;
        int numBytes = (sizeInBits + 7) / 8;
        memset(value, 0, ((sizeInBits + 31) / 32) * 4);  // Clear first
        memcpy(value, src, numBytes);
        // Debug: fprintf(stderr, "  updateOut: value=%08x src=%p numBytes=%d\n", *value, (void*)src, numBytes);
    }

    PortDirection direction() const { return m_direction; }
    const char *name() const { return m_name.c_str(); }

    DPIPortBinding *m_next;

private:
    PortDirection m_direction;
    Port<byte> *m_port;
    string m_name;
    bool m_initialized;
};

/////////////////////////////////////////////////////////////////
//
// DPI Clock Binding
//
/////////////////////////////////////////////////////////////////
struct DPIClockBinding
{
    const char *name;
    Clock *clock;
    DPIClockBinding *next;
};

/////////////////////////////////////////////////////////////////
//
// DPIModule
//
// Wrapper for a Cascade component used in Verilator simulation
//
/////////////////////////////////////////////////////////////////
class DPIModule
{
public:
    DPIModule(const char *name, const char *verilogName, Component *component = nullptr);
    ~DPIModule();

    void transfer(uint32_t *value, const char *name, int sizeInBits, bool input);
    void tick(const char *clockName);

private:
    void advance();

    string m_name;
    string m_verilogName;
    Component *m_component;
    DPIPortBinding *m_portBindings;
    DPIClockBinding *m_clockBindings;
    DPIPortBinding *m_nextPort;
    bool m_updateIn;
    bool m_ownsComponent;
};

/////////////////////////////////////////////////////////////////
//
// Component Factory
//
/////////////////////////////////////////////////////////////////
class DPIModuleFactory
{
public:
    typedef Component *(*ConstructorFunc)();

    static void registerFactory(const char *name, ConstructorFunc func)
    {
        factories()[name] = func;
    }

    static Component *construct(const char *name)
    {
        auto it = factories().find(name);
        if (it == factories().end())
        {
            fprintf(stderr, "Error: Unknown CModule '%s'\n", name);
            return nullptr;
        }
        return it->second();
    }

    static void addParam(const char *name, int value)
    {
        params()[name] = value;
    }

    static int getParam(const char *name, int defaultValue = 0)
    {
        auto it = params().find(name);
        if (it != params().end())
            return it->second;
        return defaultValue;
    }

    static void clearParams()
    {
        params().clear();
    }

    static std::map<string, ConstructorFunc> &factories()
    {
        static std::map<string, ConstructorFunc> f;
        return f;
    }

private:
    static std::map<string, int> &params()
    {
        static std::map<string, int> p;
        return p;
    }
};

/////////////////////////////////////////////////////////////////
//
// DPIModule Implementation
//
/////////////////////////////////////////////////////////////////

DPIModule::DPIModule(const char *name, const char *verilogName, Component *component)
    : m_name(name),
      m_verilogName(verilogName),
      m_component(component),
      m_portBindings(nullptr),
      m_clockBindings(nullptr),
      m_nextPort(nullptr),
      m_updateIn(true),
      m_ownsComponent(component == nullptr)
{
    // Construct component if not provided
    if (!m_component)
    {
        m_component = DPIModuleFactory::construct(name);
        DPIModuleFactory::clearParams();
    }

    if (!m_component)
        return;

    // Build port bindings from component interface
    InterfaceDescriptor *descriptor = m_component->getInterfaceDescriptor();
    PortSet::PortSetType portSet = (PortSet::PortSetType)(PortSet::AllIOs | PortSet::Resets | PortSet::Clocks);

    DPIPortBinding **lastBinding = &m_portBindings;
    for (PortIterator it(portSet, descriptor, m_component); it; it++)
    {
        if (it.hasWrapper() && it.wrapper()->noverilog)
            continue;

        const InterfaceEntry *entry = it.entry();
        void *address = it.address();
        strbuff portName = it.getName();

        // Create port binding
        DPIPortBinding *binding = new DPIPortBinding(entry, address, portName);
        *lastBinding = binding;
        lastBinding = &binding->m_next;

        // Create clock binding if this is a clock port
        if (entry->direction == PORT_CLOCK)
        {
            DPIClockBinding *clk = new DPIClockBinding;
            clk->name = entry->name;
            clk->clock = (Clock *)address;
            clk->next = m_clockBindings;
            m_clockBindings = clk;
            clk->clock->setManual();
        }
    }

    // Find first input port
    m_nextPort = m_portBindings;
    advance();
}

DPIModule::~DPIModule()
{
    while (m_portBindings)
    {
        DPIPortBinding *tmp = m_portBindings;
        m_portBindings = m_portBindings->m_next;
        delete tmp;
    }
    while (m_clockBindings)
    {
        DPIClockBinding *tmp = m_clockBindings;
        m_clockBindings = m_clockBindings->next;
        delete tmp;
    }
    if (m_ownsComponent)
        delete m_component;
}

void DPIModule::advance()
{
    for (; m_nextPort; m_nextPort = m_nextPort->m_next)
    {
        PortDirection dir = m_nextPort->direction();
        if (dir == PORT_INOUT)
            break;
        if (m_updateIn && (dir == PORT_INPUT || dir == PORT_RESET))
            break;
        if (!m_updateIn && dir == PORT_OUTPUT)
            break;
    }
}

void DPIModule::transfer(uint32_t *value, const char *name, int sizeInBits, bool input)
{
    // Direction change?
    if (m_updateIn != input)
    {
        m_updateIn = input;
        m_nextPort = m_portBindings;
        advance();
    }

    if (!m_nextPort)
    {
        fprintf(stderr, "Error: Unmatched port '%s' (input=%d)\n", name, input);
        return;
    }

    // Debug: print port matching
    // fprintf(stderr, "DPI: %s port '%s' (cascade: '%s') size=%d\n",
    //         input ? "push" : "pop", name, m_nextPort->name(), sizeInBits);

    if (value)
    {
        if (input)
            m_nextPort->updateIn(value, sizeInBits);
        else
            m_nextPort->updateOut(value, sizeInBits);
    }

    m_nextPort = m_nextPort->m_next;
    advance();
}

void DPIModule::tick(const char *clockName)
{
    // Find the clock
    DPIClockBinding *clk = m_clockBindings;
    for (; clk && clk->name && strcmp(clk->name, clockName); clk = clk->next)
        ;

    if (!clk)
    {
        fprintf(stderr, "Error: Clock '%s' not found\n", clockName);
        return;
    }

    // Call the component's update function via function pointer
    // (update() is intentionally non-virtual in cascade)
    if (m_component)
    {
        // Debug: fprintf(stderr, "DPI tick: calling update()\n");
        UpdateFunction updateFn = m_component->getDefaultUpdate();
        if (updateFn)
            (m_component->*updateFn)();
    }
}

END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// DPI-C Export Functions (called from Verilog)
//
/////////////////////////////////////////////////////////////////

extern "C" void setCModuleParam(const char *name, int value)
{
    Cascade::DPIModuleFactory::addParam(name, value);
}

static bool s_simInitialized = false;

static void ensureSimInit()
{
    if (!s_simInitialized)
    {
        s_simInitialized = true;
        Sim::init();
    }
}

extern "C" void *createCModule(const char *name, const char *verilogName)
{
    Cascade::DPIModule *module = new Cascade::DPIModule(name, verilogName);
    ensureSimInit();
    return module;
}

extern "C" void clockCModule(void *module, const char *clockName)
{
    ((Cascade::DPIModule *)module)->tick(clockName);
}

extern "C" void ignoreCPort(void *module, const char *name, int isInput)
{
    ((Cascade::DPIModule *)module)->transfer(nullptr, name, 0, isInput != 0);
}

// Transfer functions for various bit widths
static void pushToC(void *module, const svBitVecVal *value, const char *name, int sizeInBits)
{
    ((Cascade::DPIModule *)module)->transfer((uint32_t *)value, name, sizeInBits, true);
}

static void popFromC(void *module, svBitVecVal *value, const char *name, int sizeInBits)
{
    ((Cascade::DPIModule *)module)->transfer((uint32_t *)value, name, sizeInBits, false);
}

// Macro to declare pushToC/popFromC functions for each bit width
#define DECLARE_DPI_TRANSFER(N)                                                                       \
    extern "C" void pushToC##N(void *module, const svBitVecVal *value, const char *name, int sizeInBits) \
    {                                                                                                 \
        pushToC(module, value, name, sizeInBits);                                                     \
    }                                                                                                 \
    extern "C" void popFromC##N(void *module, svBitVecVal *value, const char *name, int sizeInBits)      \
    {                                                                                                 \
        popFromC(module, value, name, sizeInBits);                                                    \
    }

#define DECLARE_DPI_TRANSFERS(N1, N2, N3, N4, N5, N6, N7, N8) \
    DECLARE_DPI_TRANSFER(N1)                                  \
    DECLARE_DPI_TRANSFER(N2)                                  \
    DECLARE_DPI_TRANSFER(N3)                                  \
    DECLARE_DPI_TRANSFER(N4)                                  \
    DECLARE_DPI_TRANSFER(N5)                                  \
    DECLARE_DPI_TRANSFER(N6)                                  \
    DECLARE_DPI_TRANSFER(N7)                                  \
    DECLARE_DPI_TRANSFER(N8)

DECLARE_DPI_TRANSFERS(32, 64, 96, 128, 160, 192, 224, 256);
DECLARE_DPI_TRANSFERS(288, 320, 352, 384, 416, 448, 480, 512);
DECLARE_DPI_TRANSFERS(544, 576, 608, 640, 672, 704, 736, 768);
DECLARE_DPI_TRANSFERS(800, 832, 864, 896, 928, 960, 992, 1024);

extern "C" void setCModuleTraces(const char *traces)
{
    descore::setTraces(traces);
}

extern "C" void dumpCModuleVars(const char *dumps)
{
    Sim::setDumps(dumps);
}

extern "C" void disableCAssertion(const char * /* message */)
{
    // Simplified - would need access to assertParams
}

extern "C" void setCParameter(const char *name, const char *value)
{
    bool success = Parameter::setValueByString(name, value);
    if (!success)
        fprintf(stderr, "Error: Failed to set parameter %s to %s\n", name, value);
}

#endif // _VERILOG_DPI
