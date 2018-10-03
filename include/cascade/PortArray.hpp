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
// PortArray.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Container for an array of ports
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_PortArray_hpp_
#define Cascade_PortArray_hpp_

#include "Cascade.hpp"
#include "Component.hpp"

BEGIN_NAMESPACE_CASCADE

class PortName;

template <typename T>
class PortArray
{
    DECLARE_NOCOPY(PortArray);
    void archive (Archive &) {} // Never call Archive on Arrays
    friend class UpdateConstructor;

public:

    ////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    //////////////////////////////////////////////////////////////////////// 

    inline void preConstruct (int size)
    {
        m_size = size;
    }

    PortArray (int size) : m_array(NULL)
    {
        // Add the port array to the interface descriptor.  This function returns true
        // if a PortArray macro was used, in which case m_size will already be set.
        // If a marco was *not* used, or if the macro did not supply a size (in which
        // case m_size will be -1), then just use the size supplied to the constructor.
        // Do not allow multiple sizes to be specified.
        const char *error = NULL;
        if (T::addPortArrayEntry(this) && (m_size != -1))
        {
            if (size != -1)
                error = "size has already been specified";
        }
        else
            m_size = size;
        if (m_size < 0)
            error = "size must be non-negative";
        if (error)
        {
            InterfaceDescriptor *descriptor = Hierarchy::currFrame->descriptor;
            const InterfaceEntry *entry = descriptor->getEntry(descriptor->size() - 1);
            const char *portName = entry->name ? entry->name : PortName::portName[entry->direction];
            die("port array %s.%s: %s", descriptor->getClassName(), portName, error);
        }

        CascadeValidate(!Hierarchy::currFrame->array, "Constructing illegal array of port arrays");
        Hierarchy::currFrame->array = true;
        PortWrapper::s_arrayIndex = 0;
        m_array = new T[m_size];
        PortWrapper::s_arrayIndex = -1;
        Hierarchy::currFrame->array = false;
    }

    ~PortArray ()
    {
        delete[] m_array;
    }

    ////////////////////////////////////////////////////////////////////////
    //
    // Accessors
    //
    ////////////////////////////////////////////////////////////////////////
    inline T &operator[] (int n)
    {
        assert((unsigned) n < (unsigned) m_size);
        return m_array[n];
    }

    inline const T &operator[] (int n) const
    {
        assert((unsigned) n < (unsigned) m_size);
        return m_array[n];
    }

    inline int size () const 
    { 
        return m_size; 
    }

private:
    T  *m_array;
    int m_size;
};

// Generic array used to get array sizes in interface descriptors
struct DummyPortArrayEntry
{
    static void addPortArrayEntry (void *, const char *) {}
};
typedef PortArray<DummyPortArrayEntry> GenericPortArray;

END_NAMESPACE_CASCADE

#define DECLARE_PORT_ARRAY(Port) \
template <typename T> class Array< Port<T> > : public Cascade::PortArray< Port<T> > \
{ \
public: \
    Array (int size = -1) : Cascade::PortArray< Port<T> >(size) {} \
}

DECLARE_PORT_ARRAY(Input);
DECLARE_PORT_ARRAY(Output);
DECLARE_PORT_ARRAY(InOut);
DECLARE_PORT_ARRAY(Register);
DECLARE_PORT_ARRAY(FifoInput);
DECLARE_PORT_ARRAY(FifoOutput);

template <> class Array<Clock> : public Cascade::PortArray<Clock>
{
public:
    Array (int size = -1) : Cascade::PortArray<Clock>(size) {}
};

#endif

