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
// Event.cpp
//
// Copyright (C) 2009 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 07/20/2009
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SimDefs.hpp"
#include "SimGlobals.hpp"
#include "SimArchive.hpp"
#include "Event.hpp"

BEGIN_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Globals
//
/////////////////////////////////////////////////////////////////
std::vector<IEventFactory *> g_eventTypeTable;
int g_eventTypeId;
descore::SpinLock g_eventTypeLock;

// Automatically delete the factory objects on exit
struct CleanupEventTypeTable
{
    ~CleanupEventTypeTable ()
    {
        for (unsigned i = 0 ; i < g_eventTypeTable.size() ; i++)
            delete g_eventTypeTable[i];
    }
};
static CleanupEventTypeTable cleanupEventTypeTable;

////////////////////////////////////////////////////////////////////////////////
//
// equals()
//
////////////////////////////////////////////////////////////////////////////////
bool IEvent::equals (const IEvent *rhs) const
{
    if (typeid(*this) != typeid(*rhs))
        return false;
    return _equals(rhs);
}

/////////////////////////////////////////////////////////////////
//
// Archiving
//
/////////////////////////////////////////////////////////////////
Archive &operator| (Archive &ar, IEvent *&event)
{
    if (ar.isLoading())
    {
        // Use the type ID to create an event object of the appropriate type
        int type;
        ar | type;
        assert_always((unsigned) type < g_eventTypeTable.size(), "Invalid event type ID - the archive file appears to be invalid");
        event = g_eventTypeTable[type]->createEvent();
    }
    else
    {
        // Archive the event object type ID
        int type = event->getTypeId();
        ar | type;
    }
    event->archive(ar);

    return ar;
}

////////////////////////////////////////////////////////////////////////////////
//
// .writes()
//
////////////////////////////////////////////////////////////////////////////////
void EventHelper::write (PortWrapper *port, bool array)
{
    if (port->isFifo())
        return;

    assert_always(port->connection == PORT_UNCONNECTED || port->connection == PORT_WIRED,
        "%s::%s cannot be a writer of connected port %s", m_component, m_fn, *port->getName());
    port->nofake = 1;
    if (array && !port->arrayInternal)
    {
        for (port = port->next ; port && port->arrayInternal ; port = port->next)
            port->nofake = 1;
    }
}

void EventHelper::Wr (const PortSet &ports)
{
    PortIterator it(ports);
    for ( ; it ; it++)
    {
        PortWrapper *w = it.wrapper();
        if (w->connection == PORT_UNCONNECTED || w->connection == PORT_WIRED)
            write(w);
    }
}

END_NAMESPACE_CASCADE
