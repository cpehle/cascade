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
// assert.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "StringBuffer.hpp"
#include "Wildcard.hpp"
#include "AssertParams.hpp"
#include "Iterators.hpp"
#include "assert.hpp"
#include "Thread.hpp"

BEGIN_NAMESPACE_DESCORE
static __thread assertion_context_func_t t_globalAssertionContext = NULL;
END_NAMESPACE_DESCORE

/////////////////////////////////////////////////////////////////
//
// getAssertionContext()
//
/////////////////////////////////////////////////////////////////
string getAssertionContext ()
{
    if (descore::t_globalAssertionContext)
        return descore::t_globalAssertionContext();
    return "";
}

BEGIN_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// setGlobalAssertionContext()
//
////////////////////////////////////////////////////////////////////////////////
assertion_context_func_t setGlobalAssertionContext (assertion_context_func_t fn)
{
    assertion_context_func_t ret = t_globalAssertionContext;
    t_globalAssertionContext = fn;
    return ret;
}

// warning count
static volatile int g_numWarnings = 0;

// first error in linked list
runtime_error *g_error = NULL;

// Parameters
DeclareParameterGroup(assertParams);

////////////////////////////////////////////////////////////////////////////////
//
// Breakpoints
//
////////////////////////////////////////////////////////////////////////////////

// Cause a breakpoint if we're inside a debugger; do nothing otherwise.
// If breaking here is undesirable (e.g. if there are a lot of assertion
// failures that are expected and are being caught), then use the 
// debugger to set enable_breakpoint to 0.
#ifdef _DEBUG

static bool enable_breakpoint = true;

bool debug_breakpoint_enabled ()
{
    return enable_breakpoint && !assertParams.disableDebugBreakpoint;
}
void enable_debug_breakpoint ()
{
    assertParams.disableDebugBreakpoint = false;
    enable_breakpoint = true;
}
void disable_debug_breakpoint ()
{
    assertParams.disableDebugBreakpoint = true;
    enable_breakpoint = false;
}

#ifdef _MSC_VER
#include <excpt.h>
void debug_breakpoint ()
{
    if (descore::debug_breakpoint_enabled())                            
    {                                                                   
        __try                                                           
        {                                                               
            __debugbreak();                                             
        }                                                               
        __except (EXCEPTION_EXECUTE_HANDLER)                            
        {                                                               
        }
    }
}

#else // !_MSC_VER

void catch_debug_breakpoint_sigtrap (int)
{
}

#endif // _MSC_VER

#endif // _DEBUG

////////////////////////////////////////////////////////////////////////////////
//
// runtime_error
//
////////////////////////////////////////////////////////////////////////////////

__thread va_list runtime_error::temp_args;

//----------------------------------
// Constructors
//----------------------------------
runtime_error::runtime_error (const char *file, int line, const char *function, const string &context, 
                              const char *exp, const string &message, bool fatal)
    : std::runtime_error(""),
      m_file(file),
      m_line(line),
      m_function(function),
      m_context(context),
      m_exp(exp),
      m_message(message),
      m_hasFullContext(true),
      m_handled(false),
      m_next(NULL)
{
    strbuff fullMessage;
    if (**context)
        fullMessage.append(fatal ? "Error: %s\n" : "Warning: %s\n", *context);
    if (**message)
    {
        if (!strncmp(*message, "Error: ", 7))
            fullMessage.append("%s", *message);
        else if (fatal)
            fullMessage.append("Error: %s", *message);
        else
            fullMessage.append("Warning: %s", *message);
        if (fullMessage[strlen(fullMessage)-1] != '\n')
            fullMessage.append("\n");
    }
    else
        fullMessage.append("Assertion failed: %s\n", exp);
    fullMessage.append("%s:%d: %s\n", file, line, function);
    m_what = fullMessage;
    if (init() && fatal)
        debug_breakpoint();
}

runtime_error::runtime_error (const char *message, ...)
    : std::runtime_error(""),
      m_file(""),
      m_line(0),
      m_function(""),
      m_context(""),
      m_exp(""),
      m_hasFullContext(false),
      m_handled(false),
      m_next(NULL)
{
    va_list args;
    va_start(args, message);
    m_message = m_what = vstr(message, args);
    va_end(args);
    if (init())
        debug_breakpoint();
}

runtime_error::runtime_error (const string &message)
  : std::runtime_error(""),
    m_file(""),
    m_line(0),
    m_function(""),
    m_context(""),
    m_exp(""),
    m_message(message),
    m_what(message),
    m_hasFullContext(false),
    m_handled(false),
    m_next(NULL)
{
    if (init())
        debug_breakpoint();
}

bool runtime_error::init ()
{
    setErrorHook(NULL)(*this);
    if (isDisabled())
    {
        m_handled = true;
        return false;
    }

    descore::ScopedLock lock(errorMutex());
    m_next = g_error;
    g_error = this;
    return true;
}

runtime_error::runtime_error (const runtime_error &rhs, bool cloning) : std::runtime_error("")
{
    copy(rhs);

    if (cloning)
    {
        m_handled = true;
        rhs.m_handled = true;
    }
    else
    {
        descore::ScopedLock lock(errorMutex());
        m_next = g_error;
        g_error = this;
    }
}

runtime_error::runtime_error (const runtime_error &rhs) : std::runtime_error("")
{
    copy(rhs);
    logerr("Warning: runtime_error copy constructor invoked.  "
        "Consider using 'catch (error_type &)' and 'throw' without arguments.\n");

    descore::ScopedLock lock(errorMutex());
    m_next = g_error;
    g_error = this;
}

void runtime_error::copy (const runtime_error &rhs)
{
    m_file = rhs.m_file;
    m_line = rhs.m_line;
    m_function = rhs.m_function;
    m_context = rhs.m_context;
    m_exp = rhs.m_exp;
    m_message = rhs.m_message;
    m_what = rhs.m_what;
    m_handled = rhs.m_handled;
    m_hasFullContext = rhs.m_hasFullContext;
}

runtime_error *runtime_error::clone () const
{
    return new runtime_error(*this, true);
}

void runtime_error::rethrow () const
{
    throw runtime_error(*this, false);
}

//----------------------------------
// Destructor
//----------------------------------

runtime_error::~runtime_error () throw ()
{
    descore::ScopedLock lock(errorMutex());
    for (runtime_error **pperr = &g_error ; *pperr ; pperr = &((*pperr)->m_next))
    {
        if (*pperr == this)
        {
            *pperr = m_next;
            break;
        }
    }

    // If we're the last copy of the error and we haven't been handled, then
    // report & abort
    if (m_handled)
        return;
    runtime_error *error;
    for (error = g_error ; error ; error = error->m_next)
    {
        if (error->m_what == m_what)
            return;
    }
    setLogQuietMode(false);
    logerr("\n**** Unhandled descore::runtime_error! Aborting. ****\n");
    reportFatal();
    abort();
}

//----------------------------------
// Error handling
//----------------------------------

void runtime_error::reportFatal () const
{
    const char *type = typeid(*this).name();
#ifndef _MSC_VER
    type = abi::__cxa_demangle(type, 0, 0, NULL);
#endif
    if (!m_hasFullContext)
        logerr("Encountered run-time error of type '%s':\n", type);
    logerr("%s\n", *m_what);
    handled();
    setFatalHook(NULL)(*this);
}

void runtime_error::reportAndExit (int exitStatus) const
{
    reportFatal();
    exit(exitStatus);
}

void runtime_error::handled () const
{
    descore::ScopedLock lock(errorMutex());
    m_handled = true;
    for (runtime_error *error = g_error ; error ; error = error->m_next)
    {
        if ((error == this) || (error->m_what == m_what))
            error->m_handled = true;
    }
}

//----------------------------------
// Disabled assertions
//----------------------------------

bool runtime_error::isDisabled () const
{
    const char *output = *m_what;
    std::vector<string> disabled = assertParams.disabledAssertions;
    for (const string &s : disabled)
    {
        if (wildcardFind(output, *s))
            return true;
    }
    return false;
}

//----------------------------------
// Append
//----------------------------------

void runtime_error::append (const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string s = vstr(msg, args);
    va_end(args);
    append(s);
}

void runtime_error::append (const string &msg)
{
    m_what += msg;
}

////////////////////////////////////////////////////////////////////////////////
//
// Termination handler
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _MSC_VER
#include <exception>
using namespace std;
#endif

static void (*s_default_termination_handler) () = NULL;
static bool s_initialized_termination_handler = false;

static void restoreDefaultHandlers();

static void termination_handler ()
{
    //If we throw a sigbus while doing our termination, don't try to handle that, it causes
    // problems (and hangs).
    restoreDefaultHandlers();
    if (g_error)
        g_error->reportAndExit();
    (*s_default_termination_handler)();    
}

static void initialize_termination_handler ()
{
    s_default_termination_handler = set_terminate(termination_handler);
    s_initialized_termination_handler = true;
}

struct InitializeTerminationHandler
{
    InitializeTerminationHandler ()
    {
        initialize_termination_handler();
    }
};

static InitializeTerminationHandler initializeTerminationHandler;

////////////////////////////////////////////////////////////////////////////////
//
// Segmentation fault
//
////////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER

#include <windows.h>
static int s_exception_code;
static DWORD s_exception_pc;

#define _EXCEPTION(x) if (code == x) return #x

static const char *str_exception (int code)
{
    _EXCEPTION(EXCEPTION_ACCESS_VIOLATION);
    _EXCEPTION(EXCEPTION_DATATYPE_MISALIGNMENT);
    _EXCEPTION(EXCEPTION_BREAKPOINT);
    _EXCEPTION(EXCEPTION_SINGLE_STEP);
    _EXCEPTION(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
    _EXCEPTION(EXCEPTION_FLT_DENORMAL_OPERAND);
    _EXCEPTION(EXCEPTION_FLT_DIVIDE_BY_ZERO);
    _EXCEPTION(EXCEPTION_FLT_INEXACT_RESULT);
    _EXCEPTION(EXCEPTION_FLT_INVALID_OPERATION);
    _EXCEPTION(EXCEPTION_FLT_OVERFLOW);
    _EXCEPTION(EXCEPTION_FLT_STACK_CHECK);
    _EXCEPTION(EXCEPTION_FLT_UNDERFLOW);
    _EXCEPTION(EXCEPTION_INT_DIVIDE_BY_ZERO);
    _EXCEPTION(EXCEPTION_INT_OVERFLOW);
    _EXCEPTION(EXCEPTION_PRIV_INSTRUCTION);
    _EXCEPTION(EXCEPTION_IN_PAGE_ERROR);
    _EXCEPTION(EXCEPTION_ILLEGAL_INSTRUCTION);
    _EXCEPTION(EXCEPTION_NONCONTINUABLE_EXCEPTION);
    _EXCEPTION(EXCEPTION_STACK_OVERFLOW);
    _EXCEPTION(EXCEPTION_INVALID_DISPOSITION);
    _EXCEPTION(EXCEPTION_GUARD_PAGE);
    _EXCEPTION(EXCEPTION_INVALID_HANDLE);
    return NULL;
}

static int get_exception_information (unsigned int code, struct _EXCEPTION_POINTERS *ep) 
{ 
    s_exception_code = code;
    s_exception_pc = ep->ContextRecord->Eip;
    return str_exception(code) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
} 

void tryFunction (void (*fn) ())
{
    __try
    {
        fn();
    }
    __except(get_exception_information(GetExceptionCode(), GetExceptionInformation()))
    {
        setLogQuietMode(false);
        logerr("Exception %08x (%s) at 0x%p\n", s_exception_code, str_exception(s_exception_code), s_exception_pc);
        exit(s_exception_code);
    }
}   

static void restoreDefaultHandlers ()  
{
}

#else

#include <signal.h>

static void catch_segv (int)
{
    setLogQuietMode(false);
    logerr("catch_segv(): Segmentation fault\n");
    abort();
}
static void catch_bus (int)
{
    setLogQuietMode(false);
    logerr("catch_bus(): Bus error\n");
    abort();
}
static void catch_fpe (int)
{
    setLogQuietMode(false);
    logerr("catch_fpe(): Arithmetic exception\n");
    abort();
}

struct InitializeHandlers
{
    InitializeHandlers ()
    {
        signal(SIGSEGV, catch_segv);
        signal(SIGFPE, catch_fpe);
        signal(SIGBUS, catch_bus);
    }
} initializeHandler;

static void restoreDefaultHandlers ()  
{
    signal(SIGSEGV, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    if (s_default_termination_handler) {
        set_terminate(s_default_termination_handler); 
    }
}

void tryFunction (void (*fn) ())
{
    fn();
}

#endif

/////////////////////////////////////////////////////////////////
//
// error()
//
/////////////////////////////////////////////////////////////////
int error (const runtime_error &error, bool fatal)
{
    if (error.isDisabled())
    {
        error.handled();
        return 0;
    }

    if (!fatal)
    {
        logerr("%s", error.what());
        if (descore::atomicIncrement(g_numWarnings) > assertParams.maxWarnings)
            die("Maximum number of warnings (%d) exceeded", *assertParams.maxWarnings);
        return 0;
    }

    if (assertParams.abortOnError)
        error.reportAndExit();

    errorMutex().lock();
    if (!s_initialized_termination_handler)
        initialize_termination_handler();
    errorMutex().unlock();
    error.rethrow();
    return 0;
}

/////////////////////////////////////////////////////////////////
//
// setErrorHook
//
/////////////////////////////////////////////////////////////////
static void default_error_hook (runtime_error &)
{
}

ErrorHook setErrorHook (ErrorHook fn)
{
    descore::ScopedLock lock(errorMutex());
    static ErrorHook hook = &default_error_hook;

    ErrorHook ret = hook;
    if (fn)
        hook = fn;
    return ret;
}

/////////////////////////////////////////////////////////////////
//
// setFatalHook
//
/////////////////////////////////////////////////////////////////
static void default_fatal_hook (const runtime_error &)
{
}

FatalHook setFatalHook (FatalHook fn)
{
    descore::ScopedLock lock(errorMutex());
    static FatalHook hook = &default_fatal_hook;

    FatalHook ret = hook;
    if (fn)
        hook = fn;
    return ret;
}

/////////////////////////////////////////////////////////////////
//
// resetWarningCount()
//
/////////////////////////////////////////////////////////////////
void resetWarningCount ()
{
    g_numWarnings = 0;
}

END_NAMESPACE_DESCORE
