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
// Trace.cpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/14/2010
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Trace.hpp"
#include "Iterators.hpp"
#include "PrintStrings.hpp"

BEGIN_NAMESPACE_DESCORE

// Globals that can be restricted to this file
static Tracer g_defaultTracer;        // Default tracer object
 TraceKeys    g_overloadedGroupMask;  // Bitmask of bits that have been assigned to multiple groups
NullTracer    g_nullTracer;           // Global null tracer object

// Globals that need to be macro/inline-accessible
__thread TraceKeys  t_globalTraceKeys = 0;         // Bitmask of groups being traced in the global context
__thread Tracer     *t_tracer = &g_defaultTracer;  // Pointer to current tracer object
_TraceKey           g_anonymousTraceKey("", "");   // Global trace key used for anonymous tracing

ThreadLocalData<string> t_globalTraceContext; // Name of the global context (initialized to "")
ThreadLocalData<LogFileSet> t_traceLogFiles;  // Set of log files targeted by current trace statement

////////////////////////////////////////////////////////////////////////////////
//
// allTraceKeys()
//
// Global set of all trace key names used by -showkeys
//
////////////////////////////////////////////////////////////////////////////////
static std::set<string> &allTraceKeys ()
{
    static std::set<string> g_allTraceKeys;
    return g_allTraceKeys;
}

////////////////////////////////////////////////////////////////////////////////
//
// defaultTraceGroup()
//
// Default trace group containing no contexts.  This is used as the head of the
// trace group linked list.  Initially, all trace keys are assigned to this group.
//
////////////////////////////////////////////////////////////////////////////////
static TraceGroup &defaultTraceGroup ()
{
    static TraceGroup g_defaultTraceGroup;
    return g_defaultTraceGroup;
}

////////////////////////////////////////////////////////////////////////////////
//
// setGlobalTraceKeys()
//
////////////////////////////////////////////////////////////////////////////////
TraceKeys setGlobalTraceKeys (TraceKeys mask)
{
    TraceKeys ret = t_globalTraceKeys;
    t_globalTraceKeys = mask;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// setGlobalTraceContext()
//
////////////////////////////////////////////////////////////////////////////////
string setGlobalTraceContext (const string &context, bool recomputeGlobalTraceKeys)
{
    string ret = *descore::t_globalTraceContext;
    descore::t_globalTraceContext = context;
    if (recomputeGlobalTraceKeys)
        t_globalTraceKeys = computeTraceKeys(context);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// RecomputeGlobalTraceKeys
//
// Helper for setting the global trace keys in all threads when
// the set of traces changes.
//
////////////////////////////////////////////////////////////////////////////////
struct RecomputeGlobalTraceKeys
{
    DECLARE_NOCOPY(RecomputeGlobalTraceKeys);
public:
    RecomputeGlobalTraceKeys () 
        : m_globalTraceContext(t_globalTraceContext),
        m_globalTraceKeys(t_globalTraceKeys) 
    {
    }
    void recompute (const char *contexts)
    {
        if (wildcardMatch(contexts, *m_globalTraceContext))
            m_globalTraceKeys = computeTraceKeys(m_globalTraceContext);
    }
    string    &m_globalTraceContext;
    TraceKeys &m_globalTraceKeys;
};
static descore::ThreadLocalData<RecomputeGlobalTraceKeys> t_recomputeGlobalTraceKeys;

////////////////////////////////////////////////////////////////////////////////
//
// setTracer()
//
////////////////////////////////////////////////////////////////////////////////
Tracer *setTracer (Tracer *tracer)
{
    Tracer *ret = t_tracer;
    t_tracer = tracer;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// setTraceLogFile
//
////////////////////////////////////////////////////////////////////////////////
void setTraceLogFile (descore::LogFile f)
{
    setTraceLogFile("", f);
}

void setTraceLogFile (const char *keyname, descore::LogFile f)
{
    for (TraceGroup *group = defaultTraceGroup().next ; group ; group = group->next)
    {
        for (_TraceKey *key = group->keys ; key ; key = key->next)
        {
            if (wildcardMatch(keyname, key->keyname))
                key->logFile = f;
        }
    }
}

void setTraceLogFile (const descore::_TraceKey &key, descore::LogFile f)
{
    setTraceLogFile(key.keyname, f);
}

////////////////////////////////////////////////////////////////////////////////
//
// ITraceCallback
//
////////////////////////////////////////////////////////////////////////////////
static Mutex &traceMutex()
{
    static Mutex g_traceMutex;
    return g_traceMutex;
}

typedef std::set<ITraceCallback *, allow_ptr<ITraceCallback *> > TraceCallbackSet;
static TraceCallbackSet &traceCallbacks ()
{
    static TraceCallbackSet g_traceCallbacks;
    return g_traceCallbacks;
}
ITraceCallback::ITraceCallback ()
{
    ScopedLock lock(traceMutex());
    traceCallbacks().insert(this);
}
ITraceCallback::~ITraceCallback ()
{
    ScopedLock lock(traceMutex());
    traceCallbacks().erase(this);
}

////////////////////////////////////////////////////////////////////////////////
//
// setTrace/unsetTrace()
//
////////////////////////////////////////////////////////////////////////////////

// Shared implementation
static void updateTrace (const char *context, const char *keyname, const char *filename, bool include);

void setTrace (const char *context, const char *keyname, const char *filename)
{
    bool include = true;
    if (*context == '-')
    {
        include = false;
        context++;
    }
    if (keyname && *keyname == '-')
    {
        include = false;
        keyname++;
    }
    updateTrace(context, keyname, filename, include);
}

void unsetTrace (const char *context, const char *keyname, const char *filename)
{
    updateTrace(context, keyname, filename, false);
}

#define KEYCHAR(x) (                             \
    ((x) >= 'a' && (x) <= 'z') ||                \
    ((x) >= 'A' && (x) <= 'Z') ||                \
    ((x) >= '0' && (x) <= '9') ||                \
    ((x) == '_' || (x) == '$' || (x) == '*' || (x) == '?'))

static bool is_keyname (const char *ch)
{
    for ( ; *ch ; ch++)
    {
        if (!KEYCHAR(*ch))
            return false;
    }
    return true;
}

void updateTrace (const char *context, const char *keyname, const char *filename, bool include)
{
    if (!keyname && is_keyname(context))
        updateTrace("", context, filename, include);
    if (!keyname)
        keyname = "";

    traceMutex().lock();

    if (*keyname == '_')
        keyname++;

    assert_always(is_keyname(keyname), "Invalid trace key name: %s", keyname);

    // If the trace groups change, then we'll need to recompute trace keys for all contexts.
    // Otherwise, we only need to recompute trace keys for matching contexts.
    bool recomputeAllTraceKeys = false;

    // Iterate over the trace groups to find all traces that match this trace name.
    for (TraceGroup *group = &defaultTraceGroup() ; group ; group = group->next)
    {
        // Find all matching traces within this trace group.  There are three possibilities:
        // - If there are no matching traces then NULL is returned
        // - If all traces match then the group itself is returned
        // - If some of the traces match then they are removed from the trace group,
        //   and a new trace group is returned containing those traces.  This new
        //   trace group will have been inserted into the linked list immediately
        //   following the current group.
        TraceGroup *matchGroup = group->selectTraces(keyname, filename);
        if (matchGroup)
        {
            if (group != matchGroup)
                recomputeAllTraceKeys = true;
            group = matchGroup;
            group->contexts.updateTrace(context, include);
        }
    }

    if (!include)
    {
        // Garbage collect: any trace groups that are no longer tracing in any contexts
        // can be merged with the default trace group.
        TraceGroup **ppgroup = &defaultTraceGroup().next;
        while (*ppgroup)
        {
            TraceGroup *group = *ppgroup;
            if (!(*ppgroup)->contexts.specifiers.size() && !(*ppgroup)->contexts.contexts.size())
            {
                recomputeAllTraceKeys = true;
                
                // Move the keys back to the default trace group
                while (group->keys)
                {
                    _TraceKey *key = group->keys;
                    group->keys = key->next;
                    defaultTraceGroup().addKey(key);
                }
                
                // Delete the trace group
                *ppgroup = group->next;
                delete group;
            }
            else
                ppgroup = &group->next;
        }
    }

    if (recomputeAllTraceKeys)
    {
        // The default trace group is not tracing so it has mask 0
        defaultTraceGroup().setMask(0);
        
        // The group containing the anonymous trace key has mask 1
        if (g_anonymousTraceKey.group != &defaultTraceGroup())
            g_anonymousTraceKey.group->setMask(1);
        
        // Assign mask bits to the remaining trace groups
        TraceKeys mask = 2;
        TraceKeys assignedGroupMask = 0;
        g_overloadedGroupMask = 0;
        for (TraceGroup *group = defaultTraceGroup().next ; group ; group = group->next)
        {
            if (group != g_anonymousTraceKey.group)
            {
                group->setMask(mask);
                g_overloadedGroupMask |= (mask & assignedGroupMask);
                assignedGroupMask |= mask;
            }
            mask <<= 1;
            if (!mask)
                mask = 2;
        }
    }

    traceMutex().unlock();

    if (recomputeAllTraceKeys)
        context = "*";

    // Set the global mask
    t_recomputeGlobalTraceKeys.doAcross(&RecomputeGlobalTraceKeys::recompute, context);

    // Invoke callbacks
    TraceCallbackSet &callbacks = traceCallbacks();
    for (ITraceCallback *callback : callbacks)
        callback->notifyTrace(context);
}

////////////////////////////////////////////////////////////////////////////////
//
// setTraces()
//
////////////////////////////////////////////////////////////////////////////////
void setTraces (const char *traces)
{
    std::vector<string> vecTraces = expandSpecifierString(traces);
    for (unsigned i = 0 ; i < vecTraces.size() ; i++)
    {
        string context = vecTraces[i];
        string key;
        string file = "*";
        const char *keyname = NULL;

        // Look for :file
        string::size_type idx = context.find(':');
        if (idx != string::npos)
        {
            file = context.substr(idx+1);
            context = context.substr(0, idx);
        }

        // Look for /key
        idx = context.find('/');
        if (idx != string::npos)
        {
            key = context.substr(idx+1);
            context = context.substr(0, idx);
            keyname = *key;
        }
        setTrace(*context, keyname, *file);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// expandSpecifierString()
//
////////////////////////////////////////////////////////////////////////////////

static void append (std::vector<string> &lhs, const std::vector<string> &rhs)
{
    for (unsigned i = 0 ; i < rhs.size() ; i++)
        lhs.push_back(rhs[i]);
}
static std::vector<string> expandBraces (const string &str);

std::vector<string> expandSpecifierString (const string &str)
{
    std::vector<string> ret;
    const char *curr = *str;
    int depth = 0;
    for (const char *ch = curr ; *ch ; ch++)
    {
        if (*ch == '{')
            depth++;
        else if (*ch == '}')
            depth--;
        else if (!depth && *ch == ';')
        {
            append(ret, expandBraces(string(curr, 0, ch - curr)));
            curr = ch + 1;
        }
    }
    if (*curr)
        append(ret, expandBraces(curr));
    return ret;
}

// Helper function: parse a single specifier into a list of specifiers by 
// expanding within curly braces.
static std::vector<string> expandBraces (const string &s)
{
    std::vector<string> ret;
    const char *ch1 = strchr(*s, '{');
    if (!ch1)
        ret.push_back(s);
    else
    {
        const char *ch2 = ch1 + 1;
        int depth = 0;
        for ( ; depth || *ch2 != '}' ; ch2++)
        {
            assert_always(*ch2, "Missing '}' in specifier");
            if (*ch2 == '{')
                depth++;
            else if (*ch2 == '}')
                depth--;
        }
        string prefix = s.substr(0, ch1 - *s);
        std::vector<string> lhs = expandSpecifierString(string(ch1+1, ch2 - ch1 - 1));
        std::vector<string> rhs = expandBraces(ch2+1);
        for (unsigned i = 0 ; i < lhs.size() ; i++)
        {
            for (unsigned j = 0 ; j < rhs.size() ; j++)
                ret.push_back(prefix + lhs[i] + rhs[j]);
        }
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// parseTraces()
//
////////////////////////////////////////////////////////////////////////////////
void parseTraces (int &csz, char *rgsz[])
{
    int numArgs = csz;
    char **srcArgs = rgsz;

    csz = 1;
    for (int i = 1 ; i < numArgs ; i++)
    {
        if (!strcmp(rgsz[i], "-showtraces") || !strcmp(rgsz[i], "-showkeys"))
        {
            bool detailed = !strcmp(rgsz[i], "-showkeys");
            string match = "*";
            if (i < numArgs - 1)
            {
                match = rgsz[i+1];
                string::size_type idx = match.find(':');
                if (idx == string::npos)
                    match = str("*%s*:*", *match);
                else
                    match = str("*%s*:*%s*", *match.substr(0, idx), *match.substr(idx+1));
            }
            string currKey = "";
            std::set<string> keys;
            for (const string &key_file : allTraceKeys())
            {
                if (wildcardMatch(*key_file, *match))
                {
                    string::size_type idx = key_file.find(':');
                    string file = key_file.substr(idx+1);
                    string key = key_file.substr(0, idx);
                    if (detailed)
                    {
                        if (key != currKey)
                            printf("\n%s\n    %s", *key, *file);
                        else
                            printf(", %s", *file);
                        currKey = key;
                    }
                    else
                        keys.insert(key);
                }
            }
            if (detailed)
                printf("\n");
            else
                printStrings(keys);
            exit(0);
        }
        else if (!strcmp(rgsz[i], "-trace"))
        {
            assert_always(i < numArgs - 1, "-trace requires an argument (-trace <specifiers>)");
            setTraces(rgsz[++i]);
        }
        else
            rgsz[csz++] = srcArgs[i];
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// computeTraceKeys()
//
////////////////////////////////////////////////////////////////////////////////
TraceKeys computeTraceKeys (const string &context)
{
    descore::ScopedLock lock(traceMutex());
    TraceKeys mask = 0;
    for (TraceGroup *group = defaultTraceGroup().next ; group ; group = group->next)
    {
        if (group->contexts.isTracing(context))
            mask |= group->keys->mask;
    }
    return mask;
}

////////////////////////////////////////////////////////////////////////////////
//
// _TraceKey
//
////////////////////////////////////////////////////////////////////////////////
_TraceKey::_TraceKey (const char *_keyname, const char *_filename)
  : mask(0),
    keyname(_keyname),
    filename(_filename),
    logFile(LOG_STDOUT)
{
    if (*keyname == '_')
        keyname++;
    const char *slash;
    while ((slash = strchr(filename, '/')) != NULL)
        filename = slash + 1;
    while ((slash = strchr(filename, '\\')) != NULL)
        filename = slash + 1;

    defaultTraceGroup().addKey(this);
    if (*keyname)
        allTraceKeys().insert(str("%s:%s", keyname, filename));
}

////////////////////////////////////////////////////////////////////////////////
//
// LogFileSet
//
////////////////////////////////////////////////////////////////////////////////

LogFileSet::LogFileSet () : m_numLogFiles(0), m_mask0(0), m_files(m_files0), m_mask(&m_mask0), m_maxLogFiles(32)
{
}

LogFileSet::~LogFileSet ()
{
    if (m_maxLogFiles > 32)
    {
        delete[] m_files;
        delete[] m_mask;
    }
}

void LogFileSet::reset ()
{
    m_numLogFiles = 0;
    memset(m_mask, 0, 4 * ((m_maxLogFiles + 31) / 32));
}

void LogFileSet::setFile (LogFile f) 
{
    m_numLogFiles = 1;
    m_files[0] = f;
}

void LogFileSet::insert (LogFile f)
{
    while (f > m_maxLogFiles)
    {
        LogFile *files = new LogFile[2 * m_maxLogFiles];
        uint32 *mask   = new uint32[2 * m_maxLogFiles / 32];
        memcpy(files, m_files, m_numLogFiles * sizeof(LogFile));
        memcpy(mask, m_mask, m_maxLogFiles / 8);
        memset(mask + m_maxLogFiles / 32, 0, m_maxLogFiles / 8);
        if (m_maxLogFiles > 32)
        {
            delete[] m_files;
            delete[] m_mask;
        }
        m_files = files;
        m_mask = mask;
        m_maxLogFiles *= 2;
    }

    int index = f / 32;
    uint32 bit = 1 << (f % 32);
    if (m_mask[index] & bit)
        return;
    m_mask[index] |= bit;
    m_files[m_numLogFiles++] = f;
}

void LogFileSet::puts (const char *sz) const
{
    for (int i = 0 ; i < m_numLogFiles ; i++)
        ::log_puts(m_files[i], sz);
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceContextSet
//
////////////////////////////////////////////////////////////////////////////////
void TraceContextSet::updateTrace (const string &context, bool include)
{
    bool wildcard = strchr(*context, '?') || strchr(*context, '*');

    if (wildcard)
    {
        // Erase any existing trace specifiers superceded by this one
        bool foundInclude = include;
        std::list<ContextSpecifier>::iterator it = specifiers.begin();
        while (it != specifiers.end())
        {
            if (wildcardSubsumed(*it->context, *context))
            {
                it = specifiers.erase(it);
                continue;
            }
            else
                foundInclude |= it->include;
            it++;
        }
        
        // If there is at least one positive specifier then prepend this trace specifier,
        // otherwise reset the specifiers.
        if (foundInclude)
        {
            ContextSpecifier s = { context, include };
            specifiers.push_front(s);
        }
        else
            specifiers.clear();

        // Erase any explicit contexts that are subsumed by this specifier
        std::set<string>::iterator itContext = contexts.begin();
        while (itContext != contexts.end())
        {
            std::set<string>::iterator itNext = itContext;
            itNext++;
            if (wildcardMatch(*context, **itContext))
                contexts.erase(itContext);
            itContext = itNext;
        }
    }
    else if (include)
    {
        contexts.insert(context);
    }
    else
    {
        contexts.erase(context);

        // If the context matches an include specifier then we need to prepend
        // an exclude specifier.
        for (ContextSpecifier &s : specifiers)
        {
            if (wildcardMatch(*context, *s.context))
            {
                if (s.include)
                {
                    ContextSpecifier s = { context, include };
                    specifiers.push_front(s);
                }
                break;
            }
        }
    }
}

bool TraceContextSet::isTracing (const string &context)
{
    if (contexts.count(context))
        return true;
    for (ContextSpecifier &s : specifiers)
    {
        if (wildcardMatch(*context, *s.context))
            return s.include;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceGroup
//
////////////////////////////////////////////////////////////////////////////////
TraceGroup::TraceGroup ()
: keys(NULL),
next(NULL)
{
}

void TraceGroup::addKey (_TraceKey *key)
{
    key->next = keys;
    keys = key;
    key->group = this;
}

void TraceGroup::setMask (TraceKeys mask)
{
    for (_TraceKey *key = keys ; key ; key = key->next)
        key->mask = mask;
}

TraceGroup *TraceGroup::selectTraces (const char *keyname, const char *filename)
{
    // First pass: scan the list of keys to determine if all of them match,
    // none of them match, or some of them match.
    bool allMatch = true;
    bool noneMatch = true;
    for (_TraceKey *key = keys ; key ; key = key->next)
    {
        if (wildcardMatch(keyname, key->keyname) && wildcardMatch(filename, key->filename))
            noneMatch = false;
        else
            allMatch = false;
    }

    // Return NULL if none of the keys match
    if (noneMatch)
        return NULL;

    // Return the entire group if all of the keys match and we're not the default group
    if (allMatch && (this != &defaultTraceGroup()))
        return this;

    // Some of the keys match, so create a new group, put it in the linked list
    // of groups immediately after this group, and move the matching keys to the
    // new group.
    TraceGroup *group = new TraceGroup;
    group->next = next;
    next = group;
    group->contexts = contexts;
    _TraceKey **ppkey = &keys;
    while (*ppkey)
    {
        _TraceKey *key = *ppkey;
        if (wildcardMatch(keyname, key->keyname) && wildcardMatch(filename, key->filename))
        {
            *ppkey = key->next;
            group->addKey(key);
        }
        else
            ppkey = &key->next;
    }
    return group;
}

////////////////////////////////////////////////////////////////////////////////
//
// Tracer
//
////////////////////////////////////////////////////////////////////////////////

Tracer &Tracer::beginTracev (const string &context, const char *keyname, const char *msg, va_list args)
{
    if (traceEnabled())
    {
        TracerState &state = *t_state;
        state.m_context = context;
        state.m_keyname = keyname;
        state.m_needHeader = false;
        state.m_inHeader = false;
        
        header();
        if (msg)
            output(*vstr(msg, args));
    }
    return *this;
}

void Tracer::appendTrace (const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    output(*vstr(msg, args));
    va_end(args);
}

void Tracer::traceHeader (const string &context, const string &keyname)
{
    LogFileSet &files = *t_traceLogFiles;
    if (context != "")
        files.puts(*str("%s: ", *context));
    if (keyname != "")
        files.puts(*str("[%s] ", *keyname));
}

void Tracer::output (const char *_msg)
{
    TracerState &state = *t_state;
    LogFileSet &files = *t_traceLogFiles;
    char *msg = (char *) _msg;
    while (*msg)
    {
        if (state.m_needHeader)
            header();
        state.m_needHeader = true;
        char *cr = strchr(msg, '\n');
        if (!cr || !cr[1])
        {
            files.puts(msg);
            state.m_needHeader = false;
            return;
        }
        char temp = cr[1];
        cr[1] = 0;
        files.puts(msg);
        cr[1] = temp;
        msg = cr + 1;
    }
}

void Tracer::header ()
{
    TracerState &state = *t_state;
    if (!state.m_inHeader)
    {
        state.m_inHeader = true;
        traceHeader(state.m_context, state.m_keyname);
        state.m_inHeader = false;
    }
}

END_NAMESPACE_DESCORE
