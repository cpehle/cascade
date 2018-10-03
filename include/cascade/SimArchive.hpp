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
// SimArchive.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Support for archiving of simulations
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_SimArchive_hpp_
#define Cascade_SimArchive_hpp_

#include <descore/Archive.hpp>
#include "Stack.hpp"

class Component;
struct Interface;

////////////////////////////////////////////////////////////////////////
//
// SimArchive (all static)
//
////////////////////////////////////////////////////////////////////////
class SimArchive
{
    friend class Sim;
public:

    // Archive an entire simulation
    static void archiveSimulation (Archive &ar);

    // Convenience functions to load/save simulations if no additional information is being stored
    static void loadSimulation (const char *filename);
    static void saveSimulation (const char *filename, bool safeMode = false);
    inline static void loadSimulation(const std::string &str) 
    {
        loadSimulation(*str);
    }
    inline static void saveSimulation(const std::string &str, bool safeMode = false) 
    {
        saveSimulation(*str, safeMode);
    }

    // Register/unregister a callback function that should be invoked whenever 
    // a simulation is archived.  Multiple callbacks can be registered; they 
    // will be called in the order in which they were registered.  The same 
    // callback function, however, cannot be registered twice.
    typedef void (*Callback) (Archive &);
    static void registerCallback (Callback callback);
    static void unregisterCallback (Callback callback);

private:

    // Archive a component
    static void archiveComponent (Component *component);

    static Archive *s_ar;          // Archive object used to archive a simulation
    static Component *s_component; // Component being archived
};

/////////////////////////////////////////////////////////////////
//
// Cascade-specific archive functions
//
/////////////////////////////////////////////////////////////////

template <typename T>
inline Archive &operator| (Archive &ar, Cascade::stack<T> &v)
{
    ar.archiveVector(v);
    return ar;
}

// Archive a Component pointer
Archive &operator| (Archive &ar, Component *&component);
inline Archive &operator| (Archive &ar, const Component *&component)
{
    return ar | (Component *&) component;
}

// Archive an Interface pointer
Archive &operator| (Archive &ar, Interface *&_interface);
inline Archive &operator| (Archive &ar, const Interface *&_interface)
{
    return ar | (Interface *&) _interface;
}

// Archive a pointer to a component (any type)
template <typename T>
static void archiveComponentPointer (Archive &ar, T *&p)
{
    STATIC_ASSERT(__is_base_of(Component, T));
    ar | (Component *&) p;
}

// Archive a pointer to an interface (any type)
template <typename T>
static void archiveInterfacePointer (Archive &ar, T *&p)
{
    STATIC_ASSERT(__is_base_of(Interface, T));
    ar | (Interface *&) p;
}

#endif

