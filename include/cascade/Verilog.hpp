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
// Verilog.hpp
//
// Copyright (C) 2008 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 12/22/2008
//
// Facilities for Cascade/Verilog co-simulation
//
//////////////////////////////////////////////////////////////////////

#ifndef Verilog_hpp_081222123144
#define Verilog_hpp_081222123144

#ifdef _VERILOG

#include <vcs_vpi_user.h>
#include <svdpi.h>

BEGIN_NAMESPACE_CASCADE

class VerilogModule;
struct IReadWordArray;

/////////////////////////////////////////////////////////////////
//
// VerilogPortBinding
//
// When using the VPI interface, we obtain VPI handles for the
// ports at initialization time that can be used to copy values
// to/from the Verilog simulation at run time.  When using the DPI
// interface, there are no port handles so we just store pointers
// to the C++ values.
//
/////////////////////////////////////////////////////////////////
class VerilogPortBinding
{
    DECLARE_NOCOPY(VerilogPortBinding);
    friend class VerilogModule;
public:
    // port and module are NULL for DPI port bindings
    VerilogPortBinding (vpiHandle port, vpiHandle module, const InterfaceEntry *entry, 
                        void *address, const strbuff &name, bool reverseDirection = false);

    // Late initialization for DPI (validate/set the name and size)
    void initializeDPI (const char *name, int sizeInBits);

    // Marshal data from Verilog to C++
    void updateIn ();                                                     // VPI
    void updateIn (svBitVecVal *value, const char *name, int sizeInBits); // DPI
    void copyIn (IReadWordArray &array);                                  // Shared

    // Marshal data from C++ to Verilog
    void updateOut ();                                                     // VPI
    void updateOut (svBitVecVal *value, const char *name, int sizeInBits); // DPI

private:
    // Validation (check for C++/Verilog port mismatches)
    void validatePortSize (const char *cname) const;
    void validatePortName (const char *cname) const;

private:
    vpiHandle      m_vpiHandle;   // When using VPI
    PortDirection  m_direction;   // PORT_[INPUT|OUTPUT|INOUT|CLOCK|RESET]
    const PortInfo *m_info;       // Port type traits
    int16          m_sizeInBits;  // Number of bits to copy to/from Verilog
    bool           m_initializedDPI; // C++ port bits are zeroed on the first updateIn
    union
    {
        Port<byte> *m_port;       // Pointer to bound C++ port (to access value/flags)
        Clock      *m_clock;      // When m_direction == PORT_CLOCK
        ResetPort  *m_resetPort;  // When m_direction == PORT_RESET
    };
    string         m_name;        // Name of port in verilog module

    // Keep bindings in a linked list
    VerilogPortBinding *m_next;
};

/////////////////////////////////////////////////////////////////
//
// VerilogClockBinding
//
// Used for VPI/DPI clock callbacks
//
/////////////////////////////////////////////////////////////////
struct VerilogClockBinding
{
    union
    {
        VerilogModule *cmodule; // cmodule containing the clock (used for VPI)
        const char    *name;    // name of the clock (used for DPI)
    };
    Clock     *clock;  // Clock port that we're bound to
    vpiHandle port;    // Verilog clock port for RTL-in-C++ modules

    VerilogClockBinding *next;
};

/////////////////////////////////////////////////////////////////
//
// VerilogModule
//
// Wrapper around a C++ (Cascade) implementation of a Verilog
// module, or a Verilog implementation of a Cascade component.
//
/////////////////////////////////////////////////////////////////

enum VerilogModuleInterface
{ 
    VPI_MODULE, 
    DPI_MODULE,
    VPI_SIMULATION_MODULE // created by $run_simulation
};

enum VerilogModuleImplementation
{
    VERILOG_MODULE,  // Verilog module instantiated within Cascade
    CASCADE_MODULE   // Cascade component instantiated within Verilog
};

class VerilogModule : public Component
{
    DECLARE_COMPONENT(VerilogModule);
public:
    VerilogModule (string name, string verilogName, VerilogModuleInterface type, 
                   Component *component = NULL, COMPONENT_CTOR);
    ~VerilogModule ();

    // DPI callbacks
    void dpiTransfer (svBitVecVal *value, const char *name, int sizeInBits, bool input);
    void dpiTick (const char *clockName);

    // VPI callbacks
    void vpiTick (Clock *clock);
    void updateOut ();

    // Clock tick
    void clkTick (Clock *clock);

    // Initialize the input ports and pulse output ports with zero 
    static void initModules ();
    void init ();

private:
    struct CModulePort
    {
        const InterfaceEntry *entry;
        void                 *address;
        strbuff              name;
    };

    // Helper function to bind a VPI port
    void bindPort (vpiHandle port, vpiHandle module, const CModulePort &cport);

    // Helper function to find the next port binding whose direction
    // matches the current DPI transfer direction
    void dpiAdvance ();

    // This is a verilog module unless it was created by $run_simulation
    virtual bool isVerilogModuleWrapper () const
    {
        return m_type != VPI_SIMULATION_MODULE;
    }

private:
    string m_cmoduleName;                 // Module name
    const char *m_componentName;          // Component name (module name, or NULL for simulation module)
    Component *m_module;                  // Module implementation
    VerilogModuleInterface m_type;        // Module type (VPI/DPI)
    VerilogModuleImplementation m_impl;   // Module implementation (Verilog/Cascade)
    VerilogPortBinding *m_portBindings;   // Port bindings (in order for DPI)
    VerilogClockBinding *m_clockBindings; // Clock bindings (no particular order)

    // The DPI pushToC/popFromC calls are required to visit the ports in
    // order.  Keep track here of which port is next.
    VerilogPortBinding *m_dpiNextPort; // Next port to push/pop

    // For both DPI and VPI, keep track of which direction the data is moving.
    // For DPI, this is used to detect a change in direction which requires
    // m_dpiNextPort to be reset.  For VPI, this is used to avoid multiple
    // copies when there are multiple simultaneous rising clock edges.
    bool m_updateIn;   // True for Verilog->C++ direction

public:
    // Global list of CModules for initialization and cleanup
    static VerilogModule *s_first;
    VerilogModule        *m_next;
};

/////////////////////////////////////////////////////////////////
//
// VerilogModuleFactory
//
// Helper class used to create the C++ implementation of a verilog
// module.
//
/////////////////////////////////////////////////////////////////
class VerilogModuleFactory
{
    DECLARE_NOCOPY(VerilogModuleFactory);
public:
    VerilogModuleFactory (const char *name);
    virtual ~VerilogModuleFactory () {}

    // Virtual function implemented by factory instances to construct C++ implementation
    virtual Component *constructComponent () = 0;

    // Global function used to construct a component by name
    static Component *constructComponent (string name);

    // Add a parameter prior to object creation
    static void addNamedParam (const char *name, int val);
    static void addIndexedParam (int val);

    // Component/module registration for Verilog modules within Cascade
    static void registerModule (const char *name, const char *verilogName);
    static void registerComponent (const char *name, Component *component);
    static bool isModuleRegistered (const char *name);

protected:
    // Component factories can call this functions to retrieve parameters.
    // Parameters values are set from Verilog either by name prior to creation of
    // the C++ implementation, or, when using VPI, by index in the $create_cmodule
    // call as arguments following the cmodule name.
    //
    // If name is of the form "k|name" then the parameter can be specified either by
    // name, or as the kth argument (starting from 0) after the cmodule name.  Otherwise,
    // the parameter must be specified by name.
    //
    // If a default value is supplied then the parameter does not need to exist in advance.
    static int param (const char *name);
    static int param (const char *name, int defaultValue, bool required = false);

private:
    // Static map of module name => factory instance
    typedef std::map<string, VerilogModuleFactory *> FactoryMap;
    static FactoryMap& s_factoryMap( );
    static string& s_currName( );

    // Parameters set prior to creation of C++ implementation
    static std::map<string, int>& s_namedParams( );
    static std::vector<int>& s_indexedParams( );
};

END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// VerilogComponent
//
/////////////////////////////////////////////////////////////////
template <typename TInterface>
class VerilogComponent : public Component, public TInterface
{
    DECLARE_COMPONENT(VerilogComponent);
public:
    VerilogComponent (const char *name, COMPONENT_CTOR)
    {
        Cascade::VerilogModuleFactory::registerComponent(name, this);
    }

    // Create the VerilogComponent with the supplied name, or return
    // NULL if no Verilog module has been registered with this name
    static TInterface *create (const char *name)
    {
        if (Cascade::VerilogModuleFactory::isModuleRegistered(name))
            return new VerilogComponent(name);
        return NULL;
    }

    // Return true if a module has been registered with the supplied
    // name, false otherwise
    static bool isModuleRegistered (const char *name)
    {
        return Cascade::VerilogModuleFactory::isModuleRegistered(name);
    }
};

/////////////////////////////////////////////////////////////////
//
// CModule declaration
//
// The first argument is the name of the CModule (without quotes), which 
// must match exactly the name supplied by Verilog.  The second argument
// is a code snippet that creates the C++ implementation.  In the common
// simple case, the code snippet is of the form 'new T()', e.g.
//
//  DECLARE_CMODULE(adder, new Adder);
//
// The code snippet can also retrieve parameters using the param() function, e.g.
//
//  DECLARE_CMODULE(mux, new Mux(param("numInputs")));
//
/////////////////////////////////////////////////////////////////

#define DECLARE_CMODULE(name, construct) \
class VerilogModuleFactory__##name : public Cascade::VerilogModuleFactory \
{ \
public: \
    VerilogModuleFactory__##name () : Cascade::VerilogModuleFactory(#name) {} \
    virtual Component *constructComponent () \
    { \
        return construct; \
    } \
} verilogModuleFactory_##name

#else  // #ifdef _VERILOG

#define DECLARE_CMODULE(name, construct)

#endif // #ifdef _VERILOG

#endif // #ifndef Verilog_hpp_081222123144

