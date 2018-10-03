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
// Stack.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Custom stack implementation.  This is a simple, lightweight implementation 
// that assumes that T is just a structure with no custom constructors or 
// assignment operators.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Stack_hpp_
#define Cascade_Stack_hpp_

BEGIN_NAMESPACE_CASCADE

// T must not have any constructors or assignment operators
template <typename T, typename Alloc = std::allocator<T> >
class stack
{
    DECLARE_NOCOPY(stack);
public:
    stack () : m_size(0), m_capacity(0), m_vals(NULL) {}
    ~stack () 
    { 
        if (m_vals)
        {
            Alloc alloc;
            alloc.deallocate(m_vals, m_capacity);
        }
    }

    // Push a value onto the stack
    inline void push (const T &val)
    {
        if (m_size == m_capacity)
            realloc(2 * m_capacity);
        m_vals[m_size++] = val;
    }

    // Pop the top value off of the stack
    inline T &pop ()
    {
        assert(m_size);
        return m_vals[--m_size];
    }

    // Retrieve the top value
    inline T &back ()
    {
        assert(m_size);
        return m_vals[m_size-1];
    }
    inline const T &back () const
    {
        assert(m_size);
        return m_vals[m_size-1];
    }

    // Retrieve an arbitrary indexed value
    inline T &operator[] (int index)
    {
        assert((unsigned) index < (unsigned) m_size);
        return m_vals[index];
    }
    inline const T &operator[] (int index) const
    {
        assert((unsigned) index < (unsigned) m_size);
        return m_vals[index];
    }

    // Get the size of the stack
    inline int size () const 
    { 
        return m_size; 
    }
    inline bool empty () const
    {
        return !m_size;
    }

    // Clear the stack
    inline void clear () 
    { 
        m_size = 0; 
    }

    // Ensure that the stack can handle the specified capacity
    void reserve (int size)
    {
        if (m_capacity < size)
            realloc(size);
    }

    // Set the size of the stack (element values will be undefined)
    void resize (int size)
    {
        reserve(size);
        m_size = size;
    }

protected:

    // Helper function
    void realloc (unsigned capacity)
    {
        Alloc alloc;
        if (m_vals)
        {
            T *newvals = alloc.allocate(capacity);
            memcpy(newvals, m_vals, m_size * sizeof(T));
            alloc.deallocate(m_vals, m_capacity);
            m_vals = newvals;
            m_capacity = capacity;
        }
        else
        {
            m_capacity = capacity ? capacity : 2;
            m_vals = alloc.allocate(m_capacity);
        }
    }

protected:

    int m_size;
    int m_capacity;
    T *m_vals;
};

END_NAMESPACE_CASCADE

#endif

