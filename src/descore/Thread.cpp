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
// Thread.cpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/17/2011
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Thread.hpp"
#include "Trace.hpp"

#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#ifndef _GC
#include <sched.h>
#endif
#endif

BEGIN_NAMESPACE_DESCORE

static Thread g_mainThread;
static IThreadLocalDataType *g_threadLocalDataTypes = NULL;
static IThreadLocalDataType **g_threadLocalDataTypesTail = &g_threadLocalDataTypes;
__thread std::vector<IThreadLocalDataType *> *t_threadLocalTypesToDestruct;
__thread bool t_creatingThreadLocalData = false;
__thread bool t_destroyingThreadLocalData = false;
static __thread Thread *t_self = NULL;

Thread *Thread::self ()
{
    return t_self ? t_self : &g_mainThread;
}

////////////////////////////////////////////////////////////////////////////////
//
// Global mutexes
//
////////////////////////////////////////////////////////////////////////////////
Mutex &smartPtrMutex ()
{
    static Mutex mutex;
    return mutex;
}

Mutex &errorMutex ()
{
    static Mutex mutex;
    return mutex;
}

static void initGlobalMutexes ()
{
    static bool initialized = false;
    if (!initialized)
    {
        smartPtrMutex();
        errorMutex();
        initialized = true;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Error helper
//
////////////////////////////////////////////////////////////////////////////////
static string getError (int error)
{
#ifdef _MSC_VER
    char msg[256] = {0};
    error = error;

    ::FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        ::GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &msg,
        256, 
        NULL );

    string ret(msg);
    return ret;
#else
    return strerror(error);
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// Thread()
//
////////////////////////////////////////////////////////////////////////////////
Thread::Thread () 
  : m_stackSize(0), 
    m_handle(NULL), 
    m_entryPoint(NULL), 
    m_running(false), 
    m_error(NULL),
    m_waitOnError(false)
{
    initGlobalMutexes();
}

////////////////////////////////////////////////////////////////////////////////
//
// setStackSize()
//
////////////////////////////////////////////////////////////////////////////////
void Thread::setStackSize (int sizeInBytes)
{
    assert(sizeInBytes > 0);
    assert(!m_running, "Cannot set stack size: thread has already been started");
    m_stackSize = sizeInBytes;
}

////////////////////////////////////////////////////////////////////////////////
//
// waitOnError()
//
////////////////////////////////////////////////////////////////////////////////
void Thread::waitOnError ()
{
    m_waitOnError = true;
}

////////////////////////////////////////////////////////////////////////////////
//
// ~Thread()
//
////////////////////////////////////////////////////////////////////////////////
Thread::~Thread ()
{
    if (m_handle && (!g_error || m_waitOnError))
        wait();
}

////////////////////////////////////////////////////////////////////////////////
//
// wait()
//
////////////////////////////////////////////////////////////////////////////////
struct CleanupError
{
    DECLARE_NOCOPY(CleanupError);
public:
    CleanupError (runtime_error * volatile &_err) : err(_err) {}
    ~CleanupError ()
    {
        err->handled();
        delete err;
        err = NULL;
    }

    runtime_error * volatile &err;
};

void Thread::wait ()
{
    assert(m_handle, "Thread has not been started");
    if (m_running)
    {
    #ifdef _MSC_VER
        int error = (::WaitForSingleObject(m_handle, INFINITE) == DWORD(-1));
    #else
        int error = pthread_join((pthread_t) (intptr_t) m_handle, NULL);
    #endif
        assert(!m_running, "Failed to wait for thread: %s", *getError(error));
    }
#ifdef _MSC_VER
    BOOL ret = ::CloseHandle(m_handle);
    assert(ret, "Failed to close thread handle: %s", *getError(0));
#endif
    m_handle = NULL;
    delete m_entryPoint;
    if (m_error)
    {
        CleanupError cleanup(m_error);
        if (!g_error)
            m_error->rethrow();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// running()
//
////////////////////////////////////////////////////////////////////////////////
bool Thread::running () const
{
    return m_running;
}

////////////////////////////////////////////////////////////////////////////////
//
// check_and_rethrow_errors()
//
////////////////////////////////////////////////////////////////////////////////
void Thread::check_and_rethrow_errors () {
    if (m_error)
    {
        CleanupError cleanup(m_error);
        if (!g_error)
            m_error->rethrow();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// startThread()
//
////////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
typedef int thread_ret_t;
#else
typedef void *thread_ret_t;
#endif

// In Log.cpp
void createLogBuffers ();

static thread_ret_t beginThread (void *thread)
{
    ((Thread *) thread)->runThread();
    return 0;
}

void Thread::startThread (IThreadFunction *entryPoint)
{
    // Make sure the main thread creates the log buffers
    createLogBuffers();

    assert(!m_handle, "Thread has already been started");
    m_entryPoint = entryPoint;
    m_running = true;

    // Capture global state
    m_globalAssertionContext = setGlobalAssertionContext(NULL);
    setGlobalAssertionContext(m_globalAssertionContext);
    m_globalTraceContext = getTraceContext();
    m_tracer = t_tracer;

#ifdef _MSC_VER
    m_handle = ::CreateThread(NULL, m_stackSize, (LPTHREAD_START_ROUTINE) &beginThread, this, 0, NULL);
    int error = (m_handle == NULL);
#else
    pthread_t id;
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    assert(!error, "Failed to obtain thread attributes: %s", *getError(error));
    if (m_stackSize)
    {
        error = pthread_attr_setstacksize(&attr, m_stackSize);
        assert(!error, "Failed to set stack size: %s", *getError(error));
    }
    error = pthread_create(&id, &attr, &beginThread, this);
    m_handle = (void *) (intptr_t) id;
#endif

    assert(!error, "Failed to create thread: %s", *getError(error));
}

struct ThreadLocalObjects
{
    ThreadLocalObjects ()
    {
        t_creatingThreadLocalData = true;
        t_threadLocalTypesToDestruct = new std::vector<IThreadLocalDataType *>;

        // Make sure the log buffers are created first so that if other constructors 
        // fail we can log the error.
        createLogBuffers();
        for (IThreadLocalDataType *t = g_threadLocalDataTypes ; t ; t = t->next)
            t->createObjects();
        t_creatingThreadLocalData = false;
    }
    ~ThreadLocalObjects()
    {
        t_destroyingThreadLocalData = true;
        for (int i = t_threadLocalTypesToDestruct->size() - 1 ; i >= 0 ; i--)
            (*t_threadLocalTypesToDestruct)[i]->deleteObjects();
        delete t_threadLocalTypesToDestruct;
    }
};

struct thread_exit {};

void Thread::runThread ()
{
    // The outer try block is specifically to catch errors in the construction
    // or destruction of thread-local objects.  Because logging depends on 
    // thread-local objects, we can't use logerr() for these errors.
    runtime_error *threadLocalObjectError = NULL;
    const char *state = "construction";
    try
    {
        t_self = this;
        ThreadLocalObjects objs;
        state = "destruction";

        try
        {
            // Set global state
            setGlobalAssertionContext(m_globalAssertionContext);
            t_globalTraceContext = m_globalTraceContext;
            t_globalTraceKeys = computeTraceKeys(m_globalTraceContext);
            t_tracer = m_tracer;

            m_entryPoint->startThread();
        }
        catch (runtime_error &e)
        {
            m_error = e.clone();
        }
        catch (std::exception &e)
        {
            m_error = runtime_error("Error: %s", e.what()).clone();
        }
        catch (thread_exit &)
        {
        }
        catch (...)
        {
            m_error = runtime_error("Error: Unknown exception").clone();
        }
        if (m_error)
        {
            const char *what = m_error->what();
            logerr("%s", what);
            if (what[strlen(what)-1] != '\n')
                logerr("\n");
        }
    }
    catch (runtime_error &e)
    {
        threadLocalObjectError = e.clone();
    }
    catch (std::exception &e)
    {
        threadLocalObjectError = runtime_error("Error: %s", e.what()).clone();
    }
    catch (thread_exit &)
    {
        threadLocalObjectError = runtime_error("Error: Thread::exit() called during %s of thread-local objects", state).clone();
    }
    catch (...)
    {
        threadLocalObjectError = runtime_error("Error: Unknown exception").clone();
    }
    if (threadLocalObjectError)
    {
        fprintf(stderr, "Error during %s of thread-local objects\n    %s", state, threadLocalObjectError->what());
        if (!m_error)
            m_error = threadLocalObjectError;
        else
            delete threadLocalObjectError;
    }
    if (m_error)
        m_error->m_handled = false;
    m_running = false;
}

////////////////////////////////////////////////////////////////////////////////
//
// sleep()
//
////////////////////////////////////////////////////////////////////////////////
void Thread::sleep (int milliseconds)
{
#ifdef _MSC_VER
    ::Sleep(milliseconds);
#else
    struct timespec tm;
    tm.tv_nsec = (milliseconds % 1000) * 1000000;
    tm.tv_sec = milliseconds / 1000;
    nanosleep(&tm, NULL);
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// yield()
//
////////////////////////////////////////////////////////////////////////////////
void Thread::yield ()
{
#ifdef _MSC_VER
    ::Sleep(0); //FIXME: Is this right?
#else
    #ifndef _GC
    sched_yield();
    #endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// exit()
//
////////////////////////////////////////////////////////////////////////////////
void Thread::exit ()
{
    assert_always(!isMainThread(), "descore::Thread::exit() cannot be called from the main thread");
    throw thread_exit();
}

////////////////////////////////////////////////////////////////////////////////
//
// isMainThread()
//
////////////////////////////////////////////////////////////////////////////////
bool Thread::isMainThread ()
{
    return !t_self || (t_self == &g_mainThread);
}

////////////////////////////////////////////////////////////////////////////////
//
// Synchronization
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _MSC_VER
#ifdef __x86_64__
static int interlocked_add (volatile int *value, int amount)
{
    int ret;
    
    __asm__ __volatile__
        (
            "lock xadd %1, %0":
            "+m"(*value), "=r"(ret): // outputs
            "1"(amount):             // inputs
            "memory", "cc"           // clobbers
            );
    
    return ret + amount;
}
#endif
#endif

int atomicIncrement (volatile int &value)
{
#ifdef _MSC_VER
    return InterlockedIncrement((volatile LONG *) &value);
#else
    return interlocked_add(&value, 1);
#endif
}

int atomicDecrement (volatile int &value)
{
#ifdef _MSC_VER
    return InterlockedDecrement((volatile LONG *) &value);
#else
    return interlocked_add(&value, -1);
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// Mutex
//
////////////////////////////////////////////////////////////////////////////////
Mutex::Mutex() 
{
#ifndef _MSC_VER
    m_mutex_internal = (void*)(new pthread_mutex_t);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init((pthread_mutex_t*)m_mutex_internal, &attr);
#else
    CRITICAL_SECTION *cs = new CRITICAL_SECTION;
    InitializeCriticalSection(cs);
    m_mutex_internal = cs;
#endif
}

Mutex::~Mutex() throw()
{
#ifndef _MSC_VER
    pthread_mutex_destroy((pthread_mutex_t*)m_mutex_internal);
    delete ((pthread_mutex_t*)m_mutex_internal);
#else
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *) m_mutex_internal; 
    DeleteCriticalSection(cs);
    delete cs;
#endif    
}

void Mutex::lock() 
{
#ifndef _MSC_VER
    pthread_mutex_lock( (pthread_mutex_t*)m_mutex_internal );
#else
    EnterCriticalSection((CRITICAL_SECTION*) m_mutex_internal);
#endif    
}

void Mutex::unlock() 
{
#ifndef _MSC_VER
    pthread_mutex_unlock( (pthread_mutex_t*)m_mutex_internal );
#else
    LeaveCriticalSection((CRITICAL_SECTION*) m_mutex_internal);
#endif    
}

////////////////////////////////////////////////////////////////////////////////
//
// ThreadLocalDataInstanceIds
//
////////////////////////////////////////////////////////////////////////////////
ThreadLocalDataInstances::ThreadLocalDataInstances () : m_firstFreeId(-1)
{
}

int ThreadLocalDataInstances::allocId (ThreadLocalInstanceAllocator *alloc)
{
    ScopedLock lock(m_mutex);
    if (m_firstFreeId >= 0)
    {
        int ret = m_firstFreeId;
        m_firstFreeId = m_ids[ret];
        m_allocators[ret] = alloc;
        return ret;
    }
    m_ids.push_back(0);
    m_allocators.push_back(alloc);
    return m_ids.size() - 1;
}

void ThreadLocalDataInstances::freeId (int id)
{
    assert((unsigned) id < m_ids.size());
    assert(m_allocators[id]);
    ScopedLock lock(m_mutex);
    m_ids[id] = m_firstFreeId;
    m_firstFreeId = id;
    delete m_allocators[id];
    m_allocators[id] = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// IThreadLocalDataType
//
////////////////////////////////////////////////////////////////////////////////
IThreadLocalDataType::IThreadLocalDataType () : next(NULL)
{
    *g_threadLocalDataTypesTail = this;
    g_threadLocalDataTypesTail = &next;
}

////////////////////////////////////////////////////////////////////////////////
//
// numProcessors()
//
////////////////////////////////////////////////////////////////////////////////
int numProcessors ()
{
#ifndef _MSC_VER
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#endif
}

END_NAMESPACE_DESCORE
