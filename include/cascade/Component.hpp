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
// Component.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Component_hpp_
#define Cascade_Component_hpp_

#include "ComponentExtensions.hpp"
#include "Hierarchy.hpp"
#include "Event.hpp"
#include <descore/Trace.hpp>

// Class declaration
class Component;

BEGIN_NAMESPACE_CASCADE
class ClockDomain;
struct ConstructionFrame;
struct Hierarchy;
class PortName;
struct UpdateFunctions;
class Tracer;
struct Waves;
typedef void (Component::*UpdateFunction) ();
END_NAMESPACE_CASCADE

#define COMPONENT_NULL_ID 0x7fff

// #define ENABLE_ACTIVATION_STATS

////////////////////////////////////////////////////////////////////////
//
// Component
//
// This is the base class from which all components are derived.  reset(), 
// tick(), archive() and update functions are the only functions that ever 
// need to be explicitly implemented; the others are defined automatically 
// by the infrastrucutre.  The components are stored in a tree created 
// in the natural way, using the 'parentComponent', 'childComponent' and 
// 'nextComponent' pointers.
//
////////////////////////////////////////////////////////////////////////
class Component : public Cascade::ComponentExtensions
{
    ////////////////////////////////////////////////////////////////////////////
    //
    // Functions and member variables for developer use
    //
    ////////////////////////////////////////////////////////////////////////////

public:

    //----------------------------------
    // Activation
    //----------------------------------

    // Activate the component
    inline void activate () 
    {
#ifdef ENABLE_ACTIVATION_STATS
        if (!m_componentActive)
            Sim::stats.numActivations++;
#endif
        m_componentActive = 1;
    }

    // Deactivate the component
    inline void deactivate () 
    {
#ifdef ENABLE_ACTIVATION_STATS
        Sim::stats.numDeactivations++;
#endif
        m_componentActive = 0;
    }

    // Determine if the component is active
    inline bool isActive () const 
    { 
        return m_componentActive != 0; 
    }

    //----------------------------------
    // Name
    //----------------------------------

    // Get the full hierarchical component name
    strbuff getName () const;

    // Get the component name without the parent component names
    strbuff getLocalName () const;

    //----------------------------------
    // Tracing
    //----------------------------------

    // Enable tracing within this component
    void setTrace (const char *key = "");

    // Disable tracing within this component
    void unsetTrace (const char *key = "");

    //----------------------------------
    // Other accessors
    //----------------------------------

    // Get the period of this component's clock in picoseconds
    int getClockPeriod ();

    // Returns the number of rising clock edges, including the current one.
    // This function should *only* be called from the component's update 
    // function; in particular calling getTickCount() for a different 
    // component can return the incorrect result if the other component
    // is in a different clock domain.
    int getTickCount () const;

    // Returns true if this component is serving as a verilog module
    bool isVerilogModule () const
    {
        return parentComponent && parentComponent->isVerilogModuleWrapper();
    }

    //----------------------------------
    // Events
    //----------------------------------
    template <typename fn_t>
    inline void scheduleEvent (int delay, fn_t fn)
    {
        scheduleEventInternal(delay, new typename Cascade::MethodEventType<fn_t>::ev_t(this, fn));
    }
    template <typename fn_t, typename A1>
    inline void scheduleEvent (int delay, fn_t fn, A1 a1)
    {
        scheduleEventInternal(delay, new typename Cascade::MethodEventType<fn_t>::ev_t(this, fn, a1));
    }
    template <typename fn_t, typename A1, typename A2>
    inline void scheduleEvent (int delay, fn_t fn, A1 a1, A2 a2)
    {
        scheduleEventInternal(delay, new typename Cascade::MethodEventType<fn_t>::ev_t(this, fn, a1, a2));
    }
    template <typename fn_t, typename A1, typename A2, typename A3>
    inline void scheduleEvent (int delay, fn_t fn, A1 a1, A2 a2, A3 a3)
    {
        scheduleEventInternal(delay, new typename Cascade::MethodEventType<fn_t>::ev_t(this, fn, a1, a2, a3));
    }
    template <typename fn_t, typename A1, typename A2, typename A3, typename A4>
    inline void scheduleEvent (int delay, fn_t fn, A1 a1, A2 a2, A3 a3, A4 a4)
    {
        scheduleEventInternal(delay, new typename Cascade::MethodEventType<fn_t>::ev_t(this, fn, a1, a2, a3, a4));
    }

public:
    Component *parentComponent; // pointer to parent component (NULL for top-level components)
    Component *childComponent;  // pointer to first child component
    Component *nextComponent;   // pointer to next sibling

    ////////////////////////////////////////////////////////////////////////////
    //
    // Functions and member variables for infrastructure use
    //
    ////////////////////////////////////////////////////////////////////////////

public:

    // Insert the component into the tree of components
    Component();

    // Notify the infrastructure that we're cleaning up
    virtual ~Component();

    // Default implementation just dies
    virtual void archive (Archive &ar);

    // Combinational update.  update() is intentionally non-virtual
    // so that default update() functions can be detected automatically.
    void update () {}

    // Components may override tick() to perform actions on the rising clock 
    // edge.  It is called automatically by Sim.  Note that tick() is 
    // intentionally non-virtual; this is used to automatically construct
    // lists of Tickable components at initialization time.
    void tick () {}

protected:

    DECLARE_NOCOPY(Component);

    friend class Sim;
    friend class SimArchive;
    friend struct Cascade::ConstructionFrame;
    friend struct Cascade::Hierarchy;
    friend class Cascade::PortName;
    friend class Cascade::InterfaceName;
    friend class Cascade::UpdateWrapper;
    friend class Cascade::ClockDomain;
    friend struct Cascade::UpdateFunctions;
    friend class Cascade::Tracer;
    friend class Archive;
    friend class Cascade::PortWrapper;
    friend struct Cascade::Waves;

public:

    // Used to provide an informative error message if a class doesn't inherit from Component
    typedef bool _is_component;

    // Used to provide an informative error message if a class inherits from multiple Components
    void _this_class_inherits_from_two_components () {}

    //----------------------------------
    // Name
    //----------------------------------
    
    // Support for explicit component names (if Component class includes a
    // member m_componentName then it will override this static member and will
    // be used as the component name).
    static const int m_componentName = 0;

    inline static const char *resolveName (const char *name, int)
    {
        return name;
    }
    inline static const char *resolveName (const char *, const char *name)
    {
        return name;
    }
    inline static const char *resolveName (const char *, const string &name)
    {
        return *name;
    }

    // Return the string corresponding to the class name.  Need a default implementation
    // in case the Component() constructor causes an assertion to fail.
    virtual const char *getComponentName () const 
    { 
        return "Unknown unconstructed component"; 
    }

    // Format the local name of this component, including 'id'.  Does not
    // include parent component names.  Returns true if name != "".
    bool formatLocalName (strbuff &s) const;

    // Format the full name of this component, including 'id' and parent names, 
    // as well as a scope separator if requested.  Does nothing if name is ""
    // so that anonymous components are omitted from the naming hierarchy.
    void formatName (strbuff &s, bool separator = true) const;

    // Format the child id.  Default is simply %d.  Allows for custom
    // formatting for container components (e.g. arrays).
    virtual void formatChildId (strbuff &s, int id) const;

    // Suppress the child name.  Allows for custom naming for
    // container components (e.g. arrays).
    virtual bool suppressChildName () const
    {
        return false;
    }

    // Suppress the dot after the component name.  Used for named arrays.
    virtual bool suppressDot () const
    {
        return false;
    }

    //----------------------------------
    // Tracing 
    //----------------------------------

    TraceKeys getTraceKeys () const
    {
        return m_componentTraces;
    }

    string getTraceContext () const
    {
        return *getName();
    }

    void setTraceKeys (TraceKeys keys)
    {
        m_componentTraces = keys;
    }

    //----------------------------------
    // Misc
    //----------------------------------

    // Returns true if this component is automatically archived
    virtual bool autoArchive () const 
    { 
        return true;
    }

    // Schedule an event
    void scheduleEventInternal (int delay, Cascade::IEvent *event);

    // Set the component id during initialization
    void setComponentId (uint16 id) 
    { 
        assert_always(id != COMPONENT_NULL_ID, "Too many sibling components");
        m_componentId = id; 
    }

    // Since Tick is not defined as a virtual function, this virtual function
    // (defined by the Component macro) is actually used to call Tick.
    virtual void doTick () = 0;

    // Returns true if tick() has been defined for this component
    virtual bool hasTick () const = 0;

    // Returns the default update() function for this component
    virtual Cascade::UpdateFunction getDefaultUpdate () const = 0;

    // Delegate to Tracer::getTraceId()
    static int getTraceId (const char *traceName);

    // Retrieve the component's clock domain
    Cascade::ClockDomain *getClockDomain (bool required = true) const;

    // Used by ComponentExtentions to retrieve the component
    Component *getComponent ()
    {
        return this;
    }
    const Component *getComponent () const
    {
        return this;
    }

    // Returns true if this is a Cascade::VerilogModule containing a cascade component
    virtual bool isVerilogModuleWrapper () const
    {
        return false;
    }

public: // Functions required for Array<Component>

    static void addInterfaceArrayEntry (void *, const char *) {}
    static const Cascade::Hierarchy::Type hierarchyType = Cascade::Hierarchy::COMPONENT;

protected:
    uint16 m_componentTraces;
    uint16 m_componentActive      : 1;
    uint16 m_componentId          : 15;

};

// Make sure the Component structure isn't bigger than expected.  Should be
// 4 pointeres (vtable + hierarchy) plus 4 bytes, which may be rounded up to 
// 5 pointers.
STATIC_ASSERT(sizeof(Component) <= 5 * sizeof(void *));

////////////////////////////////////////////////////////////////////////
//
// Macros to create a named component
//
////////////////////////////////////////////////////////////////////////
#define _DECLARE_COMPONENT(T, component, name) \
    typedef _is_component this_class_needs_to_inherit_from_Component; \
    typedef T _thistype; \
    DECLARE_NOCOPY(T); \
    protected: \
    virtual bool hasTick () const \
    {  \
        void (Component::* t1) (); \
        t1 = (void (Component::*) ()) &T::_this_class_inherits_from_two_components; \
        t1 = (void (Component::*) ()) &T::tick; \
        return t1 != &Component::tick; \
    } \
    virtual void doTick () { tick(); } \
    virtual Cascade::UpdateFunction getDefaultUpdate () const { return (Cascade::UpdateFunction) &T::update; } \
    virtual void setParentComponent (Component *c) \
    { \
        assert_always((void *) this == (void *) (Component *) this, \
        "'Component' (or a base component) must appear first in %s's inheritance list", #T); \
        CascadeValidate(this == c, "currComponent mismatch"); \
    } \
    public: \
    DECLARE_INTERFACE_FUNCTIONS(T) \
    static void addInterfaceArrayEntry (void *, const char *) {} \
    static const Cascade::Hierarchy::Type hierarchyType = Cascade::Hierarchy::COMPONENT; \
    inline strbuff getName () const { return Component::getName(); } \
    inline Component *getComponent () { return this; } \
    inline const Component *getComponent () const { return this; } \
    static const char *getInterfaceName () { return *name ? ((name[0] == '0' && !name[1]) ? NULL : name) : component; } \
    static const char *getClassName () { return component; } \
    virtual const char *getComponentName () const { return resolveName(getInterfaceName(), m_componentName); } \
    TraceKeys getTraceKeys () const { return m_componentTraces; } \
    string getTraceContext () const { return *getName(); } \
    string getAssertionContext () const { return *getName(); } \
    private: 

#define DECLARE_COMPONENT(T, ...)  _DECLARE_COMPONENT(T, #T, "" #__VA_ARGS__)

#endif

