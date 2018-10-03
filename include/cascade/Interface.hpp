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
// Interface.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Cascade automatically constructs a static interface descriptor for 
// every component and interface which is constructed in the simulation.
// This is accomplished via callbacks and constructor "magic".  The
// code in Hierarchy.cpp/.hpp is responsible for the high-level logic
// which monitors dynamic component/interface construction; it calls
// into the code in Interface.cpp/.hpp which maintains the actual 
// interface descriptors.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Interface_hpp_
#define Cascade_Interface_hpp_

#include "Stack.hpp"
#include "Triggers.hpp"
#include <descore/assert.hpp>
#include <descore/Trace.hpp>

class Component;
class Clock;

BEGIN_NAMESPACE_CASCADE

class InterfaceDescriptor;
struct PortInfo;
class PortWrapper;
struct InterfaceBase;
typedef void (*PreConstructFunction) (void *, InterfaceDescriptor *);

// Cast to byte address for byte arithmetic
#define BADDR(x) ((const byte *) x)

// Byte offset between two pointers
#define BDIFF(x, y) ((uint32) (BADDR(x) - BADDR(y)))

// The order here must match the order of PortName::portName in Ports.cpp
enum PortDirection
{
    // Note: PortWrapper currently uses a 3-bit field for 'direction', so
    // only the first eight types can have wrappers.
    PORT_INPUT,
    PORT_OUTPUT,
    PORT_INOUT,
    PORT_REGISTER,
    PORT_INFIFO,
    PORT_OUTFIFO,
    PORT_TEMP,    // Temporary storage for patched registers

    // These don't have wrappers
    PORT_CLOCK,    
    PORT_RESET,     
    PORT_SIGNAL, // Used for waves only

    NUM_PORT_DIRECTIONS
};

////////////////////////////////////////////////////////////////////////
//
// InterfaceEntry
//
// Structure to describe a single port/array/sub-interface
//
////////////////////////////////////////////////////////////////////////
struct InterfaceEntry
{
    uint32 offset;          // Byte offset from 'this' pointer
    int16  id;              // Interface entry unique id (-1 for unique interfaces)

    union
    {
        uint16 flags;
        struct 
        {
            uint8 isInterface : 1; // True if this is a sub-interface, not a port
            uint8 isBase      : 1; // True if this is a base interface (as opposed to a member interface)
            uint8 isArray     : 1; // True if this is a dynamic 'Array<>' of ports or interfaces
            uint8 arrayInternal : 1; // This port is in a static array but is not the first element
            uint8 direction   : 4; // One of PortDirection (unused if isInterface is true)
            uint8 stride;          // Array stride (in bytes) for port arrays
        };
    };

    const char *name;   // Name of port or array, or NULL if not supplied via macros

    union
    {
        InterfaceDescriptor *descriptor; // For interface entries
        const PortInfo      *portInfo;   // For port entries
    };
};

////////////////////////////////////////////////////////////////////////
//
// InterfaceDescriptor
//
// Class which describes an entire interface.  Every component and
// interface has an associated static InterfaceDescriptor.  A set of
// callback functions are used to build and validate each interface
// descriptor.
//
////////////////////////////////////////////////////////////////////////
class InterfaceDescriptor
{
    DECLARE_NOCOPY(InterfaceDescriptor);
public:
    InterfaceDescriptor (PreConstructFunction preConstruct, const char *name, const char *className, uint32 maxOffset);

    //----------------------------------
    // Initialization
    //----------------------------------

    // Add a port name to the list of port names.  Called before beginInterface().
    // Used for both individual ports and static arrays of ports (e.g. in_data[3]).
    //
    //  offset   - byte offset of port (or first port in static array) from start of interface
    //  name     - name of port (including [<size>] for static arrays)
    //  arrayLen - size of static array (or 0 for individual ports)
    //  stride   - array stride ( = sizeof(port type) )
    //
    void addPortName (uint32 offset, const char *name, int arrayLen = 0, int stride = 0);

    // Indicate that a new interface is being constructed.  Calls preConstruct.
    void beginInterface (void *interface);

    // Add a signal to the interface
    void addSignal (uint32 offset, const char *name, int arrayLen, int stride, const PortInfo *info);

    // Add a port to the interface.
    void addPort (PortDirection dir, uint32 offset, const PortInfo *port, uint16 id, PortWrapper *wrapper);

    // Add an array of ports to the current interface.  Returns true if one of the
    // PortArray macros was used to declare a named port array with optional size.
    bool addPortArray (PortDirection dir, uint32 offset, const PortInfo *port, int stride);

    // Add a sub-interface to the interface.
    void addInterface (uint32 offset, bool isBase, InterfaceDescriptor *descriptor);

    // Add an array of interfaces to the interface.
    void addInterfaceArray (uint32 offset, InterfaceDescriptor *descriptor, const char *arrayName);

    // Add a clock to the list of clock offsets
    void addClock (uint32 offset);

    // Indicate that the interface is fully constructed; 'interface' is used to extract the vtable
    void endInterface (const void *_interface);

    //----------------------------------
    // reset()
    //----------------------------------

    // Call reset() for every sub-interface (first) and then for this interface (second)
    void reset (void *_interface, int level);

    //----------------------------------
    // Accessors
    //----------------------------------

    inline const char *getName () const 
    { 
        return m_name; 
    }
    inline const char *getClassName () const 
    { 
        return m_className; 
    }
    inline uint32 maxOffset () const
    {
        return m_maxOffset;
    };
    inline bool containsArray () const 
    { 
        return m_containsArray; 
    }
    inline bool containsArrayWithClock () const 
    { 
        return m_containsArrayWithClock; 
    }
    inline int size () const 
    { 
        return m_entries.size(); 
    }
    inline int depth () const 
    { 
        return m_depth; 
    }
    inline const InterfaceEntry *getEntry (unsigned index) const 
    { 
        return &m_entries[index]; 
    }
    inline int numClocks () const
    {
        return m_clockOffsets.size();
    }
    inline Clock *getClock (const void *_interface, int index)
    {
        return (Clock *) (BADDR(_interface) + m_clockOffsets[index]);
    }
    string getAssertionContext () const
    {
        return m_name;
    }

private:
    // Helper function for construction/validation
    void addEntry (InterfaceEntry &entry, bool checkNames = true);

private:

    // The name of the interface (pointer to shared static string).
    const char *m_name;      // Human-readable
    const char *m_className; // Actual class name

    // Virtual function table used to call the appropriate reset() function.
    // NULL for Arrays.
    const void *m_vtable;

    // Function pointer used to call preConstruct
    PreConstructFunction m_preConstruct;

    // Size in bytes of the Component/Interface. This is
    // used to determine whether or not a given port is a member of
    // the interface, and to validate that ports are not dynamically
    // created using the 'new' operator.
    uint32 m_maxOffset;

    // The depth of this interface (1 if no sub-interfaces, otherwise
    // 1 + (largest depth of any sub-interface).
    uint16 m_depth;

    // Once the interface descriptor has been fully constructed,
    // subsequent interface constructions are validated to ensure
    // that they produce the exact same entries.  m_validateIndex
    // gives the index into the vector of entries at which we 
    // are validating.  Before the descriptor has been constructed,
    // the index is -1.
    int16 m_validateIndex;

    // While we're validating, we also need an index into the clock
    // offsets array.
    int16 m_validateClockIndex;

    // This flag indicates that this interface or a sub-interface
    // is an array.  It is used as an optimization to avoid 
    // unnecessary searching when looking up a port name.
    bool m_containsArray;

    // This flag indicates that this interface contains array of
    // sub-interfaces that contains at least one clock.  This forces a much
    // slower search for a default clock.
    bool m_containsArrayWithClock;

    // The actual list of ports and interfaces.
    stack<InterfaceEntry> m_entries;

    // Keep a separate list of clock offsets so that components can efficiently
    // find their clocks.  This list includes the clocks from this interface
    // and all sub-interfaces.
    stack<uint32> m_clockOffsets;

    // Stack of pre-initialized entry names, and current index into stack
    // as entries are added.  If m_entryNameIndex == -1 then preConstruct 
    // has not yet been called.  Otherwise, m_entryNameIndex is the index of
    // the next entry to match up with a port name.
    struct EntryName 
    {
        uint32 offset;
        uint32 arrayInternal;
        const char *name;
    };
    stack<EntryName> m_entryNames;
    int16 m_entryNameIndex;
};

////////////////////////////////////////////////////////////////////////
//
// InterfaceBase
//
// Every component and interface derives from this base interface
// which allows the interface descriptors to be dynamically 
// created.
//
////////////////////////////////////////////////////////////////////////
struct InterfaceBase
{
    virtual ~InterfaceBase () {}

    // Return the static interface descriptor for this component/class.
    virtual InterfaceDescriptor *getInterfaceDescriptor () const { return NULL; }

    // For interfaces, set the parent component pointer.  For components, validate
    // that the component pointer matches 'this', and that Component is the first
    // inherited base class.
    virtual void setParentComponent (Component *c) = 0;

    // For an interface, return the parent component pointer.  For a component,
    // return 'this'.
    virtual Component *getComponent () = 0;
    virtual const Component *getComponent () const = 0;

    // This function is automatically called at reset time.
    // It behaves exactly like a constructor (and the order in which
    // it is called for base/member interfaces is the same) with 
    // the exception that it will generally be called multiple times, so:
    //
    //    *** ALL IMPLEMENTATIONS MUST BE IDEMPOTENT ***
    //
    // Generally, this function should only be initializing ports and
    // internal state, which is an idempotent operation.  It should not
    // usually allocate memory; this should be done in the constructor.
    //
    // Components/interfaces can implement either version of reset depending
    // on whether or not the reset is level-dependent.  They can also implement
    // both.  Both will be called.
    virtual void reset (int /* level */) {}
    virtual void reset () {}
};

END_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////
//
// Interface
//
// Base struct from which all interfaces must derive.  The constructor
// indicates that a new interface is being constructed.
//
// Note: The DECLARE_INTERFACE macro adds a '_component' member to
//       actual interfaces.
//
////////////////////////////////////////////////////////////////////////
struct Interface : public Cascade::InterfaceBase
{
    Interface ();

    // Used to provide an informative error message if a class doesn't inherit from Interface
    typedef bool _is_interface;

    // Functions so that the obj_* tracing macros will work with a generic interface pointer
    virtual TraceKeys getTraceKeys () const = 0;
    virtual string getTraceContext () const = 0;
};

// Make sure the Interface structure is just a vtable
STATIC_ASSERT(sizeof(Interface) == sizeof(void *));

////////////////////////////////////////////////////////////////////////
//
// Macros to create a named interface.  The common case is to use
// DECLARE_INTERFACE(), which provides a default constructor.
// If a custom constructor is needed (e.g. if the interface contains
// an Array of ports), then use the DECLARE_INTERFACE_WITH_CTOR() and
// INTERFACE_CTOR macros separately.
//
////////////////////////////////////////////////////////////////////////

// For interfaces without a custom constructor, just use this
// macro exactly as you would use DECLARE_COMPONENT.  T is the
// exact interface struct name, and 'name' is the user-friendly
// interface name.
#undef DECLARE_INTERFACE
#define DECLARE_INTERFACE(T, ...) \
    typedef _is_interface this_class_needs_to_inherit_from_Interface; \
    typedef T _thistype; \
    T (INTERFACE_CTOR) {} \
    DECLARE_INTERFACE_WITH_CTOR_INTERNAL(T, __VA_ARGS__)

// For a custom constructor, use DECLARE_INTERFACE_WITH_CTOR() rather
// than DECLARE_INTERFACE(), then use INTERFACE_CTOR/IMPL_CTOR
// at the end of constructor parameter lists, as described in
// Hierarchy.hpp.
#define DECLARE_INTERFACE_WITH_CTOR(T, ...) \
    typedef T _thistype; \
    DECLARE_INTERFACE_WITH_CTOR_INTERNAL(T, __VA_ARGS__)

#define DECLARE_INTERFACE_WITH_CTOR_INTERNAL(T, ...) \
    static const Cascade::Hierarchy::Type hierarchyType = Cascade::Hierarchy::INTERFACE; \
    static void addInterfaceArrayEntry (void *arrayAddress, const char *arrayName) \
    { \
        Cascade::Hierarchy::addInterfaceArray(arrayAddress, getInterfaceDescriptorStatic(), arrayName); \
    } \
    DECLARE_INTERFACE_NAME(#T, "" #__VA_ARGS__) \
    DECLARE_INTERFACE_FUNCTIONS(T) \
    Component *_component; \
    virtual void setParentComponent (Component *c) { _component = c; } \
    Component *getComponent () { return _component; } \
    const Component *getComponent () const { return _component; } \
    strbuff getName () const { return Cascade::InterfaceName::getInterfaceName(this, _component, getInterfaceDescriptorStatic()); } \
    string getAssertionContext () const { return *getName(); } \
    inline TraceKeys getTraceKeys () const { return _component->getTraceKeys(); } \
    inline string getTraceContext () const { return *_component->getName(); }

#define DECLARE_INTERFACE_NAME(_interface, name) \
    static const char *getInterfaceName () { return *name ? name : _interface; }

#define DECLARE_INTERFACE_FUNCTIONS(T) \
    virtual Cascade::InterfaceDescriptor *getInterfaceDescriptor () const { return getInterfaceDescriptorStatic(); } \
    static Cascade::InterfaceDescriptor *getInterfaceDescriptorStatic () \
    { \
        static Cascade::InterfaceDescriptor _interface(&_thistype::preConstruct, \
        _thistype::getInterfaceName(), #T, sizeof(_thistype)); \
        return &_interface; \
    } \
    DECLARE_REFLECTION_FUNCTIONS()

// The _Index constructor shouldn't be required, but for some reason MSVC complains
// if it is omitted.
#define DECLARE_REFLECTION_FUNCTIONS() \
    template <int line, int x> struct _Counter { enum { count = _Counter<line-1,x>::count }; }; \
    template <int x> struct _Counter<__LINE__,x> { enum { count = -1 }; }; \
    template <int n> struct _Index : public Cascade::InterfaceDescriptor { _Index () {} }; \
    void _preConstruct (...) {} \
    static void preConstruct (void *interface, Cascade::InterfaceDescriptor *descriptor) \
    { \
        ((_thistype *) interface)->_preConstruct((_Index<0>*) descriptor); \
    }

#define DECLARE_NAMED_PORT(type, name) \
    type name; \
    template <int x> struct _Counter<__LINE__,x> { enum { count = _Counter<__LINE__-1,x>::count + 1 }; }; \
    void _preConstruct (_Index<_Counter<__LINE__,0>::count> *descriptor) \
    { \
        struct getsize { byte name; }; \
        descriptor->addPortName((uint32) descore_offsetof(_thistype, name), #name, (int) descore_offsetof(getsize, name), sizeof(type)); \
        _preConstruct((_Index<_Counter<__LINE__,0>::count+1> *) descriptor); \
    }

#define DECLARE_NAMED_PORT_ARRAY(type, name, ...) \
    Array< type > name; \
    template <int x> struct _Counter<__LINE__,x> { enum { count = _Counter<__LINE__-1,x>::count + 1 }; }; \
    void _preConstruct (_Index<_Counter<__LINE__,0>::count> *descriptor) \
    { \
        int portArraySize[] = {-1, ##__VA_ARGS__}; \
        int lastElement = portArraySize[(sizeof(portArraySize)/sizeof(int))-1]; \
        name.preConstruct(lastElement); \
        descriptor->addPortName(descore_offsetof(_thistype, name), #name); \
        _preConstruct((_Index<_Counter<__LINE__,0>::count+1> *) descriptor); \
    }

#define DECLARE_RESET_PORT(level, name) \
    Cascade::ResetPort name; \
    template <int x> struct _Counter<__LINE__,x> { enum { count = _Counter<__LINE__-1,x>::count + 1 }; }; \
    void _preConstruct (_Index<_Counter<__LINE__,0>::count> *descriptor) \
    { \
        struct getsize { byte name; }; \
        Cascade::ResetPort::preConstruct((Cascade::ResetPort *) &name, (int) descore_offsetof(getsize, name), level); \
        descriptor->addPortName((uint32) descore_offsetof(_thistype, name), #name, (int) descore_offsetof(getsize, name), sizeof(Cascade::ResetPort)); \
        _preConstruct((_Index<_Counter<__LINE__,0>::count+1> *) descriptor); \
    }

#define Signal(type, name) \
    type name; \
    template <int x> struct _Counter<__LINE__,x> { enum { count = _Counter<__LINE__-1,x>::count + 1 }; }; \
    void _preConstruct (_Index<_Counter<__LINE__,0>::count> *descriptor) \
    { \
        struct getsize { byte name; }; \
        descriptor->addSignal((uint32) descore_offsetof(_thistype, name), #name, (int) descore_offsetof(getsize, name), sizeof(type), Cascade::getPortInfo<type>()); \
        _preConstruct((_Index<_Counter<__LINE__,0>::count+1> *) descriptor); \
    }

/////////////////////////////////////////////////////////////////
//
// *** Did you forget DECLARE_INTERFACE or DECLARE_COMPONENT? ***
//
// Without this declaration, if the developer omits the appropriate
// DECLARE_XXX macro and then uses one of the port macros, they will 
// see the somewhat unhelpful error message
//
//      missing ';' before '<'
//
// on the port macro.  Declare _Counter here to get a different
// error message (too many template arguments) that will refer to
// this location in the source, so that the developer can see the 
// comment and figure out that they are missing a DECLARE_XXX macro.
//
/////////////////////////////////////////////////////////////////
template <int a> struct _Counter;

/////////////////////////////////////////////////////////////////
//
// InterfaceName
//
// Static helper class for constructing the full hierarchical
// name of an interface.
//
/////////////////////////////////////////////////////////////////
BEGIN_NAMESPACE_CASCADE
class InterfaceName
{
public:
    // Top-level function called to locate an interface within a component.
    //
    //  address - memory address of interface
    //  c       - component that contains interface (we'll search its interface descriptor)
    //  type    - interface descriptor of interface, used to validate type (since it's
    //            possible for multiple interfaces to have the same address when one of them
    //            inherits from the other).
    //            
    static strbuff getInterfaceName (const void *address, const Component *c, const InterfaceDescriptor *type);

private:
    // The second-level recursive function constructs a stack of SearchState entries that
    // locates the specified interface relative to the containing component.  Each SearchState
    // entry in the stack specifies the next level interface by the entry index within the
    // current interface descriptor and, if the descriptor is an interface array, the index
    // within the array.
    struct SearchState
    {
        SearchState () {}
        SearchState (int entry, int array) : entryIndex(entry), arrayIndex(array) {}
        int entryIndex;
        int arrayIndex;
    };

    // Second-level function called to locate an interface within a specified interface.
    // Returns true on success and pushes a SearchState onto the stack; returns false if 
    // the interface was not found.
    //
    //  s          - string buffer in which interface name is constructed
    //  address    - memory address of interface we're trying to find
    //  type       - interface descriptor used to check interface type (see getInterfaceName above)
    //  interface  - address of interface in which we're recursively searching
    //  descriptor - interface descriptor for interface in which we're searching
    //
    static bool formatInterfaceName (strbuff &s, const void *address, 
        const InterfaceDescriptor *type, const void *_interface, 
        const InterfaceDescriptor *descriptor, stack<SearchState> &searchStack);
};
END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Port macros
//
/////////////////////////////////////////////////////////////////

#define Input(type,name)       DECLARE_NAMED_PORT(Input< type >, name)
#define Output(type,name)      DECLARE_NAMED_PORT(Output< type >, name)
#define InOut(type,name)       DECLARE_NAMED_PORT(InOut< type >, name)
#define Register(type,name)    DECLARE_NAMED_PORT(Register< type >, name)
#define FifoInput(type,name)   DECLARE_NAMED_PORT(FifoInput< type >, name)
#define FifoOutput(type,name)  DECLARE_NAMED_PORT(FifoOutput< type >, name)

#define InputArray(type,name,...)       DECLARE_NAMED_PORT_ARRAY(Input< type >, name, ##__VA_ARGS__)
#define OutputArray(type,name,...)      DECLARE_NAMED_PORT_ARRAY(Output< type >, name, ##__VA_ARGS__)
#define InOutArray(type,name,...)       DECLARE_NAMED_PORT_ARRAY(InOut< type >, name, ##__VA_ARGS__)
#define RegisterArray(type,name,...)    DECLARE_NAMED_PORT_ARRAY(Register< type >, name, ##__VA_ARGS__)
#define FifoInputArray(type,name,...)   DECLARE_NAMED_PORT_ARRAY(FifoInput< type >, name, ##__VA_ARGS__)
#define FifoOutputArray(type,name,...)  DECLARE_NAMED_PORT_ARRAY(FifoOutput< type >, name, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////
//
// Port sets
//
////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_CASCADE

// Encapsulate the inputs or outputs of an interface
struct PortSet
{
    enum PortSetType 
    { 
        Inputs      = (1 << PORT_INPUT) | (1 << PORT_INFIFO),
        Outputs     = (1 << PORT_OUTPUT) | (1 << PORT_OUTFIFO),
        FifoInputs  = (1 << PORT_INFIFO),
        FifoOutputs = (1 << PORT_OUTFIFO),
        Fifos       = FifoInputs | FifoOutputs,
        InOuts      = 1 << PORT_INOUT,
        Registers   = 1 << PORT_REGISTER, 
        ReadPorts   = Inputs | InOuts, 
        WritePorts  = Outputs | InOuts, 
        AllIOs      = ReadPorts | WritePorts,
        AllPorts    = AllIOs | Registers,
        Clocks      = 1 << PORT_CLOCK,
        Resets      = 1 << PORT_RESET,
        Signals     = 1 << PORT_SIGNAL,
        Everything  = AllPorts | Clocks | Resets | Signals
    };

    template <typename IType>
    PortSet (const IType *_interface, PortSetType _type) : type(_type), descriptor(IType::getInterfaceDescriptorStatic()), _interface(_interface) {}
    PortSet (const PortSet &rhs) : type(rhs.type), descriptor(rhs.descriptor), _interface(rhs._interface) {}
    PortSet &operator= (const PortSet &rhs) { type = rhs.type; _interface = rhs._interface; descriptor = rhs.descriptor; return *this; }

    PortSetType type;
    const InterfaceDescriptor *descriptor;
    const void *_interface;

    static inline bool match (PortSetType type, uint8 entryDir)
    {
        return (type & (1 << entryDir)) != 0;
    }
};

// Main connection function
void connectPorts (const Cascade::PortSet &ports1, const Cascade::PortSet &ports2, bool chain, int delay = 0);

END_NAMESPACE_CASCADE

// Define various port sets
#define DECLARE_PORT_SET(PortType) \
    template <typename IType> Cascade::PortSet PortType (IType &_interface) \
    { \
        return Cascade::PortSet(&_interface, Cascade::PortSet::PortType); \
    } \
    template <typename IType> Cascade::PortSet PortType (IType *_interface) \
    { \
        return Cascade::PortSet(_interface, Cascade::PortSet::PortType); \
    }

DECLARE_PORT_SET(Inputs)
DECLARE_PORT_SET(Outputs)
DECLARE_PORT_SET(InOuts)
DECLARE_PORT_SET(AllPorts)

////////////////////////////////////////////////////////////////////////
//
// Port iterator
//
////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_CASCADE

// Generic port iterator
class PortIterator
{
    DECLARE_NOCOPY(PortIterator);
public:
    PortIterator (const PortSet &ports);
    PortIterator (PortSet::PortSetType type, const Component *component);
    PortIterator (PortSet::PortSetType type, const InterfaceDescriptor *descriptor, const void *_interface);
    ~PortIterator ();

    // Standard iterator accessors
    inline operator bool () const 
    { 
        return m_valid; 
    }
    void operator++ (int);

    // Return the depth of the iterator search stack
    inline int depth () const
    { 
        return (int) (m_state - m_stack + 1); 
    }

    // Return the byte address of the port at the top of the stack
    inline void *address () const
    { 
        void *address = (void *) (BADDR(m_state->_interface) + m_state->entry->offset);
        return m_state->entry->isArray ? arrayAddress(address, m_state->arrayIndex) : address;
    }

    // Return the interface entry for the top of the stack
    inline const InterfaceEntry *entry () const
    {
        return m_state->entry;
    }

    // Indicates whether or not the port at the top of the stack has a wrapper
    inline bool hasWrapper ()
    {
        return m_state->entry->direction < PORT_CLOCK;
    }

    // Return the port wrapper for the port at the top of the stack
    inline PortWrapper *wrapper () 
    { 
        return * (PortWrapper * const *) address(); 
    }

    // Return the current port name
    void formatName (strbuff &s) const;
    inline strbuff getName () const
    {
        strbuff s;
        formatName(s);
        return s;
    }

    // Return the byte address of the port/interface/array at the specified search stack depth
    inline const void *address (unsigned i) const
    {
        return BADDR(m_stack[i]._interface) + m_stack[i].entry->offset;
    }

    // Return the interface entry at the specified search stack depth
    inline const InterfaceEntry *entry (unsigned i) const
    { 
        return m_stack[i].entry; 
    }

    // Return the array index of the port at the top of the stack
    inline unsigned index () const
    { 
        return m_state->arrayIndex; 
    }

    // Return the array index of the port at the specified search stack depth
    inline unsigned index (unsigned i) const
    { 
        return m_stack[i].arrayIndex; 
    }

    // Return the array size of the entry at the top of the stack, or zero if it
    // is not an array.
    int arraySize () const;

private:
    void initialize (const InterfaceDescriptor *descriptor, const void *_interface);
    void advance ();
    void *arrayAddress (void *array, unsigned index) const;

private:
    PortSet::PortSetType m_type;
    bool m_valid;

    struct State
    {
        const void *_interface;      // byte address of interface at this stack depth
        const InterfaceEntry *entry; // current entry from interface descriptor
        unsigned entryIndex;         // index of entry within interface descriptor
        unsigned numEntries;         // number of entries in interface descriptor
        unsigned arrayIndex;         // for array entries, current array index
        unsigned arraySize;          // for array entires, total size of array
    };
    State *m_stack;  // State array used as search stack
    State *m_state;  // Current top of stack
};

// Specialized iterator for iterating over clocks
class ClockIterator
{
    DECLARE_NOCOPY(ClockIterator);
public:
    ClockIterator (const Component *component);
    ~ClockIterator ();

    inline operator bool () const
    {
        return m_fastIterator ? (m_clockIndex >= 0) : *m_pit;
    }
    void operator++ (int);
    Clock *operator* ();

private:
    bool  m_fastIterator; // True if we're just iterating over the descriptor clock array
    int16 m_clockIndex;
    union
    {
        const Component *m_component; // when m_fastIterator = true
        PortIterator *m_pit;          // when m_fastIterator = false
    };
};

END_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////
//
// Interface connections
//
// There are two primary ways to connect interfaces.  They can be
// *connected*:
//
//   connect(I1 ports1, I2 ports2)
//
// which connects the inputs of one to the outputs of the other, or
// they can be *chained*:
//
//   chain(I1 inner, I2 outer)
//
// which treats them as identical interfaces at different levels of
// a hierarchy; the inner interface (I1) is connected to the outer
// interface (I2) such that the inputs of I1 connect to the inputs
// of I2, and the outputs of I2 connect to the outputs of I1.
//
// The above functions produce combinational connections.  Synchronous
// connections can be established using syncConnect and syncChain.
// The set of ports to be connected for an interface can also be
// restricted using the constructs Inputs(I1), Outputs(I2), or
// InOuts(I3), e.g.
//
//   chain(Inputs(I1), Inputs(I2))
//
////////////////////////////////////////////////////////////////////////

// Proxy connection functions
template <typename I1, typename I2> inline void connect (const I1 &ports1, const I2 &ports2) 
{ 
    Cascade::connectPorts(Inputs(ports1), Outputs(ports2), false); 
    Cascade::connectPorts(Inputs(ports2), Outputs(ports1), false); 
    Cascade::connectPorts(InOuts(ports1), InOuts(ports2), true); 
}
template <typename I1, typename I2> inline void syncConnect (const I1 &ports1, const I2 &ports2, int delay = 1) 
{ 
    Cascade::connectPorts(Inputs(ports1), Outputs(ports2), false, delay); 
    Cascade::connectPorts(Inputs(ports2), Outputs(ports1), false, delay); 
}
template <typename I1, typename I2> inline void chain (const I1 &inner, const I2 &outer) 
{ 
    Cascade::connectPorts(Inputs(inner), Inputs(outer), true); 
    Cascade::connectPorts(Outputs(inner), Outputs(outer), true); 
    Cascade::connectPorts(InOuts(inner), InOuts(outer), true); 
}
template <typename I1, typename I2> inline void syncChain (const I1 &inner, const I2 &outer, int delay = 1) 
{ 
    Cascade::connectPorts(Inputs(inner), Inputs(outer), true, delay); 
    Cascade::connectPorts(Outputs(inner), Outputs(outer), true, delay); 
}

template <typename IType> inline void connect (const Cascade::PortSet &ports1, const IType &ports2) 
{ 
    Cascade::connectPorts(ports1, Cascade::PortSet(&ports2, Cascade::PortSet::AllIOs), false); 
}
template <typename IType> inline void syncConnect (const Cascade::PortSet &ports1, const IType &ports2, int delay = 1) 
{ 
    Cascade::connectPorts(ports1, Cascade::PortSet(&ports2, Cascade::PortSet::AllIOs), false, delay); 
}
template <typename IType> inline void chain (const Cascade::PortSet &inner, const IType &outer) 
{ 
    Cascade::connectPorts(inner, Cascade::PortSet(&outer, inner.type), true); 
}
template <typename IType> inline void syncChain (const Cascade::PortSet &inner, const IType &outer, int delay = 1) 
{ 
    Cascade::connectPorts(inner, Cascade::PortSet(&outer, inner.type), true, delay); 
}

template <typename IType> inline void connect (const IType &ports1, const Cascade::PortSet &ports2) 
{ 
    Cascade::connectPorts(Cascade::PortSet(&ports1, Cascade::PortSet::AllIOs), ports2, false); 
}
template <typename IType> inline void syncConnect (const IType &ports1, const Cascade::PortSet &ports2, int delay = 1)
{ 
    Cascade::connectPorts(Cascade::PortSet(&ports1, Cascade::PortSet::AllIOs), ports2, false, delay); 
}
template <typename IType> inline void chain (const IType &inner, const Cascade::PortSet &outer) 
{ 
    Cascade::connectPorts(Cascade::PortSet(&inner, outer.type), outer, true); 
}
template <typename IType> inline void syncChain (const IType &inner, const Cascade::PortSet &outer, int delay = 1) 
{ 
    Cascade::connectPorts(Cascade::PortSet(&inner, outer.type), outer, true, delay); 
}

inline void connect (const Cascade::PortSet &ports1, const Cascade::PortSet &ports2) 
{ 
    Cascade::connectPorts(ports1, ports2, false); 
}
inline void syncConnect (const Cascade::PortSet &ports1, const Cascade::PortSet &ports2, int delay = 1) 
{ 
    Cascade::connectPorts(ports1, ports2, false, delay); 
}
inline void chain (const Cascade::PortSet &inner, const Cascade::PortSet &outer) 
{ 
    Cascade::connectPorts(inner, outer, true); 
}
inline void syncChain (const Cascade::PortSet &inner, const Cascade::PortSet &outer, int delay = 1) 
{ 
    Cascade::connectPorts(inner, outer, true, delay); 
}

#endif

