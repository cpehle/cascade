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
// Array.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Container for a 1, 2 or 3 dimensional array of interfaces or components
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Array_hpp_
#define Cascade_Array_hpp_

#include "Component.hpp"
#include "Interface.hpp"

#define ARRAY_ELEMENT(x,y,z) m_array[(((z) * m_sizeY) + (y)) * m_sizeX + (x)]

template <typename T>
class Array : public Component
{
    DECLARE_COMPONENT(Array);
    typedef T *T_ptr;
    void archive (Archive &) {} // Never call archive on Arrays

    // Helper struct to reduce the number of distinct constructors that we need
    struct ArrayNameArg
    {
        ArrayNameArg (const char *_name) : name(_name), names(NULL)
        {
        }
        ArrayNameArg (const char **_names) : name(NULL), names(_names)
        {
        }
        const char *name;
        const char **names;
    };

public:

    ////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    ////////////////////////////////////////////////////////////////////////    

    // No custom allocator
    Array (int sizeX, int sizeY = -1, int sizeZ = -1, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(sizeZ), m_componentName(NULL), m_names(NULL)
    {
        construct0();
    }
    Array (const ArrayNameArg &n, int sizeX, int sizeY = -1, int sizeZ = -1, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(sizeZ), m_componentName(n.name), m_names(n.names)
    {
        construct0();
    }
    void construct0 ()
    {
        init_size();
        m_array = new T_ptr[m_size];
        for (int i = 0 ; i < m_size ; i++)
            m_array[i] = new T;
        initialize();
    }

    // One dimensional array with custom allocator
    template <class Allocator>
    Array (int sizeX, const Allocator *allocator, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(-1), m_sizeZ(-1), m_componentName(NULL), m_names(NULL)
    {
        construct1(allocator);
    }
    template <class Allocator>
    Array (const ArrayNameArg &n, int sizeX, const Allocator *allocator, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(-1), m_sizeZ(-1), m_componentName(n.name), m_names(n.names)
    {
        construct1(allocator);
    }
    template <class Allocator>
    void construct1 (const Allocator *allocator)
    {
        // Hack to eliminate the MSVC warning:
        //
        // warning C4100: 'allocator' : unreferenced formal parameter
        //
        // when allocator is a dummy pointer that the compiler optimizes out
        // (e.g. with ARRAY_INDEX_ALLOCATOR)
        allocator = allocator;
        init_size();
        m_array = new T_ptr[m_size];
        for (int i = 0 ; i < m_sizeX ; i++)
            allocator->allocateElement(ARRAY_ELEMENT(i, 0, 0), i);
        initialize();
    }

    // Two dimensional array with custom allocator
    template <class Allocator>
    Array (int sizeX, int sizeY, const Allocator *allocator, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(-1), m_componentName(NULL), m_names(NULL)
    {
        construct2(allocator);
    }
    template <class Allocator>
    Array (const ArrayNameArg &n, int sizeX, int sizeY, const Allocator *allocator, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(-1), m_componentName(n.name), m_names(n.names)
    {
        construct2(allocator);
    }
    template <class Allocator>
    void construct2 (const Allocator *allocator)
    {
        // Hack to eliminate the MSVC warning:
        //
        // warning C4100: 'allocator' : unreferenced formal parameter
        //
        // when allocator is a dummy pointer that the compiler optimizes out
        // (e.g. with ARRAY_INDEX_ALLOCATOR)
        allocator = allocator;
        init_size();
        m_array = new T_ptr[m_size];
        for (int j = 0 ; j < m_sizeY ; j++)
        {
            for (int i = 0 ; i < m_sizeX ; i++)
                allocator->allocateElement(ARRAY_ELEMENT(i, j, 0), i, j);
        }
        initialize();
    }

    // Three dimensional array with custom allocator
    template <class Allocator>
    Array (int sizeX, int sizeY, int sizeZ, const Allocator *allocator, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(sizeZ), m_componentName(NULL), m_names(NULL)
    {
        construct3(allocator);
    }
    template <class Allocator>
    Array (const ArrayNameArg &n, int sizeX, int sizeY, int sizeZ, const Allocator *allocator, ARRAY_CTOR) 
        : m_array(NULL), m_sizeX(sizeX), m_sizeY(sizeY), m_sizeZ(sizeZ), m_componentName(n.name), m_names(n.names)
    {
        construct3(allocator);
    }
    template <class Allocator>
    void construct3 (const Allocator *allocator)
    {
        // Hack to eliminate the MSVC warning:
        //
        // warning C4100: 'allocator' : unreferenced formal parameter
        //
        // when allocator is a dummy pointer that the compiler optimizes out
        // (e.g. with ARRAY_INDEX_ALLOCATOR)
        allocator = allocator;
        init_size();
        m_array = new T_ptr[m_size];
        for (int k = 0 ; k < m_sizeZ ; k++)
        {
            for (int j = 0 ; j < m_sizeY ; j++)
            {
                for (int i = 0 ; i < m_sizeX ; i++)
                    allocator->allocateElement(ARRAY_ELEMENT(i, j, k), i, j, k);
            }
        }
        initialize();
    }

    void init_size ()
    {
        if (m_sizeY == -1)
        {
            assert(m_sizeZ == -1);
            m_sizeY = m_sizeZ = 1;
            m_dimension = 1;
        }
        else if (m_sizeZ == -1)
        {
            m_sizeZ = 1;
            m_dimension = 2;
        }
        else
            m_dimension = 3;
        m_size = m_sizeX * m_sizeY * m_sizeZ;
    }

    void initialize ()
    {
        assert(m_sizeX >= 0);
        assert(m_sizeY >= 0);
        assert(m_sizeZ >= 0);

        // Interface descriptor callback.  Adds an entry to the current interface
        // for arrays of ports or interfaces; does nothing for arrays of components.
        T::addInterfaceArrayEntry(this, m_names ? "" : m_componentName);
    }

    ~Array ()
    {
        // Hack to let ~Component() know that we're not in the hierarchy if we're an interface array
        if (T::hierarchyType == Cascade::Hierarchy::INTERFACE)
            parentComponent = this;

        for (int i = 0 ; i < m_size ; i++)
            delete m_array[i];
        delete[] m_array;
    }

    virtual bool autoArchive () const
    {
        return false;
    }

    ////////////////////////////////////////////////////////////////////////
    //
    // Accessors
    //
    ////////////////////////////////////////////////////////////////////////
    inline T &operator() (int x, int y = 0, int z = 0)
    {
        assert((unsigned) x < (unsigned) m_sizeX);
        assert((unsigned) y < (unsigned) m_sizeY);
        assert((unsigned) z < (unsigned) m_sizeZ);
        return *ARRAY_ELEMENT(x, y, z);
    }

    inline const T &operator() (int x, int y = 0, int z = 0) const
    {
        assert((unsigned) x < (unsigned) m_sizeX);
        assert((unsigned) y < (unsigned) m_sizeY);
        assert((unsigned) z < (unsigned) m_sizeZ);
        return *ARRAY_ELEMENT(x, y, z);
    }

    inline T &operator[] (int n)
    {
        assert((unsigned) n < (unsigned) m_size);
        return *m_array[n];
    }

    inline const T &operator[] (int n) const
    {
        assert((unsigned) n < (unsigned) m_size);
        return *m_array[n];
    }

    inline int size () const 
    { 
        return m_size; 
    }

    ////////////////////////////////////////////////////////////////////////
    //
    // Child name helpers
    //
    ////////////////////////////////////////////////////////////////////////
    virtual void formatChildId (strbuff &s, int id) const
    {
        if (m_names)
            s.puts(m_names[id]);
        else if (m_dimension == 1)
            s.append("(%d)", id);
        else if (m_dimension == 2)
            s.append("(%d,%d)", id % m_sizeX, id / m_sizeX);
        else
            s.append("(%d,%d,%d)", id % m_sizeX, (id / m_sizeX) % m_sizeY, id / m_sizeX / m_sizeY);
    }

    virtual bool suppressChildName () const
    {
        return m_componentName || m_names;
    }

    virtual bool suppressDot () const
    {
        return m_componentName != NULL;
    }

    ////////////////////////////////////////////////////////////////////////
    //
    // doAcross()
    //
    // Call the same function on all array elements.
    //
    ////////////////////////////////////////////////////////////////////////

    void doAcross (void (T::*func) ())
    {
        for (int i = 0 ; i < m_size ; i++)
            (m_array[i]->*func)();
    }
    template <typename A1, typename X1> 
    void doAcross (void (T::*func) (X1), const A1 &arg)
    {
        for (int i = 0 ; i < m_size ; i++)
            (m_array[i]->*func)(arg);
    }
    template <typename A1, typename A2, typename X1, typename X2>
    void doAcross (void (T::*func) (X1, X2), const A1 &arg1, const A2 &arg2)
    {
        for (int i = 0 ; i < m_size ; i++)
            (m_array[i]->*func)(arg1, arg2);
    }
    template <typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
    void doAcross (void (T::*func) (X1, X2, X3), const A1 &arg1, const A2 &arg2, const A3 &arg3)
    {
        for (int i = 0 ; i < m_size ; i++)
            (m_array[i]->*func)(arg1, arg2, arg3);
    }
    template <typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4> 
    void doAcross (void (T::*func) (X1, X2, X3, X4), const A1 &arg1, const A2 &arg2, const A3 &arg3, const A4 &arg4)
    {
        for (int i = 0 ; i < m_size ; i++)
            (m_array[i]->*func)(arg1, arg2, arg3, arg4);
    }

private:
    T_ptr *m_array;
    int m_size;
    int m_sizeX;
    int m_sizeY;
    int m_sizeZ;
    int m_dimension; // 1, 2, or 3

    // By default, elements of arrays have names of the form
    //      <parent>.<element_name>(id)
    // If the array is named (i.e. m_componentName is set), then the elements are named
    //      <parent>.<array_name>(id)
    // If m_names is set, then this array provides individual element
    // names that replace <element_name>(id).
    const char *m_componentName;
    const char **m_names;
};

BEGIN_NAMESPACE_CASCADE

// Generic array used to handle arrays in interface descriptors.
struct DummyArrayEntry
{
    static void addInterfaceArrayEntry (void *, const char *) {}
    static const Cascade::Hierarchy::Type hierarchyType = Cascade::Hierarchy::COMPONENT;
    static InterfaceDescriptor *getInterfaceDescriptorStatic () { return NULL; }
};
typedef ::Array<Cascade::DummyArrayEntry> GenericArray;

// Helper structure used to allocate array elements passing their index
// to their constructor
struct ArrayIndexAllocator
{
    operator const ArrayIndexAllocator * () const
    {
        return this;
    }

    template <typename T>
    inline void allocateElement (T *&element, int x) const
    {
        element = new T(x);
    }

    template <typename T>
    inline void allocateElement (T *&element, int x, int y) const
    {
        element = new T(x, y);
    }

    template <typename T>
    inline void allocateElement (T *&element, int x, int y, int z) const
    {
        element = new T(x, y, z);
    }
};

// Supply this macro as the last argument to an array constructor to 
// construct the array elements by passing their index to their constructor.
#define ARRAY_INDEX_ALLOCATOR (const Cascade::ArrayIndexAllocator *) Cascade::ArrayIndexAllocator()

// Helper structure used to allocate array elements passing a fixed argument
// to their constructor
template <typename A>
struct ArrayArgAllocator
{
    DECLARE_NOCOPY(ArrayArgAllocator);
public:

    ArrayArgAllocator (const A &_arg) : arg(_arg) {}

    operator const ArrayArgAllocator * () const
    {
        return this;
    }
    
    template <typename T>
    inline void allocateElement (T *&element, int) const 
    {
        element = new T(arg);
    }

    template <typename T>
    inline void allocateElement (T *&element, int, int) const 
    {
        element = new T(arg);
    }

    template <typename T>
    inline void allocateElement (T *&element, int, int, int) const 
    {
        element = new T(arg);
    }

    const A &arg;
};

// Supply this macro as the last argument to an array constructor to
// construct the array elements by passing a fixed argument to their constructor.
#define ARRAY_ARG_ALLOCATOR(type,value) (const Cascade::ArrayArgAllocator< type > *) Cascade::ArrayArgAllocator< type >(value)

END_NAMESPACE_CASCADE

#undef ARRAY_ELEMENT

#endif

