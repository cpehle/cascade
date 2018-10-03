/*
Copyright 2010, D. E. Shaw Research.
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
// Trace.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/14/2010
//
// Support for conditional logging.
//
// +------------+
//  Sample usage
// +------------+
//
//   // In a header or source file
//   TraceKey(fft);
//
//   // Tracing using the declared fft key
//   trace(fft, "Starting %d-point FFT\n", fftSize);
//
//   // Anonymous tracing
//   trace("Starting client %s\n", client_name);
//
// +-------------------+
//  Controlling tracing
// +-------------------+
//
// From the command line:
//
//   -trace /fft                - Trace the "fft" key in the global context
//   -trace */fft               - Trace the "fft" key in all contexts
//   -trace worker0/fft*        - Trace all keys matching fft* in the context "worker0"
//   -trace /fft:butterfly.cpp  - Only trace the "fft" key defined in the file butterfly.cpp
//
// From code:
//
//   descore::setTrace("*", "fft");
//   descore::setTrace("", "fft:butterfly.cpp");
//   descore::unsetTrace("*", "fft");
// 
// +------------------------+
//  Defining a trace context
// +------------------------+
//
// There are three ways to define a trace context.
//
// (1) Inherit from descore::TraceContext.
//
// (2) Define a trace context in an object by implementing the functions
//
//   TraceKeys getTraceKeys () const;
//   const string &getTraceContext () const;
//
// The first function should return the TraceKeys value that was precomputed
// by calling descore::computeTraceKeys(context).  The TraceKeys value must be
// recomputed whenever the set of current traces is modified.  The second 
// function can also return a 'string' or a 'const char *', but it's more
// efficient to return a 'const string &' if possible.
//
// Using this method, all traces in the object's non-static member functions 
// will be in the object's trace context. 
//
// (3) Temporarily modify the global trace context.
//
// The global trace context can be modified by calling the functions
//
//   descore::setGlobalTraceContext(context, bool recomputeGlobalTraceKeys = true);
//   descore::setGlobalTraceKeys(traceKeys);
//
// The first function takes the string context name.  The second function takes a value that 
// must have been precomputed by calling descore::computeTraceKeys(context).  Each function 
// returns the previous state, which can be restored at a later point.  The second function
// is only necessary if the first one is called with recomputeGlobalTraceKeys = false; otherwise
// the trace keys will be recomputed based on the context (which can be expensive).
//
// Using this method, all traces in global or static functions will be in the
// specified context.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef Trace_hpp_100414100025
#define Trace_hpp_100414100025

#include "Thread.hpp"
#include "StringBuffer.hpp"
#include "Wildcard.hpp"

typedef uint16 TraceKeys;

BEGIN_NAMESPACE_DESCORE

class Tracer;
struct _TraceKey;
extern TraceKeys g_overloadedGroupMask;

////////////////////////////////////////////////////////////////////////////////
//
// Globals
//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------
// Tracing macros
//----------------------------------

/// Declare a trace key
#define TraceKey(name) static descore::_TraceKey name(#name, __FILE__)

/// Conditional tracing in the current context
#define trace(x, ...) \
    !descore::maybeTracing(getTraceKeys(), x) || \
    descore::beginTrace(getTraceKeys(),getTraceContext(), false, x, ##__VA_ARGS__)

/// Conditional tracing in the global context
#define s_trace(x, ...) \
    !descore::maybeTracing(::getTraceKeys(), x) || \
        descore::beginTrace(::getTraceKeys(),::getTraceContext(), false, x, ##__VA_ARGS__)

/// Conditional tracing in a specified object's context
#define obj_trace(o, x, ...) \
    !descore::maybeTracing(descore::static_dereference(o).getTraceKeys(), x) || \
        descore::beginTrace(descore::static_dereference(o).getTraceKeys(), descore::static_dereference(o).getTraceContext(), false, x, ##__VA_ARGS__)

/// Unconditional tracing in the current context
#define force_trace(x, ...) \
    descore::beginTrace(getTraceKeys(), getTraceContext(), true, x, ##__VA_ARGS__)

/// Unconditional tracing in the global context
#define s_force_trace(x, ...) \
    descore::beginTrace(::getTraceKeys(), ::getTraceContext(), true, x, ##__VA_ARGS__)

/// Unconditional tracing in a specified object's context
#define obj_force_trace(o, x, ...) \
    descore::beginTrace(descore::static_dereference(o).getTraceKeys(), descore::static_dereference(o).getTraceContext(), true, x, ##__VA_ARGS__)

/// Check to see if tracing is enabled in the current context.
#define is_tracing(...) \
    (descore::maybeTracing(getTraceKeys(), ##__VA_ARGS__) &&            \
     descore::isTracing(getTraceKeys(), getTraceContext(), ##__VA_ARGS__))

/// Check to see if tracing is enabled in the global context
#define s_is_tracing(...) \
    (descore::maybeTracing(::getTraceKeys(), ##__VA_ARGS__) &&          \
     descore::isTracing(::getTraceKeys(), ::getTraceContext(), ##__VA_ARGS__))

/// Check to see if tracing is enabled in a specified object's context
#define obj_is_tracing(o, ...) \
    (descore::maybeTracing(descore::static_dereference(o).getTraceKeys(), ##__VA_ARGS__) && \
     descore::isTracing(descore::static_dereference(o).getTraceKeys(), descore::static_dereference(o).getTraceContext(), ##__VA_ARGS__))

//----------------------------------
// Public functions
//----------------------------------

/// Enable/disable tracing within the specified contexts.  All arguments are
/// wildcard strings.  If keyname is NULL and <context> is a syntactically valid 
/// keyname (possibly with wildcards), then this will also enable/disable tracing 
/// for the key named <context>.
void setTrace (const char *context = "", const char *keyname = NULL, const char *filename = "*");
void unsetTrace (const char *context = "", const char *keyname = NULL, const char *filename = "*");

/// Enable tracing based on a specifier string.  The string is first expanded 
/// into multiple specifiers using expandSpecifierString().  Then, each 
/// specifier has the format
///
///     context[/key][:file]
///
/// An empty context string corresponds to the global context.  If 'key' is
/// omitted then it defaults to "" (anonymous tracing).  If 'file' is omitted
/// then it defaults to *.
///
void setTraces (const char *traces);

/// Helper function to expand a string into multiple specifiers as follows:
///
/// - top-level semicolons delimit separate strings
/// - anything contained within curly braces {} is itself expanded into a 
///   list of strings, then the curly braces are replaced with each string in 
///   this list.
///
/// For example, the string "1.{a;b}.{x;y};cat" expands to the list of strings
/// "1.a.x", "1.a.y", "1.b.x", "1.b.y", "cat".
///
std::vector<string> expandSpecifierString (const string &str);

/// Parse trace directives from the command line and remove them from the 
/// command-line arguments.  Also recognizes the arguments, which display
/// trace information and then exit:
///
///   -showtraces [name]   ls-style list of all matching trace keys
///   -showkeys   [name]   detailed list of matching trace keys including files
///
void parseTraces (int &csz, char *rgsz[]);

/// Pre-compute the set of keys being traced by a context.  This can then either 
/// be returned by an object's getTraceKeys() function, or it can be installed 
/// in the global context using setGlobalTraceKeys().
TraceKeys computeTraceKeys (const string &context);

/// Set the trace context name for global functions and return the previous
/// global trace context name.  If recomputeGlobalTraceKeys is true (default),
/// then the trace keys for the new context name will be computed.  Otherwise,
/// the programmer is responsible for separately calling setGlobalTraceKeys().
/// This can be advantageous when the trace keys corresponding to the context
/// name have previously been computed and stored, since recomputing the keys
/// from scratch is expensive.
string setGlobalTraceContext (const string &context, bool recomputeGlobalTraceKeys = true);

/// Specify the set of keys being traced in the global context, and return the 
/// previous set.
TraceKeys setGlobalTraceKeys (TraceKeys keys);

/// Set the trace object and return the previous trace object.
Tracer *setTracer (Tracer *tracer);

/// Set the log file for all matching trace keys
void setTraceLogFile (descore::LogFile f);
void setTraceLogFile (const char *keyname, descore::LogFile f);
void setTraceLogFile (const descore::_TraceKey &key, descore::LogFile f);

//----------------------------------
// Helper structures
//----------------------------------

/// Callback interface that automatically gets invoked whenever the set of 
/// traces is modified so that sets of trace keys can be recomputed.
struct ITraceCallback
{
    ITraceCallback ();
    ~ITraceCallback ();

    // Contexts is a wildcard string indicating the set of contexts that 
    // need to have their trace keys recomputed.
    virtual void notifyTrace (const char *context) = 0;
};

/// Base class that provides standard trace context functionality
class TraceContext : public ITraceCallback
{
public:
    TraceContext (const string &name = "")
    {
        setName(name);
    }
    void setName (const string &name)
    {
        m_name = name;
        m_traceKeys = descore::computeTraceKeys(name);
    }
    const string &getName () const
    {
        return m_name;
    }
    const string &getTraceContext () const
    {
        return m_name;
    }
    const string &getAssertionContext () const
    {
        return m_name;
    }
    TraceKeys getTraceKeys () const
    {
        return m_traceKeys;
    }
    void notifyTrace (const char *context)
    {
        if (wildcardMatch(context, m_name.c_str()))
            m_traceKeys = descore::computeTraceKeys(m_name);
    }

protected:
    string    m_name;
    TraceKeys m_traceKeys;
};

////////////////////////////////////////////////////////////////////////////////
//
// TraceContextSet
//
// Specify a set of contexts.  Do this in two ways:
//
// - An explicit set of contexts with no wildcard strings
// - An ordered list of inclusion/exclusion wildcard specifiers 
//
// A context is included in the set if:
//
// - it is in the explicit set, or
// - it matches at least one specifier in the orderd list, 
//   and the last specifier it matches is an "include" specifier.
//
////////////////////////////////////////////////////////////////////////////////
struct TraceContextSet
{
    void updateTrace (const string &context, bool include);
    bool isTracing (const string &context);

    struct ContextSpecifier
    {
        string context; // Wildcard string
        bool   include; // true for setTrace(), false for unsetTrace()
    };
    std::list<ContextSpecifier> specifiers;  // Stored in *reverse* order
    std::set<string>            contexts;
};

////////////////////////////////////////////////////////////////////////////////
//
// TraceGroup
//
// A trace group is a list of trace keys that all share the same trace context
// set.  Each trace group is assigned to a single bit in the trace mask.
// The trace group containing anonymous tracing is always assigned to bit 0.
//
////////////////////////////////////////////////////////////////////////////////
struct _TraceKey;

struct TraceGroup
{
    TraceGroup ();

    void addKey (_TraceKey *key);
    void setMask (TraceKeys mask);
    TraceGroup *selectTraces (const char *keyname, const char *filename);

    TraceContextSet contexts; // Set of contexts being traced
    _TraceKey       *keys;    // List of keys being traced within these contexts
    TraceGroup      *next;    // Keep the trace groups in a linked list
};

////////////////////////////////////////////////////////////////////////////////
//
// Set of log files
//
////////////////////////////////////////////////////////////////////////////////
class LogFileSet
{
public:
    LogFileSet ();
    ~LogFileSet ();

    // Initialization
    void reset ();
    void setFile (LogFile f);

    // Insert a log file
    void insert (LogFile f);

    // Number of log files
    inline int size () const
    {
        return m_numLogFiles;
    }

    // Output to all the log files
    void puts (const char *sz) const;

private:
    int     m_numLogFiles;
    LogFile m_files0[32];
    uint32  m_mask0;
    LogFile *m_files;
    uint32  *m_mask;
    int     m_maxLogFiles;
};

////////////////////////////////////////////////////////////////////////////////
//
// _TraceKey
//
// Actual object that gets checked to see if tracing is enabled
//
////////////////////////////////////////////////////////////////////////////////
struct _TraceKey
{
    _TraceKey (const char *_keyname, const char *_filename);

    static const bool check_mask = true;

    ALWAYS_INLINE TraceKeys getMask () const
    {
        return mask;
    }
    ALWAYS_INLINE bool checkTrace (const string &context) const
    {
        return !(mask & g_overloadedGroupMask) || group->contexts.isTracing(context);
    }
    ALWAYS_INLINE bool isTracing (TraceKeys keys, const string &context) const
    {
        return (keys & mask) && checkTrace(context);
    }
    inline void formatName (strbuff &buff, TraceKeys, const string &) const
    {
        buff.append(",%s", keyname);
    }
    inline void addFiles (LogFileSet &files, TraceKeys, const string &) const
    {
        files.insert(logFile);
    }

    TraceKeys  mask;      // One-hot (when enabled) trace group mask
    const char *keyname;  // Trace key, or "" for anonymous tracing
    const char *filename; // File containing trace key
    TraceGroup *group;    // Pointer to group object (required for slow test)
    _TraceKey  *next;     // Keep trace keys in a linked list within each group
    LogFile    logFile;   // Log file used for trace output
};

////////////////////////////////////////////////////////////////////////////////
//
// Internal functions
//
////////////////////////////////////////////////////////////////////////////////

// Globals
extern __thread TraceKeys t_globalTraceKeys;     // Bitmask of groups being traced in the global context
extern __thread Tracer    *t_tracer;             // Pointer to current tracer object
extern _TraceKey g_anonymousTraceKey;            // Global trace key used for anonymous tracing

extern descore::ThreadLocalData<string> t_globalTraceContext; // Name of the global context (initialized to "")
extern descore::ThreadLocalData<LogFileSet> t_traceLogFiles;  // Set of log files targeted by current trace statement

END_NAMESPACE_DESCORE

ALWAYS_INLINE TraceKeys getTraceKeys ()
{
    return descore::t_globalTraceKeys;
}

ALWAYS_INLINE const string &getTraceContext ()
{
    return descore::t_globalTraceContext;
}

BEGIN_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// Trace expressions
//
////////////////////////////////////////////////////////////////////////////////
enum TraceExprOp { TRACE_AND, TRACE_OR, TRACE_NOT };

template <typename TLHS, typename TRHS, TraceExprOp op>
class TraceExpr
{
    TraceExpr &operator= (const TraceExpr &);
public:
    TraceExpr (const TLHS &lhs, const TRHS &rhs) : m_lhs(lhs), m_rhs(rhs) {}

    static const bool check_mask = ((op == TRACE_AND) && (TLHS::check_mask || TRHS::check_mask)) ||
        ((op == TRACE_OR) && (TLHS::check_mask && TRHS::check_mask));

    ALWAYS_INLINE TraceKeys getMask () const
    {
        return m_lhs.getMask() | m_rhs.getMask();
    }

    ALWAYS_INLINE bool isTracing (TraceKeys keys, const string &context) const
    {
        bool lhs = m_lhs.isTracing(keys, context);
        bool rhs = m_rhs.isTracing(keys, context);
        if (op == TRACE_AND)
            return lhs && rhs;
        else if (op == TRACE_OR)
            return lhs || rhs;
        else
            return !lhs;
    }

    void formatName (strbuff &buff, TraceKeys keys, const string &context) const
    {
        if (op == TRACE_NOT)
            return;
        if (m_lhs.isTracing(keys, context))
            m_lhs.formatName(buff, keys, context);
        if (m_rhs.isTracing(keys, context))
            m_rhs.formatName(buff, keys, context);
    }

    void addFiles (LogFileSet &files, TraceKeys keys, const string &context) const
    {
        if (op == TRACE_NOT)
            return;
        if (m_lhs.isTracing(keys, context))
            m_lhs.addFiles(files, keys, context);
        if (m_rhs.isTracing(keys, context))
            m_rhs.addFiles(files, keys, context);
    }

    strbuff getName (TraceKeys keys, const string &context) const
    {
        strbuff buff;
        formatName(buff, keys, context);
        return **buff ? *buff + 1 : "";
    }

private:
    const TLHS &m_lhs;
    const TRHS &m_rhs;
};

// AND
template <typename TLHS, typename TRHS, TraceExprOp op, typename T>
ALWAYS_INLINE TraceExpr<TraceExpr<TLHS, TRHS, op>, T, TRACE_AND> operator&& (const TraceExpr<TLHS, TRHS, op> &lhs, const T &rhs)
{
    return TraceExpr<TraceExpr<TLHS, TRHS, op>, T, TRACE_AND>(lhs, rhs);
}
template <typename T>
ALWAYS_INLINE TraceExpr<_TraceKey, T, TRACE_AND> operator&& (const _TraceKey  &lhs, const T &rhs)
{
    return TraceExpr<_TraceKey, T, TRACE_AND>(lhs, rhs);
}
template <typename TLHS, typename TRHS, TraceExprOp op>
ALWAYS_INLINE TraceExpr<TraceExpr<TLHS, TRHS, op>, _TraceKey, TRACE_AND> operator&& (const TraceExpr<TLHS, TRHS, op> &lhs, int)
{
    return TraceExpr<TraceExpr<TLHS, TRHS, op>, _TraceKey, TRACE_AND>(lhs, g_anonymousTraceKey);
}
ALWAYS_INLINE TraceExpr<_TraceKey, _TraceKey, TRACE_AND> operator&& (const _TraceKey  &lhs, int)
{
    return TraceExpr<_TraceKey, _TraceKey, TRACE_AND>(lhs, g_anonymousTraceKey);
}
template <typename TLHS, typename TRHS, TraceExprOp op>
ALWAYS_INLINE TraceExpr<_TraceKey, TraceExpr<TLHS, TRHS, op>, TRACE_AND> operator&& (int, const TraceExpr<TLHS, TRHS, op> &rhs)
{
    return TraceExpr<_TraceKey, TraceExpr<TLHS, TRHS, op>, TRACE_AND>(g_anonymousTraceKey, rhs);
}
ALWAYS_INLINE TraceExpr<_TraceKey, _TraceKey, TRACE_AND> operator&& (int, const _TraceKey &rhs)
{
    return TraceExpr<_TraceKey, _TraceKey, TRACE_AND>(g_anonymousTraceKey, rhs);
}

// OR
template <typename TLHS, typename TRHS, TraceExprOp op, typename T>
ALWAYS_INLINE TraceExpr<TraceExpr<TLHS, TRHS, op>, T, TRACE_OR> operator|| (const TraceExpr<TLHS, TRHS, op> &lhs, const T &rhs)
{
    return TraceExpr<TraceExpr<TLHS, TRHS, op>, T, TRACE_OR>(lhs, rhs);
}
template <typename T>
ALWAYS_INLINE TraceExpr<_TraceKey, T, TRACE_OR> operator|| (const _TraceKey  &lhs, const T &rhs)
{
    return TraceExpr<_TraceKey, T, TRACE_OR>(lhs, rhs);
}
template <typename TLHS, typename TRHS, TraceExprOp op>
ALWAYS_INLINE TraceExpr<TraceExpr<TLHS, TRHS, op>, _TraceKey, TRACE_OR> operator|| (const TraceExpr<TLHS, TRHS, op> &lhs, int)
{
    return TraceExpr<TraceExpr<TLHS, TRHS, op>, _TraceKey, TRACE_OR>(lhs, g_anonymousTraceKey);
}
ALWAYS_INLINE TraceExpr<_TraceKey, _TraceKey, TRACE_OR> operator|| (const _TraceKey  &lhs, int)
{
    return TraceExpr<_TraceKey, _TraceKey, TRACE_OR>(lhs, g_anonymousTraceKey);
}
template <typename TLHS, typename TRHS, TraceExprOp op>
ALWAYS_INLINE TraceExpr<_TraceKey, TraceExpr<TLHS, TRHS, op>, TRACE_OR> operator|| (int, const TraceExpr<TLHS, TRHS, op> &rhs)
{
    return TraceExpr<_TraceKey, TraceExpr<TLHS, TRHS, op>, TRACE_OR>(g_anonymousTraceKey, rhs);
}
ALWAYS_INLINE TraceExpr<_TraceKey, _TraceKey, TRACE_OR> operator|| (int, const _TraceKey &rhs)
{
    return TraceExpr<_TraceKey, _TraceKey, TRACE_OR>(g_anonymousTraceKey, rhs);
}

// NOT
template <typename TLHS, typename TRHS, TraceExprOp op>
ALWAYS_INLINE TraceExpr<TraceExpr<TLHS, TRHS, op>, _TraceKey, TRACE_NOT> operator! (const TraceExpr<TLHS, TRHS, op> &lhs)
{
    return TraceExpr<TraceExpr<TLHS, TRHS, op>, _TraceKey, TRACE_NOT>(lhs, g_anonymousTraceKey);
}
ALWAYS_INLINE TraceExpr<_TraceKey, _TraceKey, TRACE_NOT> operator! (const _TraceKey &lhs)
{
    return TraceExpr<_TraceKey, _TraceKey, TRACE_NOT>(lhs, g_anonymousTraceKey);
}

////////////////////////////////////////////////////////////////////////////////
//
// Helper functions for determining whether or not tracing is enabled
//
////////////////////////////////////////////////////////////////////////////////
template <typename TLHS, typename TRHS, TraceExprOp op>
ALWAYS_INLINE bool maybeTracing (TraceKeys traceKeys, const TraceExpr<TLHS, TRHS, op> &traceExpr)
{
    (void) traceExpr; // May be unused dependeing on TLHS, TRHS, op
    (void) traceKeys; // May be unused dependeing on TLHS, TRHS, op
    return !TraceExpr<TLHS, TRHS, op>::check_mask || ((traceKeys & traceExpr.getMask()) != 0);
}
ALWAYS_INLINE bool maybeTracing (TraceKeys traceKeys, const _TraceKey &key)
{
    return (traceKeys & key.mask) != 0;
}
ALWAYS_INLINE bool maybeTracing (TraceKeys traceKeys, const char * = NULL)
{
    return (traceKeys & 1) != 0;
}
ALWAYS_INLINE bool maybeTracing (TraceKeys traceKeys, int)
{
    return (traceKeys & 1) != 0;
}

// Catch naming collisions between trace keys and variables with a semi-helpful message
template <typename T>
bool maybeTracing (TraceKeys, T)
{
    T::variable_is_not_a_trace_key_in_this_scope;
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// Tracer
//
// Object that performs the actual tracing.
//
////////////////////////////////////////////////////////////////////////////////
class Tracer
{
public:
    // Begin a trace; takes care of printf-style formatting
    Tracer &beginTracev (const string &context, const char *keyname, const char *msg, va_list args);

    // Conversion to bool so that the trace macros work
    operator bool () const
    {
        return false;
    }

    // Streaming semantics
    template <typename T>
    inline Tracer &operator<< (const T &val)
    {
        if (traceEnabled())
            output(str(val).c_str());
        return *this;
    }
    inline Tracer &operator<< (const char *s)
    {
        if (traceEnabled())
        {
            // If the string has a carriage return followed by additional characters,
            // then output() will need to hack the string so make a copy.
            const char *ch = strchr(s, '\n');
            if (ch && ch[1])
                output(string(s).c_str());
            else
                output(s);
        }
        return *this;
    }

    // Output function that can be called from traceHeader()
    void appendTrace (const char *msg, ...);

protected:
    // Overrideable output functions
    virtual void traceHeader (const string &context, const string &keyname);
    virtual bool traceEnabled () const
    {
        return true;
    }

private:
    // Internal output functions
    void output (const char *msg);
    void header ();

private:
    struct TracerState
    {
        string m_context;
        string m_keyname;
        bool   m_needHeader;
        bool   m_inHeader;
    };
    ThreadLocalData<TracerState> t_state;
};

////////////////////////////////////////////////////////////////////////////////
//
// NullTracer
//
// Object used to suppress trace output when the initial if() succeeds, but
// the trace key bit is overloaded and it isn't actually being traced in the
// current context.
//
////////////////////////////////////////////////////////////////////////////////
struct NullTracer : public Tracer
{
    virtual void traceHeader (const string &, const string &) {}
    virtual bool traceEnabled () const { return false; }
};

extern NullTracer g_nullTracer;

////////////////////////////////////////////////////////////////////////////////
//
// beginTrace()
//
////////////////////////////////////////////////////////////////////////////////
Tracer &beginTrace (TraceKeys, const string &context, bool forceTrace, const char *msg = NULL, ...) ATTRIBUTE_PRINTF(4);
inline Tracer &beginTrace (TraceKeys, const string &context, bool forceTrace, const char *msg, ...) 
{
    if (!forceTrace && !g_anonymousTraceKey.checkTrace(context))
        return g_nullTracer;

    t_traceLogFiles->setFile(g_anonymousTraceKey.logFile);

    va_list args;
    va_start(args, msg);
    Tracer &ret = t_tracer->beginTracev(context, "", msg ? msg : "", args);
    va_end(args);
    return ret;
}
inline Tracer &beginTrace (TraceKeys, const string &context, bool forceTrace, int)
{
    return beginTrace(0, context, forceTrace, (const char *) NULL);
}
Tracer &beginTrace (TraceKeys, const string &context, bool forceTrace, const _TraceKey &key, const char *msg = NULL, ...) ATTRIBUTE_PRINTF(5);
inline Tracer &beginTrace (TraceKeys, const string &context, bool forceTrace, const _TraceKey &key, const char *msg, ...)
{
    if (!forceTrace && !key.checkTrace(context))
        return g_nullTracer;

    t_traceLogFiles->setFile(key.logFile);

    va_list args;
    va_start(args, msg);
    Tracer &ret = t_tracer->beginTracev(context, key.keyname, msg, args);
    va_end(args);
    return ret;
}
template <typename TLHS, typename TRHS, TraceExprOp op>
Tracer &beginTrace (TraceKeys keys, const string &context, bool forceTrace, const TraceExpr<TLHS, TRHS, op> &expr, const char *msg = NULL, ...) ATTRIBUTE_PRINTF(5);
template <typename TLHS, typename TRHS, TraceExprOp op>
inline Tracer &beginTrace (TraceKeys keys, const string &context, bool forceTrace, const TraceExpr<TLHS, TRHS, op> &expr, const char *msg, ...)
{
    if (!forceTrace && !expr.isTracing(keys, context))
        return g_nullTracer;

    t_traceLogFiles->reset();
    expr.addFiles(*t_traceLogFiles, keys, context);
    if (!t_traceLogFiles->size())
        t_traceLogFiles->setFile(descore::LOG_STDOUT);

    va_list args;
    va_start(args, msg);
    Tracer &ret = t_tracer->beginTracev(context, expr.getName(keys, context), msg, args);
    va_end(args);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// isTracing()
//
////////////////////////////////////////////////////////////////////////////////
inline bool isTracing (TraceKeys, const string &context, int = 0)
{
    return g_anonymousTraceKey.checkTrace(context);
}
inline bool isTracing (TraceKeys, const string &context, const _TraceKey &key)
{
    return key.checkTrace(context);
}
template <typename TExpr>
inline bool isTracing (TraceKeys keys, const string &context, const TExpr &expr)
{
    return expr.isTracing(keys, context);
}

////////////////////////////////////////////////////////////////////////////////
//
// Helper structure to accept either pointer or references in the obj_ macros
//
////////////////////////////////////////////////////////////////////////////////

template <typename T, bool _is_pointer>
struct static_dereference_helper;

template <typename T>
struct static_dereference_helper<T, false>
{
    typedef const T &deref_t;
    inline static const T &static_dereference (const T &x)
    {
        return x;
    }
};
template <typename T>
struct static_dereference_helper<const T *, true>
{
    typedef const T &deref_t;
    inline static const T &static_dereference (const T *x)
    {
        return *x;
    }
};
template <typename T>
struct static_dereference_helper<T *, true>
{
    typedef const T &deref_t;
    inline static const T &static_dereference (T *x)
    {
        return *x;
    }
};

template <typename T>
typename static_dereference_helper<T, type_traits<T>::is_pointer>::deref_t static_dereference (const T &x)
{
    return static_dereference_helper<T, type_traits<T>::is_pointer>::static_dereference(x);
}

END_NAMESPACE_DESCORE

#endif // #ifndef Trace_hpp_100414100025

