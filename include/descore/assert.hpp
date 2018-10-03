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
// assert.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// assert() replaces the standard assert macro:
//
//    assert(expected == 5);
//
// An error message with printf semantics can be supplied (and should
// whenever the assertion itself is not completely self-explanatory):
//
//    assert(size < maxSize, "The requested size (%d) exceeds the maximum (%d)", size, maxSize);
//
// die(...) fails unconditionally and is equivalent to assert(false, ...)
//
// warn(exp, ...) has the same format as assert but produces a non-fatal
// error unless the maximum number of warnings (default 5) is exceeded.
//
//////////////////////////////////////////////////////////////////////

// By default, always define assert because the standard <assert.h> is not #ifdef
// protected and will happily redefine assert if it is #included after this file.
// The fix is to always (re-)#include this file after <assert.h> is #included
// (possibly indirectly).
//
// In the case that some other library wants to define its own assert and 
// suppress the descore assert macro, it can define DISABLE_DESCORE_ASSERT.
//
#ifndef DISABLE_DESCORE_ASSERT
#undef assert
#ifdef _ASSERTOFF
#define assert _disabled_assertion
#else
#define assert assert_always
#endif
#endif

// Everything else
#ifndef descore_assert_hpp_
#define descore_assert_hpp_

#include "defines.hpp"
#include <cstdarg>
#include <stdexcept>
#include <string>
using std::string;

// Classes can implement this function to provide additional context information
// for assertions.  An implementation is defined at the global scope that by 
// default simply returns "".
string getAssertionContext ();

// Forward declaration
string vstr (const char *fmt, va_list args);

BEGIN_NAMESPACE_DESCORE

// Applications can modify the global assertion context by providing a function
// that returns a string.  setGlobalAssertionContext() specifies this function
// and returns the previous one (or NULL).
typedef string (*assertion_context_func_t) ();
assertion_context_func_t setGlobalAssertionContext (assertion_context_func_t fn);

class Thread;

////////////////////////////////////////////////////////////////////////////////
//
// Exceptions
//
////////////////////////////////////////////////////////////////////////////////

struct runtime_error : public std::runtime_error
{
    friend class descore::Thread;

    //----------------------------------
    // Constructor/destructor
    //----------------------------------
    runtime_error (const char *file, int line, const char *function, const string &context, 
                   const char *exp, const string &message, bool fatal);
    explicit runtime_error (const char *message, ...) ATTRIBUTE_PRINTF(2);
    explicit runtime_error (const string &message);
    runtime_error (const runtime_error &rhs, bool);
    runtime_error (const runtime_error &rhs);

    ~runtime_error () throw ();

    //----------------------------------
    // Basic accessors
    //----------------------------------
    const char *file () const
    {
        return m_file;
    }
    int line () const
    {
        return m_line;
    }
    const char *function () const
    {
        return m_function;
    }
    const char *context () const
    {
        return m_context.c_str();
    }
    const char *expression () const
    {
        return m_exp;
    }
    const char *message () const
    {
        return m_message.c_str();
    }
    virtual const char *what () const throw ()
    {
        handled();
        return m_what.c_str();
    }

    //----------------------------------
    // Cloning
    //----------------------------------
    virtual runtime_error *clone () const;
    virtual void rethrow () const;

    //----------------------------------
    // Functions
    //----------------------------------

    // Report a fatal error and call the fatal error hook
    void reportFatal () const;

    // Call report() and then exit with the specified status
    void reportAndExit (int exitStatus = -1) const;

    // Mark this error as handled to prevent it from self-reporting on
    // destruction if it hasn't already been reported
    void handled () const;

    // Append additional information to the what() string
    void append (const char *msg, ...) ATTRIBUTE_PRINTF(2);
    void append (const string &msg);

    // Returns true if the error is disabled based on its error message
    // and the parameter assert.disabledAssertions
    bool isDisabled () const;

private:
    bool init ();

protected:
    void copy (const runtime_error &e);

protected:

    // Error information
    const char *m_file;      // Name of file in which the error occurred
    int         m_line;      // Line number in file on which the error occurred
    const char *m_function;  // Pretty-printed signature of function in which the error occurred
    string      m_context;   // Context string associated with the error
    const char *m_exp;       // Expression that evaluated to false
    string      m_message;   // User-supplied message (or empty string if none supplied)
    string      m_what;      // Entire formatted error output string

    // Flag indicating that thiss error was constructed with full context information
    // (file, line, etc..).  When reporting an error without context information,
    // the type of the error class will be reported to provide additional information.
    bool m_hasFullContext;

    // Flag indicating that this error has been handled, so that we don't need to 
    // report it when the destructor is invoked.
    mutable bool m_handled;

    // Keep the runtime error objects in a linked list so that the termination
    // handler can access them.
    runtime_error *m_next;

    // Helper variables for constructing derived classes with varargs
    static __thread va_list temp_args;
};

// macro used to define a new exception class that derives from std::runtime_error,
// has a set of convenient constructors, and is compatible with the assertion infrastructure.
template <typename TBase = runtime_error>
struct ExceptionBaseType
{
    typedef TBase base_t;
}; 

#define IMPLEMENT_EXCEPTION(T, ...) \
    typedef descore::ExceptionBaseType<__VA_ARGS__>::base_t base_t; \
    T () : base_t(" ") {} \
    T (const char *file, int line, const char *function, const string &context, \
       const char *exp, const string &message, bool fatal) : \
        base_t(file, line, function, context, exp, message, fatal) {} \
    T (const T &e, bool cloning) : base_t(e, cloning) { copy(e); } \
    explicit T (const string &message) : base_t(message) {} \
    explicit T (const char *message, ...) ATTRIBUTE_PRINTF(2) : \
              base_t((va_start(runtime_error::temp_args, message), \
                      vstr(message, runtime_error::temp_args))) \
              { va_end(runtime_error::temp_args); } \
    ~T () throw () {}; \
    runtime_error *clone () const { return new T(*this, true); } \
    void rethrow () const { throw T(*this, false); }

#define DECLARE_EXCEPTION(T, ...) \
    struct T : public descore::ExceptionBaseType<__VA_ARGS__>::base_t   \
    { \
        IMPLEMENT_EXCEPTION(T, descore::ExceptionBaseType<__VA_ARGS__>::base_t); \
    }

////////////////////////////////////////////////////////////////////////////////
//
// Assertions
//
////////////////////////////////////////////////////////////////////////////////

// Actual function that gets called
int error (const runtime_error &error, bool fatal);

// Exception macros
#ifndef DISABLE_DESCORE_ASSERT

// Main error macro
#define descore_error(err, exp, context, fatal, msg) \
    (void) (ASSUME_FALSE(!(exp)) && descore::error(err(__FILE__, __LINE__, __FUNCSIG__, context, #exp, msg, fatal), fatal))

//----------------------------------
// Specific macros
//----------------------------------

// If a class defines getAssertionContext, then static versions of the
// assertion macros (s_*) must be used within static member functions.

#undef warn
#undef die

// Macros that will never be disabled 
#define assert_always(exp, ...)   descore_error(descore::runtime_error, exp, getAssertionContext(), true, str(__VA_ARGS__))
#define s_assert_always(exp, ...) descore_error(descore::runtime_error, exp, ::getAssertionContext(), true, str(__VA_ARGS__))

#define warn_always(exp, ...)   descore_error(descore::runtime_error, exp, getAssertionContext(), false, str(__VA_ARGS__))
#define s_warn_always(exp, ...) descore_error(descore::runtime_error, exp, ::getAssertionContext(), false, str(__VA_ARGS__))

#define assert_throw(err, exp, ...)   descore_error(err, exp, getAssertionContext(), true, str(__VA_ARGS__))
#define s_assert_throw(err, exp, ...) descore_error(err, exp, ::getAssertionContext(), true, str(__VA_ARGS__))

#define die(...)   descore_error(descore::runtime_error, false, getAssertionContext(), true, str(__VA_ARGS__))
#define s_die(...) descore_error(descore::runtime_error, false, ::getAssertionContext(), true, str(__VA_ARGS__))

// Macros disabled by _ASSERTOFF

#ifdef _ASSERTOFF
inline int _disabled_assertion_fn(...) { return 0; }
#define _disabled_assertion(...) (void) sizeof(descore::_disabled_assertion_fn(__VA_ARGS__))
#define s_assert _disabled_assertion
#define warn     _disabled_assertion
#define s_warn   _disabled_assertion
#else
// 'assert' defined at the top of this file
#define s_assert s_assert_always
#define warn     warn_always
#define s_warn   s_warn_always
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Functions
//
////////////////////////////////////////////////////////////////////////////////

// An optional callback function can be supplied to append additional output
// to an error.  The callback function will be invoked when an error is constructed;
// note that the error might still be caught and ignored within the code.
// setErrorHook(hook) installs the new error hook and returns the previous error hook.
typedef void (*ErrorHook) (runtime_error &error);
ErrorHook setErrorHook (ErrorHook fn);

// An optional callback function can be supplied to take some action after a fatal 
// error is reported.  The callback function will be invoked after the error message 
// has been logged.  setFatalHook(hook) installs the new fatal hook and returns the 
// previous fatal hook.
typedef void (*FatalHook) (const runtime_error &error);
FatalHook setFatalHook (FatalHook fn);

// Reset the warning count
void resetWarningCount ();

// Global pointer that can be checked to see if we're in an error condition
extern runtime_error *g_error;

// Call a function and, in MSVC builds, convert WIN32 exceptions (e.g. access violation) to runtime errors
void tryFunction (void (*fn) ());

#endif // #ifndef DISABLE_DESCORE_ASSERT

END_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// Breakpoints
//
////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

BEGIN_NAMESPACE_DESCORE
bool debug_breakpoint_enabled ();
void enable_debug_breakpoint ();
void disable_debug_breakpoint ();
END_NAMESPACE_DESCORE

#ifndef _MSC_VER

#include <signal.h>
BEGIN_NAMESPACE_DESCORE
void catch_debug_breakpoint_sigtrap (int);
END_NAMESPACE_DESCORE
#define debug_breakpoint()                                              \
    if (descore::debug_breakpoint_enabled())                            \
    {                                                                   \
        signal(SIGTRAP, descore::catch_debug_breakpoint_sigtrap);       \
        raise(SIGTRAP);                                                 \
    }

#endif // !_MSC_VER

#else // !_DEBUG

#define debug_breakpoint() ((void) 0)

#endif // _DEBUG

// The use of assert() str(), but the strcast header depends on things in this header,
// so this include needs to be at the end.
#include "strcast.hpp"

#endif // #ifndef descore_assert_hpp_

