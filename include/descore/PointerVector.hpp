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
// PointerVector.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Compact vector which either points directly at the single entry or at a
// standard vector of multiple entries.  This can only be used for vectors
// of non-NULL pointers to objects of size >= 2 because the low pointer bit 
// is used to distinguish between vectors of size 0/1 and larger vectors.
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_PointerVector_hpp_
#define descore_PointerVector_hpp_

#include "assert.hpp"
#include <vector>

BEGIN_NAMESPACE_DESCORE

template <typename T> struct static_deref
{
};

template <typename T>
struct static_deref <T *>
{
    typedef T deref_t;
};

template <typename T>
struct static_deref <const T *>
{
    typedef T deref_t;
};

template <typename Tp, typename Alloc = std::allocator<Tp> >
class PointerVector
{
    typedef std::vector<Tp, Alloc> vec_t;
public:
    STATIC_ASSERT(sizeof(typename static_deref<Tp>::deref_t) >= 2);

    PointerVector () : element(NULL) 
    {
    }
    ~PointerVector ()
    {
        deleteVector();
    }
    inline void deleteVector ()
    {
        if (isVector)
        {
            typename Alloc::template rebind<vec_t>::other alloc;
            alloc.destroy(vec());
            alloc.deallocate(vec(), 1);
        }
    }

    PointerVector (const PointerVector &rhs) : element(NULL)
    {
        *this = rhs;
    }

    void operator= (const PointerVector &rhs)
    {
        deleteVector();
        if (rhs.isVector)
        {
            typename Alloc::template rebind<vec_t>::other alloc;
            vec_t *myVec = alloc.allocate(1);
            alloc.construct(myVec, *rhs.vec());
            elements = (intptr_t) myVec;
            isVector = 1;
        }
        else
            element = rhs.element;
    }

    void push_back (Tp ptr)
    {
        assert(ptr);
        if (!element)
            element = ptr;
        else
        {
            if (!isVector)
            {
                Tp temp = element;

                typename Alloc::template rebind<vec_t>::other alloc;
                vec_t *myVec = alloc.allocate(1);
                new ((void *)myVec) vec_t;
                elements = (intptr_t) myVec;
                isVector = 1;
                vec()->push_back(temp);
            }
            vec()->push_back(ptr);
        }
    }

    Tp pop_back ()
    {
        if (isVector)
        {
            vec_t *myVec = vec();
            Tp ret = myVec->back();
            if (myVec->size() == 1)
            {
                deleteVector();
                element = NULL;
            }
            else
                myVec->pop_back();
            return ret;
        }
        else
        {
            assert(element);
            Tp ret = element;
            element = NULL;
            return ret;
        }
    }

    inline Tp back () const
    {
        return isVector ? vec()->back() : element;
    }

    inline int size () const
    {
        return isVector ? (int) vec()->size() : element ? 1 : 0;
    }

    inline bool empty () const
    {
        return (element == NULL);
    }

    inline Tp operator[] (int index)
    {
        return isVector ? (*vec())[index] : element;
    }     

    inline const Tp operator[] (int index) const
    {
        return isVector ? (*vec())[index] : element;
    }     

    inline void clear ()
    {
        deleteVector();
        element = NULL;
    }

private:
    inline vec_t *vec () 
    { 
        return (vec_t *) (elements - 1); 
    }
    inline const vec_t *vec () const
    { 
        return (const vec_t *) (elements - 1); 
    }

private:
    union
    {
        unsigned isVector : 1;
        Tp       element;
        intptr_t elements;
    };
};

END_NAMESPACE_DESCORE

#endif

