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
// Hierarchy.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Classes and macros used to automatically deduce the component/interface
// hierarchy at construction time.  The component hierarchy is dynamic and
// is stored as a tree with parent, first child and next sibling pointers.
// the interface hierarchy is static: every component and interface type
// has an associated static InterfaceDescriptor class which completely
// describes its ports and interfaces in a hierarchical manner.
//
// The following hooks are used to dynamically capture this information
// at construction time:
//
// * The COMPONENT_CTOR and INTERFACE_CTOR macros cause an object to
//   be constructed before the component/interface, and destructed
//   when the component/interface has been completely constructed.
//   The object's constructor and destructor call back into the Hierarchy
//   framework.  The nice thing about this mechanism is that it is
//   able to capture component/interface inheritance.  If B inherits
//   from A (e.g. struct B : public A), then A is constructed before
//   B, so any constructor magic in A will get invoked before any
//   constructor magic in B.  However, constructor *parameters*
//   are constructed in the opposite order: first the constructor
//   parameters to B, then the constructor parameters to A.  
//
// * The Component and Interface base class/struct themselves have 
//   constructor callbacks to inform the infrastructure that the 
//   top of the derivation chain has been reached and construction
//   has actually begun.
//
// * Every Port and Array constructor has a callback so that it 
//   can be inserted into the appropriate interface descriptor.
//
// As components/interfaces are constructed, a stack of construction
// frames keeps track of the current construction state.  Each frame
// corresponds to an actual class/struct (so separate frames are created
// for base and derived components/interfaces).  Each frame contains
// an interface pointer, which points to the object instance, and a
// descriptor pointer, which points to the associated static descriptor.
// When a component/interface is constructed, the following events
// occur in order (although the order of 2 and 3 depends on the specific
// pattern of inheritence):
//
// 1.  The XXX_CTOR macro informs the infrastructure that a new
//     component/interface is being constructed.  A new construction
//     frame is created.  The construction frame's descriptor pointer
//     is set to the correct interface descriptor (which is obtained
//     within the XXX_CTOR macro via _thistype), and the interface pointer
//     is set to NULL (since we can't yet access the actual 'this' pointer).
//
// 2.  Base Components/Interfaces are constructed.  The infrastructure
//     is able to distinguish base Components/Interfaces from member
//     Components/Interfaces by calling getInterfaceDescriptor() for the 
//     parent.  When base Components/Interfaces are being constructed,
//     the return value of getInterfaceDescriptor() will not match the
//     parent frame's interface pointer.
//     
// 3.  At some point, the Component/Interface constructor sets the
//     interface pointer.  There may be several frames in a row with
//     a NULL interface pointer; all of these interface pointers are
//     initialized to the same value.
//
// 4.  The Component/Interface virtual function table is initialized.
//     There is, of course, no callback at this point, but subsequent
//     calls to getInterfaceDescriptor() will return the correct
//     descriptor for the derived Component/Interface, which will match
//     the construction frame's descriptor pointer.
//
// 5.  Member ports, interfaces and arrays are constructed.
//     The constructor callbacks populate the interface descriptor.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Hierarchy_hpp_
#define Cascade_Hierarchy_hpp_

#include "SimDefs.hpp"
#include "Interface.hpp"
#include "Stack.hpp"

class Component;

BEGIN_NAMESPACE_CASCADE

class UpdateWrapper;

////////////////////////////////////////////////////////////////////////
//
// ConstructionFrame
//
// Generic construction frame base struct
//
////////////////////////////////////////////////////////////////////////
struct ConstructionFrame
{
    DECLARE_NOCOPY(ConstructionFrame);
public:
    ConstructionFrame () {}

    void beginConstruction ();
    void endConstruction ();

    // Frame type
    uint8 type;   // one of COMPONENT, INTERFACE
    bool array;   // this is an array of components, interfaces or ports
    bool isBase;  // indicates that this is a base component/interface

    // Parent frame
    ConstructionFrame *parent;

    // Pointer to component/interface
    InterfaceBase *_interface;

    // Pointer to interface descriptor
    InterfaceDescriptor *descriptor;

    // Used to assign unique IDs to the various port directions
    uint16 portID[NUM_PORT_DIRECTIONS];

public:
    // Rather that continually creating and detroying construction
    // frames and the stacks they contain, just reuse them.
    static ConstructionFrame *alloc ();
    static void free (ConstructionFrame *frame);
    static void cleanupFrames ();

private:
    static stack<ConstructionFrame *> s_frames;
};

////////////////////////////////////////////////////////////////////////
//
// Static state and callback functions
//
////////////////////////////////////////////////////////////////////////
struct Hierarchy
{
    typedef enum { COMPONENT, INTERFACE } Type;

    //---------------------------------
    // Component/Interface callbacks
    //---------------------------------

    // Called before a component or interface is constructed
    static void beginConstruction (Type type, InterfaceDescriptor *_interface, bool array);

    // Called when a component or interface is fully constructed
    static void endConstruction ();

    // Called from the Component base class constructor; returns the parent component
    static Component *setComponent (Component *component);

    // Called from PortWrapper; returns the current component being constructed
    static Component *getComponent ();

    // Called from the Interface base class constructor
    static void setInterface (InterfaceBase *_interface);

    //---------------------------------
    // Port/Array callbacks
    //---------------------------------

    // Add a port to the current interface.
    static void addPort (PortDirection dir, const void *address, const PortInfo *port, PortWrapper *wrapper = NULL);

    // Add an array of ports to the current interface.  Returns true if one of the
    // PortArray macros was used to declare a named port array with optional size.
    static bool addPortArray (PortDirection dir, const void *address, const PortInfo *port, int stride);

    // Add an array of interfaces to the current interface
    static void addInterfaceArray (const void *address, InterfaceDescriptor *_interface, const char *arrayName);

    //---------------------------------
    // Helper functions
    //---------------------------------

    // Helper function to verify that ports and interfaces are not dynamically created.
    static void validateAddress (const ConstructionFrame *frame, const void *address, const char *type);

    // Dump the construction stack (called on a fatal error during construction)
    static void dumpConstructionStack (descore::runtime_error &error);

public:
    static ConstructionFrame *currFrame;      // Stack of construction frames
    static ConstructionFrame *currComponent;  // Top of component portion of stack
};

////////////////////////////////////////////////////////////////////////
//
// Helper structs and macros that allow the infrastructure to automatically deduce 
// the component/interface hierarchy.  DECL_CTOR should always appear at the end of 
// a component/interface constructor's arguments, e.g.
//
//   SomeComponent (DECL_CTOR) {}
//   SomeComponent (int size, DECL_CTOR) {}
//
////////////////////////////////////////////////////////////////////////

class InterfaceDescriptor;

// Capture information and pass it to the construction delimiter
struct ConstructionDelimiterArgs
{
    void operator= (const ConstructionDelimiterArgs &rhs);
public:
    ConstructionDelimiterArgs (Hierarchy::Type _type, InterfaceDescriptor *_descriptor, bool _array = false)
        : type(_type), descriptor(_descriptor), array(_array)
    {
    }
    ConstructionDelimiterArgs (const ConstructionDelimiterArgs &rhs)
        : type(rhs.type), descriptor(rhs.descriptor), array(rhs.array)
    {
    }
    Hierarchy::Type type;
    InterfaceDescriptor *descriptor;
    bool array;
};

struct ConstructionDelimiter
{
    ConstructionDelimiter (const ConstructionDelimiterArgs &args)
    {
        Hierarchy::beginConstruction(args.type, args.descriptor, args.array);
    }
    ~ConstructionDelimiter () 
    { 
        Hierarchy::endConstruction(); 
    }
};

#define DECL_CTOR Cascade::ConstructionDelimiter = \
    Cascade::ConstructionDelimiterArgs(_thistype::hierarchyType, _thistype::getInterfaceDescriptorStatic())
#define ARRAY_CTOR     Cascade::ConstructionDelimiter = \
    Cascade::ConstructionDelimiterArgs(T::hierarchyType, T::getInterfaceDescriptorStatic(), true)
#define IMPL_CTOR      Cascade::ConstructionDelimiter 

#define COMPONENT_CTOR DECL_CTOR
#define INTERFACE_CTOR DECL_CTOR

END_NAMESPACE_CASCADE

#endif

