/*
Copyright 2009, D. E. Shaw Research.
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
// Event.hpp
//
// Copyright (C) 2009 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 07/20/2009
//
// Support for events scheduled for a future rising clock edge.
//
//////////////////////////////////////////////////////////////////////

#ifndef Event_hpp_090720174042
#define Event_hpp_090720174042

#include "SimArchive.hpp"
#include "Ports.hpp"
#include <descore/Thread.hpp>

BEGIN_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Declarations
//
/////////////////////////////////////////////////////////////////

// Used for event type <=> event type id mapping
template <typename ev_t> int getEventTypeId (bool mustExist = true);

// Used for event function <=> event function id mapping
template <typename fn_t> fn_t getEvent (int id);
template <typename fn_t> int getEventId (fn_t fn, bool mustExist = true);

/////////////////////////////////////////////////////////////////
//
// Event base interface
//
/////////////////////////////////////////////////////////////////
struct IEvent
{
    virtual ~IEvent () {}

    // Execute the event
    virtual void fireEvent () = 0;

    // Retrieve the event type id (used for archiving)
    virtual int getTypeId () const = 0;

    // Ask the event object to archive itself
    virtual void archive (Archive &ar) = 0;

    // Determine if two events are the same
    bool equals (const IEvent *rhs) const;
    virtual bool _equals (const IEvent *rhs) const = 0;
};

Archive &operator| (Archive &ar, IEvent *&event);

/////////////////////////////////////////////////////////////////
//
// Event implementations with up to 4 arguments
//
/////////////////////////////////////////////////////////////////

template <typename fn_t>
class MethodEvent : public IEvent
{
public:
    MethodEvent ()
    {
    }
    MethodEvent (Component *c, fn_t fn) : m_c(c), m_fn(fn)
    {
    }
    void archive (Archive &ar)
    {
        if (ar.isLoading())
        {
            // Recover the registered function from its id
            int id;
            ar | id;
            m_fn = getEvent<fn_t>(id);
        }
        else
        {
            // Convert the registered function into its id
            int id = getEventId<fn_t>(m_fn);
            ar | id;
        }
        ar | m_c;
    }
    bool _equals (const IEvent *_rhs) const
    {
        MethodEvent *rhs = (MethodEvent *) _rhs;
        return m_c == rhs->m_c && m_fn == rhs->m_fn;
    }
protected:
    Component *m_c;
    fn_t m_fn;
};

template <typename T>
class MethodEvent0 : public MethodEvent<void (T::*) ()>
{
public:
    typedef void (T::*fn_t) ();
    MethodEvent0 () 
    {
    }
    MethodEvent0 (Component *c, fn_t fn) : MethodEvent<fn_t>(c, fn)
    {
    }
    void fireEvent ()
    {
        (((T*)MethodEvent<fn_t>::m_c)->*MethodEvent<fn_t>::m_fn)();
    }
    int getTypeId () const
    {
        return getEventTypeId<MethodEvent0>();
    }
};

template <typename T, typename A1>
class MethodEvent1 : public MethodEvent<void (T::*) (A1)>
{
public:
    typedef void (T::*fn_t) (A1);
    MethodEvent1 ()
    {
    }
    MethodEvent1 (Component *c, fn_t fn, A1 a1) : MethodEvent<fn_t>(c, fn), m_a1(a1)
    {
    }
    void fireEvent ()
    {
        (((T*)MethodEvent<fn_t>::m_c)->*MethodEvent<fn_t>::m_fn)(m_a1);
    }
    int getTypeId () const
    {
        return getEventTypeId<MethodEvent1>();
    }
    void archive (Archive &ar)
    {
        MethodEvent<fn_t>::archive(ar);
        ar | m_a1;
    }
    bool _equals (const IEvent *_rhs) const
    {
        MethodEvent1 *rhs= (MethodEvent1 *) _rhs;
        return MethodEvent<fn_t>::_equals(rhs) && m_a1 == rhs->m_a1;
    }

private:
    A1 m_a1;
};

template <typename T, typename A1, typename A2>
class MethodEvent2 : public MethodEvent<void (T::*) (A1,A2)>
{
public:
    typedef void (T::*fn_t) (A1,A2);
    MethodEvent2 () 
    {
    }
    MethodEvent2 (Component *c, fn_t fn, A1 a1, A2 a2) : MethodEvent<fn_t>(c, fn), m_a1(a1), m_a2(a2)
    {
    }
    void fireEvent ()
    {
        (((T*)MethodEvent<fn_t>::m_c)->*MethodEvent<fn_t>::m_fn)(m_a1, m_a2);
    }
    int getTypeId () const
    {
        return getEventTypeId<MethodEvent2>();
    }
    void archive (Archive &ar)
    {
        MethodEvent<fn_t>::archive(ar);
        ar | m_a1 | m_a2;
    }
    bool _equals (const IEvent *_rhs) const
    {
        MethodEvent2 *rhs= (MethodEvent2 *) _rhs;
        return MethodEvent<fn_t>::_equals(rhs) && m_a1 == rhs->m_a1 && m_a2 == rhs->m_a2;
    }

private:
    A1 m_a1;
    A2 m_a2;
};

template <typename T, typename A1, typename A2, typename A3>
class MethodEvent3 : public MethodEvent<void (T::*) (A1,A2,A3)>
{
public:
    typedef void (T::*fn_t) (A1,A2,A3);
    MethodEvent3 () 
    {
    }
    MethodEvent3 (Component *c, fn_t fn, A1 a1, A2 a2, A3 a3) : MethodEvent<fn_t>(c, fn), m_a1(a1), m_a2(a2), m_a3(a3)
    {
    }
    void fireEvent ()
    {
        (((T*)MethodEvent<fn_t>::m_c)->*MethodEvent<fn_t>::m_fn)(m_a1, m_a2, m_a3);
    }
    int getTypeId () const
    {
        return getEventTypeId<MethodEvent3>();
    }
    void archive (Archive &ar)
    {
        MethodEvent<fn_t>::archive(ar);
        ar | m_a1 | m_a2 | m_a3;
    }
    bool _equals (const IEvent *_rhs) const
    {
        MethodEvent3 *rhs= (MethodEvent3 *) _rhs;
        return MethodEvent<fn_t>::_equals(rhs) && m_a1 == rhs->m_a1 && m_a2 == rhs->m_a2 && m_a3 == rhs->m_a3;
    }

private:
    A1 m_a1;
    A2 m_a2;
    A3 m_a3;
};

template <typename T, typename A1, typename A2, typename A3, typename A4>
class MethodEvent4 : public MethodEvent<void (T::*) (A1,A2,A3,A4)>
{
public:
    typedef void (T::*fn_t) (A1,A2,A3,A4);
    MethodEvent4 () 
    {
    }
    MethodEvent4 (Component *c, fn_t fn, A1 a1, A2 a2, A3 a3, A4 a4) : MethodEvent<fn_t>(c, fn), m_a1(a1), m_a2(a2), m_a3(a3), m_a4(a4)
    {
    }
    void fireEvent ()
    {
        (((T*)MethodEvent<fn_t>::m_c)->*MethodEvent<fn_t>::m_fn)(m_a1, m_a2, m_a3, m_a4);
    }
    int getTypeId () const
    {
        return getEventTypeId<MethodEvent4>();
    }
    void archive (Archive &ar)
    {
        MethodEvent<fn_t>::archive(ar);
        ar | m_a1 | m_a2 | m_a3 | m_a4;
    }
    bool _equals (const IEvent *_rhs) const
    {
        MethodEvent4 *rhs= (MethodEvent4 *) _rhs;
        return MethodEvent<fn_t>::_equals(rhs) && m_a1 == rhs->m_a1 && m_a2 == rhs->m_a2 && m_a3 == rhs->m_a3 && m_a4 == rhs->m_a4;
    }

private:
    A1 m_a1;
    A2 m_a2;
    A3 m_a3;
    A4 m_a4;
};

/////////////////////////////////////////////////////////////////
//
// Mapping from function type to method event type
//
/////////////////////////////////////////////////////////////////
template <typename fn_t> struct MethodEventType;

template <typename T> 
struct MethodEventType<void (T::*) ()>
{
    typedef MethodEvent0<T> ev_t;
};

template <typename T, typename A1> 
struct MethodEventType<void (T::*) (A1)>
{
    typedef MethodEvent1<T,A1> ev_t;
};

template <typename T, typename A1, typename A2> 
struct MethodEventType<void (T::*) (A1, A2)>
{
    typedef MethodEvent2<T,A1,A2> ev_t;
};

template <typename T, typename A1, typename A2, typename A3> 
struct MethodEventType<void (T::*) (A1, A2, A3)>
{
    typedef MethodEvent3<T,A1,A2,A3> ev_t;
};

template <typename T, typename A1, typename A2, typename A3, typename A4> 
struct MethodEventType<void (T::*) (A1, A2, A3, A4)>
{
    typedef MethodEvent4<T,A1,A2,A3,A4> ev_t;
};

/////////////////////////////////////////////////////////////////
//
// In order to support archiving, we need to assign a unique ID to 
// each event callback type so that we can create the appropriate object
// type when we load the simulation back in.
//
/////////////////////////////////////////////////////////////////

// We'll create an event factory object for each distinct MethodEvent type,
// which will be used to construct the appropriate object type when loading
// events from an archive.
struct IEventFactory
{
    virtual ~IEventFactory () {}
    virtual IEvent *createEvent () = 0;
};

// Type-specific implementation
template <typename ev_t>
struct EventFactory : public IEventFactory
{
    IEvent *createEvent ()
    {
        return new ev_t;
    }
};

// Global array of factories
extern std::vector<IEventFactory *> g_eventTypeTable;
extern int g_eventTypeId;
extern descore::SpinLock g_eventTypeLock;

// The first time this function is called it allocates a type ID and adds the
// appropriate factory object to the global array.  Subsequent calls return
// the event type ID.
template <typename ev_t>
int getEventTypeId (bool mustExist)
{
    static volatile int eventTypeId = -1;
    if (eventTypeId < 0)
    {
        descore::ScopedSpinLock lock(g_eventTypeLock);

        // Need to check again in case two threads called this for the first time simultaneously
        if (eventTypeId < 0)
        {
            assert_always(!mustExist, "Unrecognized event type (did you forget DECLARE_EVENT?)");
            eventTypeId = g_eventTypeId++;
            g_eventTypeTable.push_back(new EventFactory<ev_t>);
        }
    }
    return eventTypeId;
}

/////////////////////////////////////////////////////////////////
//
// In order to support archiving of pending events, we need to assign
// a unique ID to each event callback function of a given type.
//
/////////////////////////////////////////////////////////////////

// Per-type global array of callback functions
template <typename fn_t>
std::vector<fn_t> &getEventTable ()
{
    static std::vector<fn_t> eventTable;
    return eventTable;
}

// The first time this function is called with a callback function it allocates
// an ID and adds the callback function to the type-specificy array.  Subsequent
// calls return the callback function ID.
template <typename fn_t>
int getEventId (fn_t fn, bool mustExist)
{
    std::vector<fn_t> &eventTable = getEventTable<fn_t>();
    unsigned i;
    for (i = 0 ; i < eventTable.size() ; i++)
    {
        if (eventTable[i] == fn)
            return i;
    }
    assert_always(!mustExist, "Unrecognized event function (did you forget DECLARE_EVENT?)");
    eventTable.push_back(fn);
    return i;
}

// Recover a callback function from its ID
template <typename fn_t>
fn_t getEvent (int id)
{
    std::vector<fn_t> &table = getEventTable<fn_t>();
    assert_always((unsigned) id < table.size(), "Invalid event ID - the archive file appears to be invalid");
    return table[id];
}

/////////////////////////////////////////////////////////////////
//
// DECLARE_EVENT()
//
/////////////////////////////////////////////////////////////////
#define DECLARE_EVENT(fn) (registerEvent(&_thistype::fn), Cascade::EventHelper(getClassName(), #fn))

class EventHelper
{
    DECLARE_NOCOPY(EventHelper);
public:
    EventHelper (const char *component, const char *fn) : m_component(component), m_fn(fn) {}

    template <typename P1> 
    EventHelper &writes (const P1 &p1) { Wr(p1); return *this; }
    template <typename P1, typename P2> 
    EventHelper &writes (const P1 &p1, const P2 &p2) { Wr(p1); Wr(p2); return *this; }
    template <typename P1, typename P2, typename P3> 
    EventHelper &writes (const P1 &p1, const P2 &p2, const P3 &p3) { Wr(p1); Wr(p2); Wr(p3); return *this; }
    template <typename P1, typename P2, typename P3, typename P4> 
    EventHelper &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5> 
    EventHelper &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> 
    EventHelper &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); Wr(p6); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> 
    EventHelper &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6, const P7 &p7) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); Wr(p6); Wr(p7); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> 
    EventHelper &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6, const P7 &p7, const P8 &p8) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); Wr(p6); Wr(p7); Wr(p8); return *this; }

private:
    template <typename T> void Wr (const Port<T> &port) 
    { 
        write(port.wrapper); 
    }
    template <typename T> void Wr (const FifoPort<T> &) 
    { 
    }
    template <typename T> void Wr (const T *port)
    { 
        write(port->wrapper, true); 
    }
    template <typename T> void Wr (const PortArray<T> &ports)
    {
        if (ports.m_size)
            write(ports.m_array->wrapper, true);
    }
    void write (PortWrapper *port, bool array = false);
    void Wr (const PortSet &ports);

private:
    const char *m_component;
    const char *m_fn;
};

// Register both the event type and the specific event callback function
template <typename fn_t>
void registerEvent (fn_t fn)
{
    getEventTypeId<typename MethodEventType<fn_t>::ev_t>(false);
    getEventId(fn, false);
}


END_NAMESPACE_CASCADE

#endif // #ifndef Event_hpp_090720174042

