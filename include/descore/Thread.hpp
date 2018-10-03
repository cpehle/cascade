/*
Copyright 2011, D. E. Shaw Research.
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

////////////////////////////////////////////////////////////////////////////////
//
// Thread.hpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/16/2011
//
// Portable wrapper for a thread.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef Thread_hpp_110416230059
#define Thread_hpp_110416230059

#include "descore.hpp"
#include "assert.hpp"
#include "ThreadFunction.hpp"
#include "Iterators.hpp"

BEGIN_NAMESPACE_DESCORE

class Tracer;

////////////////////////////////////////////////////////////////////////////////
//
// Thread class
//
////////////////////////////////////////////////////////////////////////////////
class Thread
{
    DECLARE_NOCOPY(Thread);
public:
    Thread ();

    // Destructor waits for the thread to exit
    ~Thread ();

    //----------------------------------
    // Thread attributes
    //----------------------------------
    void setStackSize (int sizeInBytes);
    void waitOnError ();

    //----------------------------------
    // Non-member function entry points
    //----------------------------------
    void start (void (*fn) ()) { startThread(new ThreadFunction0(fn)); }
    template <typename A1, typename X1>
    void start (void (*fn) (X1), const A1 &a1) { startThread(new ThreadFunction1<A1,X1>(fn, a1)); }
    template <typename A1, typename A2, typename X1, typename X2>
    void start (void (*fn) (X1, X2), const A1 &a1, const A2 &a2) { startThread(new ThreadFunction2<A1,A2,X1,X2>(fn, a1, a2)); }
    template <typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
    void start (void (*fn) (X1, X2, X3), const A1 &a1, const A2 &a2, const A3 &a3) { startThread(new ThreadFunction3<A1,A2,A3,X1,X2,X3>(fn, a1, a2, a3)); }
    template <typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4>
    void start (void (*fn) (X1, X2, X3, X4), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4) { startThread(new ThreadFunction4<A1,A2,A3,A4,X1,X2,X3,X4>(fn, a1, a2, a3, a4)); }
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename X1, typename X2, typename X3, typename X4, typename X5>
    void start (void (*fn) (X1, X2, X3, X4, X5), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5) { startThread(new ThreadFunction5<A1,A2,A3,A4,A5,X1,X2,X3,X4,X5>(fn, a1, a2, a3, a4, a5)); }
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6>
    void start (void (*fn) (X1, X2, X3, X4, X5, X6), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6) { startThread(new ThreadFunction6<A1,A2,A3,A4,A5,A6,X1,X2,X3,X4,X5,X6>(fn, a1, a2, a3, a4, a5, a6)); }
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7>
    void start (void (*fn) (X1, X2, X3, X4, X5, X6, X7), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7) { startThread(new ThreadFunction7<A1,A2,A3,A4,A5,A6,A7,X1,X2,X3,X4,X5,X6,X7>(fn, a1, a2, a3, a4, a5, a6, a7)); }
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7, typename X8>
    void start (void (*fn) (X1, X2, X3, X4, X5, X6, X7, X8), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8) { startThread(new ThreadFunction8<A1,A2,A3,A4,A5,A6,A7,A8,X1,X2,X3,X4,X5,X6,X7,X8>(fn, a1, a2, a3, a4, a5, a6, a7, a8)); }

    //----------------------------------
    // Member function entry points
    //----------------------------------
    template <typename T>
    void start (T *obj, void (T::*fn) ()) { startThread(new ThreadMemberFunction0<T>(obj, fn)); }
    template <typename T, typename A1, typename X1>
    void start (T *obj, void (T::*fn) (X1), const A1 &a1) { startThread(new ThreadMemberFunction1<T,A1,X1>(obj, fn, a1)); }
    template <typename T, typename A1, typename A2, typename X1, typename X2>
    void start (T *obj, void (T::*fn) (X1, X2), const A1 &a1, const A2 &a2) { startThread(new ThreadMemberFunction2<T,A1,A2,X1,X2>(obj, fn, a1, a2)); }
    template <typename T, typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
    void start (T *obj, void (T::*fn) (X1, X2, X3), const A1 &a1, const A2 &a2, const A3 &a3) { startThread(new ThreadMemberFunction3<T,A1,A2,A3,X1,X2,X3>(obj, fn, a1, a2, a3)); }
    template <typename T, typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4>
    void start (T *obj, void (T::*fn) (X1, X2, X3, X4), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4) { startThread(new ThreadMemberFunction4<T,A1,A2,A3,A4,X1,X2,X3,X4>(obj, fn, a1, a2, a3, a4)); }
    template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename X1, typename X2, typename X3, typename X4, typename X5>
    void start (T *obj, void (T::*fn) (X1, X2, X3, X4, X5), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5) { startThread(new ThreadMemberFunction5<T,A1,A2,A3,A4,A5,X1,X2,X3,X4,X5>(obj, fn, a1, a2, a3, a4, a5)); }
    template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6>
    void start (T *obj, void (T::*fn) (X1, X2, X3, X4, X5, X6), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6) { startThread(new ThreadMemberFunction6<T,A1,A2,A3,A4,A5,A6,X1,X2,X3,X4,X5,X6>(obj, fn, a1, a2, a3, a4, a5, a6)); }
    template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7>
    void start (T *obj, void (T::*fn) (X1, X2, X3, X4, X5, X6, X7), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7) { startThread(new ThreadMemberFunction7<T,A1,A2,A3,A4,A5,A6,A7,X1,X2,X3,X4,X5,X6,X7>(obj, fn, a1, a2, a3, a4, a5, a6, a7)); }
    template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6, typename X7, typename X8>
    void start (T *obj, void (T::*fn) (X1, X2, X3, X4, X5, X6, X7, X8), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8) { startThread(new ThreadMemberFunction8<T,A1,A2,A3,A4,A5,A6,A7,A8,X1,X2,X3,X4,X5,X6,X7,X8>(obj, fn, a1, a2, a3, a4, a5, a6, a7, a8)); }

    //----------------------------------
    // Thread functions
    //----------------------------------
    
    // Wait for the thread to exit
    void wait ();

    // Determine if the thread is still running
    bool running () const;

    // Check if the thread had an exception and rethrow that exception
    void check_and_rethrow_errors();

    //----------------------------------
    // Static functions called from threads
    //----------------------------------

    // Suspend the thread for a number of milliseconds
    static void sleep (int milliseconds);

    // Yield
    static void yield ();

    // Exit the thread
    static void exit ();

    // Return the current thread object 
    static Thread *self ();

    // Check to see if the current thread is the main thread
    static bool isMainThread ();

protected:
    // Create the actual thread
    void startThread (IThreadFunction *entryPoint);

public: 
    // Public so that it can be called from the static thread function
    void runThread ();

protected:
    int                      m_stackSize;  // Size in bytes of stack, or 0 for default size
    void                     *m_handle;
    IThreadFunction          *m_entryPoint;
    volatile bool            m_running;
    runtime_error            * volatile m_error;
    bool                     m_waitOnError;

    // Parent thread state inherited by child thread
    assertion_context_func_t m_globalAssertionContext;
    string                   m_globalTraceContext;
    Tracer                   *m_tracer;
};

////////////////////////////////////////////////////////////////////////////////
//
// Synchronization
//
////////////////////////////////////////////////////////////////////////////////

// Increment an integer and return the new value
int atomicIncrement (volatile int &value);

// Decrement an integer and return the new value
int atomicDecrement (volatile int &value);

////////////////////////////////////////////////////////////////////////////////
//
// Mutex
//
////////////////////////////////////////////////////////////////////////////////
class Mutex 
{
    DECLARE_NOCOPY(Mutex);
public:
    Mutex();
    ~Mutex() throw();
    void lock();
    void unlock();
private:
    void *m_mutex_internal;
};

// Global descore mutexes that are guaranteed to be valid at static
// initialization time.
Mutex &smartPtrMutex ();
Mutex &errorMutex ();

////////////////////////////////////////////////////////////////////////////////
//
// SpinLock
//
////////////////////////////////////////////////////////////////////////////////
class SpinLock
{
    DECLARE_NOCOPY(SpinLock);
public:
    SpinLock (const char *name = "SpinLock") : m_locked(0), m_numLocks(0), m_numSpins(0), m_name(name) {}
    ~SpinLock ()
    {
        // Enable to generate a spin lock contention report
        if (0)
            printf("%s: %d / %d\n", m_name, m_numSpins, m_numLocks);
    }
    inline void backoff(int spins_so_far) {
        //NOTE: The numbers 10 and 100 are not tuned, they are just guesses
        if (spins_so_far < 100) {
            //nop, keep spinning
        } else if (spins_so_far < 100) {
            Thread::yield();
        } else {
            Thread::sleep(1);
        }
        
    }
    inline void lock ()
    {
        if (atomicIncrement(m_locked) > 1)
        {
            atomicDecrement(m_locked);
            int spins_so_far = 0;
            while (atomicIncrement(m_locked) > 1) {
                atomicDecrement(m_locked);
                backoff(++spins_so_far);
            }
            m_numSpins++;
        }
        m_numLocks++;
    }
    inline void unlock ()
    {
        atomicDecrement(m_locked);
    }
private:
    volatile int m_locked;
    int m_numLocks;
    int m_numSpins;
    const char *m_name;
};

////////////////////////////////////////////////////////////////////////////////
//
// ScopedLock: Lock and unlock based on scoping
//
////////////////////////////////////////////////////////////////////////////////
template <typename TLock>
class TScopedLock
{
    DECLARE_NOCOPY(TScopedLock);
public:
    TScopedLock(TLock &lock) : m_lock(lock) 
    {
        m_lock.lock();
    }
    ~TScopedLock() 
    {
        m_lock.unlock();
    }
private:
    TLock &m_lock;
};

typedef TScopedLock<Mutex>    ScopedLock;
typedef TScopedLock<SpinLock> ScopedSpinLock;

////////////////////////////////////////////////////////////////////////////////
//
// Dynamic thread-local storage
//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------
// ThreadLocalInstanceAllocator
//----------------------------------
struct ThreadLocalInstanceAllocator
{
    virtual ~ThreadLocalInstanceAllocator () {}
    virtual void alloc (void *mem) const = 0;
    virtual ThreadLocalInstanceAllocator *copy () const = 0;
};

template <typename T>
struct ThreadLocalInstanceAllocatorNoArgs : public ThreadLocalInstanceAllocator
{
    virtual void alloc (void *mem) const
    {
        new (mem) T;
    }
    virtual ThreadLocalInstanceAllocator *copy () const
    {
        return new ThreadLocalInstanceAllocatorNoArgs;
    }
};

template <typename T, typename A1>
struct ThreadLocalInstanceAllocatorOneArg : public ThreadLocalInstanceAllocator
{
    ThreadLocalInstanceAllocatorOneArg (A1 _arg1) : arg1(_arg1) {}

    virtual void alloc (void *mem) const
    {
        new (mem) T(arg1);
    }
    virtual ThreadLocalInstanceAllocator *copy () const
    {
        return new ThreadLocalInstanceAllocatorOneArg(arg1);
    }

    A1 arg1;
};

template <typename T, typename A1, typename A2>
struct ThreadLocalInstanceAllocatorTwoArgs : public ThreadLocalInstanceAllocator
{
    ThreadLocalInstanceAllocatorTwoArgs (A1 _arg1, A2 _arg2) : arg1(_arg1), arg2(_arg2) {}

    virtual void alloc (void *mem) const
    {
        new (mem) T(arg1, arg2);
    }
    virtual ThreadLocalInstanceAllocator *copy () const
    {
        return new ThreadLocalInstanceAllocatorTwoArgs(arg1, arg2);
    }

    A1 arg1;
    A2 arg2;
};

template <typename T, typename A1, typename A2, typename A3>
struct ThreadLocalInstanceAllocatorThreeArgs : public ThreadLocalInstanceAllocator
{
    ThreadLocalInstanceAllocatorThreeArgs (A1 _arg1, A2 _arg2, A3 _arg3) : arg1(_arg1), arg2(_arg2), arg3(_arg3) {}

    virtual void alloc (void *mem) const
    {
        new (mem) T(arg1, arg2, arg3);
    }
    virtual ThreadLocalInstanceAllocator *copy () const
    {
        return new ThreadLocalInstanceAllocatorThreeArgs(arg1, arg2, arg3);
    }

    A1 arg1;
    A2 arg2;
    A3 arg3;
};

template <typename T, typename A1, typename A2, typename A3, typename A4>
struct ThreadLocalInstanceAllocatorFourArgs : public ThreadLocalInstanceAllocator
{
    ThreadLocalInstanceAllocatorFourArgs (A1 _arg1, A2 _arg2, A3 _arg3, A4 _arg4) : arg1(_arg1), arg2(_arg2), arg3(_arg3), arg4(_arg4) {}

    virtual void alloc (void *mem) const
    {
        new (mem) T(arg1, arg2, arg3, arg4);
    }
    virtual ThreadLocalInstanceAllocator *copy () const
    {
        return new ThreadLocalInstanceAllocatorFourArgs(arg1, arg2, arg3, arg4);
    }

    A1 arg1;
    A2 arg2;
    A3 arg3;
    A4 arg4;
};

//----------------------------------
// ThreadLocalDataInstances
//----------------------------------

// Helper class to manage the instances of a given type
class ThreadLocalDataInstances
{
public:
    ThreadLocalDataInstances ();

    // Allocate an instance with the specified constructor arguments, and return the instance ID
    template <typename T>
    int allocInstance ()
    {
        return allocId(new ThreadLocalInstanceAllocatorNoArgs<T>);
    }
    template <typename T, typename A>
    int allocInstance (A arg)
    {
        return allocId(new ThreadLocalInstanceAllocatorOneArg<T, A>(arg));
    }
    template <typename T, typename A1, typename A2>
    int allocInstance (A1 arg1, A2 arg2)
    {
        return allocId(new ThreadLocalInstanceAllocatorTwoArgs<T, A1, A2>(arg1, arg2));
    }
    template <typename T, typename A1, typename A2, typename A3>
    int allocInstance (A1 arg1, A2 arg2, A3 arg3)
    {
        return allocId(new ThreadLocalInstanceAllocatorThreeArgs<T, A1, A2, A3>(arg1, arg2, arg3));
    }
    template <typename T, typename A1, typename A2, typename A3, typename A4>
    int allocInstance (A1 arg1, A2 arg2, A3 arg3, A4 arg4)
    {
        return allocId(new ThreadLocalInstanceAllocatorFourArgs<T, A1, A2, A3, A4>(arg1, arg2, arg3, arg4));
    }

    // Create a copy of an instance with the same allocator but a new id
    int copyInstance (int id)
    {
        return allocId(m_allocators[id]->copy());
    }

    // Free an ID
    void freeId (int id);

    // Get the maximum ID
    inline int maxId () const
    {
        return m_ids.size();
    }

    // Return the allocator for the specified ID, or NULL if the ID is invalid
    inline ThreadLocalInstanceAllocator *getAllocator (int id) const
    {
        return m_allocators[id];
    }

private:
    int allocId (ThreadLocalInstanceAllocator *alloc);

private:
    // A non-negative number is a pointer to the next free id;
    // -1 indicates the last free id.
    std::vector<int> m_ids;
    int              m_firstFreeId;
    Mutex            m_mutex;
    std::vector<ThreadLocalInstanceAllocator *> m_allocators;
};

//----------------------------------
// ThreadLocalDataArray
//----------------------------------

// Thread-local array of objects of a given type
template <typename T>
class ThreadLocalDataArray
{
public:
    ThreadLocalDataArray (const ThreadLocalDataInstances *instances)
    {
        m_arraySize = instances->maxId();
        m_data = (T *) new byte[m_arraySize * sizeof(T)];
        m_valid = new bool[m_arraySize];
        for (int i = 0 ; i < m_arraySize ; i++)
        {
            ThreadLocalInstanceAllocator *alloc = instances->getAllocator(i);
            m_valid[i] = (alloc != NULL);
            if (m_valid[i])
                alloc->alloc(m_data + i);
        }
    }

    ~ThreadLocalDataArray ()
    {
        for (int i = m_arraySize - 1 ; i >= 0 ; i--)
        {
            if (m_valid[i])
                m_data[i].~T();
        }
        delete[] m_valid;
        delete[] (byte *) m_data;
    }

    inline bool isValid (int id)
    {
        assert(unsigned(id) < unsigned(m_arraySize));
        return m_valid[id];
    }

    inline T &getInstance (int id)
    {
        assert(unsigned(id) < unsigned(m_arraySize) && m_valid[id],
               "Cannot access thread-local data of type %s:\n    Data was not created by current or parent thread",
               type_traits<T>::name());
        return m_data[id];
    }

private:
    T    *m_data;
    bool *m_valid;
    int  m_arraySize;
};

//----------------------------------
// IThreadLocalDataType
//----------------------------------

// Interface used to keep all thread local data arrays in a global linked list
struct IThreadLocalDataType : public ThreadLocalDataInstances
{
    IThreadLocalDataType ();

    // Called from a new thread before it starts running to create the thread's data
    virtual void createObjects () = 0;

    // Called from a thread before it exits to delete its data
    virtual void deleteObjects () = 0;

    IThreadLocalDataType *next;
};

//----------------------------------
// ThreadLocalDataType
//----------------------------------

extern __thread std::vector<IThreadLocalDataType *> *t_threadLocalTypesToDestruct;
extern __thread bool t_creatingThreadLocalData;
extern __thread bool t_destroyingThreadLocalData;

// Manage the instances of a given type
template <typename T>
class ThreadLocalDataType : public IThreadLocalDataType
{
public:

    // Create all data of this type for a new thread
    virtual void createObjects ()
    {
        if (m_data)
            return;
        assert(!t_destroyingThreadLocalData, 
               "Cannot access thread-local data of type %s:\n    Data has already been deleted",
               type_traits<T>::name());
        assert(t_creatingThreadLocalData, 
               "Cannot access thread-local data of type %s:\n    Data was not created by current or parent thread",
               type_traits<T>::name());
        t_threadLocalTypesToDestruct->push_back(this);
        m_data = new ThreadLocalDataArray<T>(this);
        m_mutex.lock();
        m_arrays.insert(m_data);
        m_mutex.unlock();
    }

    // Delete all data of this type for a thread
    virtual void deleteObjects ()
    {
        assert(m_data);
        m_mutex.lock();
        m_arrays.erase(m_data);
        m_mutex.unlock();
        delete m_data;
        m_data = NULL;
    }

    // Get a specific instance
    inline T &getInstance (int id)
    {
        // It's possible for getInstance() to be called before createObjects() if another
        // thread-local object type references this object type in its constructor.
        if (!m_data)
            createObjects();
        return m_data->getInstance(id);
    }

    // Call a function on every instance
    void doAcross (int id, void (T::*f) ())
    {
        for (ThreadLocalDataArray<T> *data : m_arrays)
        {
            if (data->isValid(id))
                (data->getInstance(id).*f)();
        }
    }
    template <typename A1, typename X1>
    void doAcross (int id, void (T::*f) (X1), const A1 &a1)
    {
        for (ThreadLocalDataArray<T> *data : m_arrays)
        {
            if (data->isValid(id))
                (data->getInstance(id).*f)(a1);
        }
    }
    template <typename A1, typename A2, typename X1, typename X2>
    void doAcross (int id, void (T::*f) (X1, X2), const A1 &a1, const A2 &a2)
    {
        for (ThreadLocalDataArray<T> *data : m_arrays)
        {
            if (data->isValid(id))
                (data->getInstance(id).*f)(a1, a2);
        }
    }
    template <typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
    void doAcross (int id, void (T::*f) (X1, X2, X3), const A1 &a1, const A2 &a2, const A3 &a3)
    {
        for (ThreadLocalDataArray<T> *data : m_arrays)
        {
            if (data->isValid(id))
                (data->getInstance(id).*f)(a1, a2, a3);
        }
    }
    template <typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4>
    void doAcross (int id, void (T::*f) (X1, X2, X3, X4), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
    {
        for (ThreadLocalDataArray<T> *data : m_arrays)
        {
            if (data->isValid(id))
                (data->getInstance(id).*f)(a1, a2, a3, a4);
        }
    }

private:
    Mutex m_mutex; // Used to protect m_arrays
    static __thread ThreadLocalDataArray<T> *m_data;
    std::set<ThreadLocalDataArray<T> *, descore::allow_ptr<ThreadLocalDataArray<T> *> > m_arrays;
};

template <typename T> __thread ThreadLocalDataArray<T> *ThreadLocalDataType<T>::m_data = NULL;

//----------------------------------
// ThreadLocalData
//----------------------------------

// Actual type instantiated by a user
template <typename T>
class ThreadLocalData
{
public:
    // Construction/destruction
    ThreadLocalData () : m_owner(Thread::self()), m_ownerData(), m_instanceId(getType().template allocInstance<T>()) {}
    template <typename A1>
    ThreadLocalData (A1 arg1) : m_owner(Thread::self()), m_ownerData(arg1), m_instanceId(getType().template allocInstance<T>(arg1)) {}
    template <typename A1, typename A2>
    ThreadLocalData (A1 arg1, A2 arg2) : m_owner(Thread::self()), m_ownerData(arg1, arg2), m_instanceId(getType().template allocInstance<T>(arg1, arg2)) {}
    template <typename A1, typename A2, typename A3>
    ThreadLocalData (A1 arg1, A2 arg2, A3 arg3) : m_owner(Thread::self()), m_ownerData(arg1, arg2, arg3), m_instanceId(getType().template allocInstance<T>(arg1, arg2, arg3)) {}
    template <typename A1, typename A2, typename A3, typename A4>
    ThreadLocalData (A1 arg1, A2 arg2, A3 arg3, A4 arg4) : m_owner(Thread::self()), m_ownerData(arg1, arg2, arg3, arg4), m_instanceId(getType().template allocInstance<T>(arg1, arg2, arg3, arg4)) {}

    ThreadLocalData (const ThreadLocalData &rhs) : 
        m_owner(Thread::self()), 
        m_ownerData(rhs.m_ownerData),
        m_instanceId(getType().copyInstance(rhs.m_instanceId))
    {
    }
    ~ThreadLocalData ()
    {
        getType().freeId(m_instanceId);
    }

    // Assignment
    ThreadLocalData &operator= (const ThreadLocalData &rhs)
    {
        getInstance() = *rhs;
        return *this;
    }
    T &operator= (const T &rhs)
    {
        return getInstance() = rhs;
    }

    // Basic accessors
    operator T & ()
    {
        return getInstance();
    }
    operator const T & () const
    {
        return getInstance();
    }
    T *operator-> ()
    {
        return &getInstance();
    }
    const T *operator-> () const
    {
        return &getInstance();
    }
    T &operator* ()
    {
        return getInstance();
    }
    const T &operator* () const
    {
        return getInstance();
    }

    // Call a function on every instance
    void doAcross (void (T::*f) ())
    {
        (m_ownerData.*f)();
        getType().doAcross(m_instanceId, f);
    }
    template <typename A1, typename X1>
    void doAcross (void (T::*f) (X1), const A1 &a1)
    {
        (m_ownerData.*f)(a1);
        getType().doAcross(m_instanceId, f, a1);
    }
    template <typename A1, typename A2, typename X1, typename X2>
    void doAcross (void (T::*f) (X1, X2), const A1 &a1, const A2 &a2)
    {
        (m_ownerData.*f)(a1, a2);
        getType().doAcross(m_instanceId, f, a1, a2);
    }
    template <typename A1, typename A2, typename A3, typename X1, typename X2, typename X3>
    void doAcross (void (T::*f) (X1, X2, X3), const A1 &a1, const A2 &a2, const A3 &a3)
    {
        (m_ownerData.*f)(a1, a2, a3);
        getType().doAcross(m_instanceId, f, a1, a2, a3);
    }
    template <typename A1, typename A2, typename A3, typename A4, typename X1, typename X2, typename X3, typename X4>
    void doAcross (void (T::*f) (X1, X2, X3, X4), const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
    {
        (m_ownerData.*f)(a1, a2, a3, a4);
        getType().doAcross(m_instanceId, f, a1, a2, a3, a4);
    }

private:
    inline const T &getInstance () const
    {
        return (Thread::self() == m_owner) ? m_ownerData : getType().getInstance(m_instanceId);
    }
    inline T &getInstance ()
    {
        return (Thread::self() == m_owner) ? m_ownerData : getType().getInstance(m_instanceId);
    }

    static ThreadLocalDataType<T> &getType ()
    {
        static ThreadLocalDataType<T> *dataType = new ThreadLocalDataType<T>;
        return *dataType;
    }

private:
    Thread *m_owner;     // Pointer to the thread that created this data
    T      m_ownerData;  // Object used by the owner thread
    int    m_instanceId; // Index of data within array for child threads
};

////////////////////////////////////////////////////////////////////////////////
//
// Determine the number of processors
//
////////////////////////////////////////////////////////////////////////////////
int numProcessors ();


END_NAMESPACE_DESCORE


#endif // #ifndef Thread_hpp_110416230059

