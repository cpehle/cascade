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
// Log.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Log.hpp"
#include "Parameter.hpp"
#include "StringBuffer.hpp"
#include "Thread.hpp"
#include <iostream>
#include <memory>

#ifdef _MSC_VER
#include <sys/timeb.h>
// 'this' : used in base member initializer list
#pragma warning(disable:4355)
#else
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#endif

BEGIN_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////
//
// Globals
//
////////////////////////////////////////////////////////////////////////
static FILE *g_logfile = NULL;
static void (*g_logHeader) () = NULL;
static void (*g_logConsole) (const char *, FILE *) = NULL;
bool g_deletedLogBuffers = false;

////////////////////////////////////////////////////////////////////////////////
//
// CopyArgs
//
////////////////////////////////////////////////////////////////////////////////
CopyArgs::CopyArgs (int _csz, char **_rgsz)
{
    copy(_csz, _rgsz);
}

CopyArgs::CopyArgs (const CopyArgs &rhs)
{
    copy(rhs.csz, rhs.rgsz);
}

CopyArgs::~CopyArgs ()
{
    delete[] rgsz;
}

void CopyArgs::copy (int _csz, char **_rgsz)
{
    csz = _csz;
    rgsz = new char* [csz];
    memcpy(rgsz, _rgsz, csz * sizeof(char *));
}

/////////////////////////////////////////////////////////////////
//
// Log parameters
//
/////////////////////////////////////////////////////////////////
#ifndef _MSC_VER
struct SyslogParams : public IParameterChangeCallback
{
    SyslogParams ()
    {
        level.addCallback(this);
        identity.addCallback(this);
        facility.addCallback(this);
        log_errors.addCallback(this);
        quiet.addCallback(this);
    }

    StringParameter (level,      "LOG_INFO",   "Syslog level");
    StringParameter (identity,   "descore",    "Syslog identity used by logsys();"
                                               " if not explicitly set, will be set to log filename");
    StringParameter (facility,   "LOG_LOCAL1", "Syslog facility used by logsys();"
                                               " see 'man 3 syslog' for a list of facilities");
    BoolParameter   (log_errors, false,        "Errors are copied to the syslog");
    BoolParameter   (quiet,      false,        "Do not copy syslog output to the console");

#define FACILITY(x) else if (s == #x) return x
    int getFacility ()
    {
        string s = facility;
        if (0);
        FACILITY(LOG_AUTH);
        FACILITY(LOG_AUTHPRIV);
        FACILITY(LOG_CRON);
        FACILITY(LOG_DAEMON);
        FACILITY(LOG_FTP);
        FACILITY(LOG_KERN);
        FACILITY(LOG_LOCAL0);
        FACILITY(LOG_LOCAL1);
        FACILITY(LOG_LOCAL2);
        FACILITY(LOG_LOCAL3);
        FACILITY(LOG_LOCAL4);
        FACILITY(LOG_LOCAL5);
        FACILITY(LOG_LOCAL6);
        FACILITY(LOG_LOCAL7);
        FACILITY(LOG_LPR);
        FACILITY(LOG_MAIL);
        FACILITY(LOG_NEWS);
        FACILITY(LOG_SYSLOG);
        FACILITY(LOG_USER);
        FACILITY(LOG_UUCP);
        else
            die("Unrecognized syslog facility: %s", *s);
        return 0;
    }

#define LEVEL(x) else if (s == #x) return x
    int getLevel ()
    {
        string s = level;
        if (0);
        LEVEL(LOG_EMERG);
        LEVEL(LOG_ALERT);
        LEVEL(LOG_CRIT);
        LEVEL(LOG_ERR);
        LEVEL(LOG_WARNING);
        LEVEL(LOG_NOTICE);
        LEVEL(LOG_INFO);
        LEVEL(LOG_DEBUG);
        else
            die("Unrecognized syslog level: %s", *s);
        return 0;
    }

    void notifyChange (Parameter *)
    {
        setLogSyslogIdentity(LOG_SYS, identity);
        setLogSyslogFacility(LOG_SYS, getFacility());
        setLogSyslogLevel(LOG_SYS, getLevel());
        setLogQuietMode(LOG_SYS, quiet);
        setLogSyslogEnabled(LOG_STDERR, log_errors);
        setLogSyslogIdentity(LOG_STDERR, identity);
        setLogSyslogFacility(LOG_STDERR, getFacility());
    }
};
#endif

struct LogParams
{
    BoolParameter(quiet,  false, "Log to the log file only; suppress stdout");
    BoolParameter(append, false, "Open the log in append mode instead of write mode");
#ifndef _MSC_VER
    ParameterGroup(SyslogParams, syslog);
#endif
};

static ParameterGroup(LogParams, logParams, "log");

////////////////////////////////////////////////////////////////////////////////
//
// LogOutput
//
// Helper class to properly close the last instance of a given output
// file or interface
//
////////////////////////////////////////////////////////////////////////////////
struct LogOutput 
{
    LogOutput (FILE *_file_out, ILogOutput *_custom_out) 
        : file_out(_file_out), custom_out(_custom_out)
    {
    }
    ~LogOutput ()
    {
        if (file_out)
            fclose(file_out);
        if (custom_out)
            custom_out->close();
    }
    FILE       *file_out;
    ILogOutput *custom_out;
};

////////////////////////////////////////////////////////////////////////////////
//
// Log descriptor
//
// Data shared by all thread-local instances of a LogBuffer
//
////////////////////////////////////////////////////////////////////////////////
class LogBuffer;
class LogStream;

struct LogDescriptor
{
    LogDescriptor (FILE *_console_out, FILE *_file_out, ILogOutput *_custom_out);
    LogDescriptor (LogDescriptor *d);
    ~LogDescriptor ();

    const char *getPrefix () const
    {
        if (prefix_fn)
            return (*prefix_fn)();
        return *prefix;
    }

    // Console output
    FILE *console_out;
    bool quiet;

    // Custom output
    FILE                       *file_out;
    ILogOutput                 *custom_out;
    std::shared_ptr<LogOutput> closer;

    // Main log file output
    bool copy_to_main_log;

    // Syslog output
    string syslog_identity;
    int    syslog_facility;
    int    syslog_level;
    bool   copy_to_syslog;

    // Prefix for each line
    string prefix;
    const char * (*prefix_fn) ();

    // Per-thread log buffer
    ThreadLocalData<LogBuffer> *buff;

    // Log stream
    LogStream *stream;
};

////////////////////////////////////////////////////////////////////////////////
//
// LogBuffer
//
// Per-thread output buffer for logging
//
////////////////////////////////////////////////////////////////////////////////
class LogBuffer
{
public:
    LogBuffer (LogDescriptor *desc) : m_desc(desc)
    {
        m_prefixBuffer2.resize(64);
    }
    ~LogBuffer ()
    {
        if (**m_buff)
        {
            m_buff.puts("\n");
            output_to_log(*m_buff);
        }
    }

    // Append to the buffer 
    void vappend (const char *message, va_list args)
    {
        m_buff.vappend(message, args);
        check_output();
    }
    void puts (const char *sz)
    {
        m_buff.puts(sz);
        check_output();
    }

    // iostream helper
    void ios_append (const char *s, int n)
    {
        static char s_buff[256];
        char *buff = s_buff;
        if (n >= 256)
            buff = new char[n+1];
        memcpy(buff, s, n);
        buff[n] = 0;
        puts(buff);
        if (n >= 256)
            delete[] buff;
    }

    // Flush the current contents (doesn't play nice with carriage returns)
    void flush ()
    {
        if (**m_buff)
        {
            output_to_log(*m_buff);
            m_buff.clear();
        }
        fflush(m_desc->console_out);
        if (m_desc->file_out)
            fflush(m_desc->file_out);
    }

private:

    // Output any complete lines with carriange returns
    void check_output ()
    {
        // Use a lock when there are multiple carriage returns to keep
        // the lines together in the output
        static descore::Mutex mutex;

        const char *msg = m_buff;
        char *end = NULL;
        const char *cr = strchr(msg, '\n');
        bool use_lock = cr && strchr(cr+1, '\n');
        if (use_lock)
            mutex.lock();
        while (cr)
        {
            // A bit of a hack to avoid extra copying
            end = (char *) cr+1;
            char temp = *end;
            *end = 0;
            output_to_log(msg);
            *end = temp;
            msg = end;
            cr = strchr(msg, '\n');
        }
        if (use_lock)
            mutex.unlock();
        if (end)
        {
            m_buff.clear();
            if (*end)
                m_buff.puts(end);
        }

        if (m_buff.len() >= 0x1000)
        {
            output_to_log(*m_buff);
            m_buff.clear();
        }
    }

    // Function used to do the actual logging
    void output_to_log (const char *message)
    {
        formatPrefix();

        // Output to the console
        if ((!logParams.quiet || !g_logfile) && !m_desc->quiet)
        {
            if (g_logConsole)
            {
                g_logConsole(*m_prefix, m_desc->console_out);
                g_logConsole(message,   m_desc->console_out);
            }
            else
            {
                fputs(*m_prefix, m_desc->console_out);
                fputs(message,   m_desc->console_out);
            }
        }

        // Output to the main log file
        if (g_logfile && m_desc->copy_to_main_log)
        {
            fputs(*m_prefix, g_logfile);
            fputs(message,   g_logfile);
            fflush(g_logfile);
        }

        // Output to the custom file
        if (m_desc->file_out)
        {
            fputs(*m_prefix, m_desc->file_out);
            fputs(message,   m_desc->file_out);
            fflush(m_desc->file_out);
        }
        else if (m_desc->custom_out)
        {
            if (**m_prefix)
                m_desc->custom_out->write(*str("%s%s", *m_prefix, message));
            else
                m_desc->custom_out->write(message);
        }

        // Output to syslog
#ifndef _MSC_VER
        if (m_desc->copy_to_syslog)
        {
            ::openlog(*m_desc->syslog_identity, LOG_NDELAY | LOG_PID, m_desc->syslog_facility);
            if (**m_prefix)
                ::syslog(m_desc->syslog_level, *str("%s%s", *m_prefix, message));
            else
                ::syslog(m_desc->syslog_level, message);
            ::closelog();
        }
#endif
    }   

    void append (char *&dst, const char *sz)
    {
        while (*sz)
            *dst++ = *sz++;
    }

    void formatPrefix ()
    {
        const char *prefix = m_desc->getPrefix();

        if (!strchr(prefix, '%'))
        {
            m_prefix = prefix;
            return;
        }

        // Get the current time
        struct tm *timeinfo;
        int milliseconds;

#ifdef _MSC_VER
        struct _timeb timebuff;
        _ftime(&timebuff);
        timeinfo = localtime(&timebuff.time);
        milliseconds = timebuff.millitm;
#else
        struct timeval tv;
        gettimeofday(&tv, 0);
        struct tm _timeinfo;
        timeinfo = localtime_r(&tv.tv_sec, &_timeinfo);
        milliseconds = tv.tv_usec / 1000;
#endif

        // Handle stftime extensions first
        int len = 6 * strlen(prefix) + 1;
        if ((int) m_prefixBuffer.size() < len)
            m_prefixBuffer.resize(len);
        char *dst = &m_prefixBuffer[0];
        for (const char *ch = prefix ; *ch ; ch++)
        {
            *dst++ = *ch;
            if (*ch == '%' && ch[1])
            {
                ch++;
                if (*ch == 'L')
                {
                    sprintf(--dst, "%03d", milliseconds);
                    dst += 3;
                }
#ifdef _MSC_VER
                else if (*ch == 'F')
                    append(dst, "Y-%m-%d");
                else if (*ch == 'D')
                    append(dst, "m/%d/%y");
                else if (*ch == 'r')
                    append(dst, "I:%M:%S %p");
                else if (*ch == 'R')
                    append(dst, "H:%M");
                else if (*ch == 'T')
                    append(dst, "H:%M:%S");
                else if (*ch == 'P')
                    *dst++ = 'p';
#endif
                else
                    *dst++ = *ch;
            }
        }
        *dst = 0;

        // Call strftime
        while (!strftime(&m_prefixBuffer2[0], m_prefixBuffer2.size(), &m_prefixBuffer[0], timeinfo))
            m_prefixBuffer2.resize(2 * m_prefixBuffer2.size());

        m_prefix = &m_prefixBuffer2[0];
    }

private:
    strbuff       m_buff;
    string        m_prefix;
    LogDescriptor *m_desc;
    std::vector<char> m_prefixBuffer;
    std::vector<char> m_prefixBuffer2;
};

////////////////////////////////////////////////////////////////////////////////
//
// LogStreams
//
// std::ostream wrapper for LogFiles
//
////////////////////////////////////////////////////////////////////////////////

// Buffer specialization
class logbuff : public std::streambuf
{
public:
    logbuff (ThreadLocalData<LogBuffer> *buff) : m_buff(buff)
    {
    }

protected:

    virtual int_type overflow (int_type c)
    {
        (*m_buff)->ios_append((const char *) &c, 1);
        return c;
    }

    virtual std::streamsize xsputn (const char *s, std::streamsize n)
    {
        (*m_buff)->ios_append(s, n);
        return n;
    }

    ThreadLocalData<LogBuffer> *m_buff;
};

// Stream initialization
static void initStream (std::ostream &os)
{
    // See comments in ostrcaststream::initstream()
#ifdef DECIMAL_DIG
    os.precision(DECIMAL_DIG);
#else
    os.precision(21); // enough for a 64-bit, radix-2 significand, e.g., x86 long double
#endif

    // Output booleans as "true"/"false"
    os.setf(std::ios_base::boolalpha);
}

// LogStream class
class LogStream
{
    friend class LogVector;
public:
    LogStream (ThreadLocalData<LogBuffer> *buff) : m_buff(buff), m_ostream(&m_buff)
    {
        initStream(m_ostream);
    }
    
    operator std::ostream & ()
    {
        return m_ostream;
    }

private:
    logbuff      m_buff;
    std::ostream m_ostream;
};

////////////////////////////////////////////////////////////////////////////////
//
// Log descriptor constructor/destructor
//
////////////////////////////////////////////////////////////////////////////////
LogDescriptor::LogDescriptor (FILE *_console_out, FILE *_file_out, ILogOutput *_custom_out)
    : console_out(_console_out),
      quiet(false),
      file_out(_file_out),
      custom_out(_custom_out),
      closer(new LogOutput(_file_out, _custom_out)),
      copy_to_main_log(true),
#ifndef _MSC_VER
      syslog_identity(logParams.syslog.identity),
      syslog_facility(logParams.syslog.getFacility()),
      syslog_level(logParams.syslog.getLevel()),
#endif
      copy_to_syslog(false),
      prefix_fn(NULL),
      buff(new ThreadLocalData<LogBuffer>(this)),
      stream(NULL)
{
}

LogDescriptor::LogDescriptor (LogDescriptor *d)
{
    *this = *d;
    stream = NULL;
    buff = new ThreadLocalData<LogBuffer>(this);
}

LogDescriptor::~LogDescriptor ()
{
    delete stream;
    delete buff;
}

////////////////////////////////////////////////////////////////////////////////
//
// LogVector
//
// Thread-safe vector used to map the LogFile enum to actual log descriptors
// and buffers without requiring locking for the first 256 log files.
//
////////////////////////////////////////////////////////////////////////////////

#define LOCKFREE_VEC_SIZE 256
#define CLOSED_LOG        ((LogDescriptor *) (intptr_t) 0x1)

class LogVector
{
public:
    LogVector () : m_size(0)
    {
        memset(m_lockfreeDescriptors, 0, sizeof(m_lockfreeDescriptors));
    }
    ~LogVector ()
    {
        // Don't delete the standard log files
        for (int i = 3 ; i < m_size && i < LOCKFREE_VEC_SIZE ; i++)
        {
            if (m_lockfreeDescriptors[i] != CLOSED_LOG)
                delete m_lockfreeDescriptors[i];
        }
        for (int i = 0 ; i < (int) m_additionalDescriptors.size() ; i++)
        {
            if (m_additionalDescriptors[i] != CLOSED_LOG)
                delete m_additionalDescriptors[i];
        }
    }

    LogFile alloc (LogDescriptor *descriptor)
    {
        LogFile f = find_free(descriptor);
        descriptor->stream = new LogStream(descriptor->buff);
        if (f >= m_size)
            m_size = f + 1;
        return f;
    }

    LogDescriptor *operator[] (LogFile f)
    {
        LogDescriptor *ret = NULL;
        if (f < LOCKFREE_VEC_SIZE)
            ret = m_lockfreeDescriptors[f];
        else if (f < m_size)
        {
            ScopedLock lock(m_mutex);
            ret = m_additionalDescriptors[f - LOCKFREE_VEC_SIZE];
        }
        assert_always(ret, "Invalid LogFile (%d)", f);
        assert_always(ret != CLOSED_LOG, "LogFile %d has already been closed", f);
        return ret;
    }

    void close (LogFile f)
    {
        delete (*this)[f];
        if (f < LOCKFREE_VEC_SIZE)
            m_lockfreeDescriptors[f] = CLOSED_LOG;
        else
        {
            ScopedLock lock(m_mutex);
            m_additionalDescriptors[f - LOCKFREE_VEC_SIZE] = CLOSED_LOG;
        }
    }

    void flush ()
    {
        for (int i = 0 ; i < m_size && i < LOCKFREE_VEC_SIZE ; i++)
            flush(m_lockfreeDescriptors[i]);
        for (int i = 0 ; i < (int) m_additionalDescriptors.size() ; i++)
            flush(m_additionalDescriptors[i]);        
    }

    void captureStandardStreams ()
    {
        static bool initialized = false;

        if (!initialized)
        {
            initStream(std::cout);
            initStream(std::cerr);
            initStream(std::clog);
            
            std::cout.rdbuf(&m_lockfreeDescriptors[LOG_STDOUT]->stream->m_buff);
            std::cerr.rdbuf(&m_lockfreeDescriptors[LOG_STDERR]->stream->m_buff);
            std::clog.rdbuf(&m_lockfreeDescriptors[LOG_STDOUT]->stream->m_buff);

            initialized = true;
        }
    }

protected:

    LogFile find_free (LogDescriptor *descriptor)
    {
        for (int i = 0 ; i < LOCKFREE_VEC_SIZE ; i++)
        {
            if (!m_lockfreeDescriptors[i] || m_lockfreeDescriptors[i] == CLOSED_LOG)
            {
                m_lockfreeDescriptors[i] = descriptor;
                return (LogFile) i;
            }
        }

        ScopedLock lock(m_mutex);
        for (int i = 0 ; i < (int) m_additionalDescriptors.size() ; i++)
        {
            if (!m_additionalDescriptors[i] || m_additionalDescriptors[i] == CLOSED_LOG)
            {
                m_additionalDescriptors[i] = descriptor;
                return (LogFile) (i + LOCKFREE_VEC_SIZE);
            }
        }
        m_additionalDescriptors.push_back(descriptor);
        return (LogFile) (m_additionalDescriptors.size() + LOCKFREE_VEC_SIZE - 1);
    }

    void flush (LogDescriptor *desc)
    {
        if (desc && desc != CLOSED_LOG)
            (*desc->buff)->flush();
    }

private:
    LogDescriptor                *m_lockfreeDescriptors[LOCKFREE_VEC_SIZE];
    std::vector<LogDescriptor *> m_additionalDescriptors;
    descore::Mutex               m_mutex;
    int                          m_size;
};

////////////////////////////////////////////////////////////////////////
//
// Log buffers
//
////////////////////////////////////////////////////////////////////////
struct LogBuffers
{
    LogBuffers ()
    {
        descriptors.alloc(new LogDescriptor(stdout, NULL, NULL));  // LOG_STDOUT
        descriptors.alloc(new LogDescriptor(stderr, NULL, NULL));  // LOG_STDERR
        descriptors.alloc(new LogDescriptor(stderr, NULL, NULL));  // LOG_SYS

#ifndef _MSC_VER
        descriptors[LOG_STDERR]->copy_to_syslog = logParams.syslog.log_errors;
        descriptors[LOG_STDERR]->syslog_level = LOG_ERR;
        descriptors[LOG_SYS]->copy_to_syslog = true;
        descriptors[LOG_SYS]->quiet = logParams.syslog.quiet;
#endif
    }

    ~LogBuffers ()
    {
        g_deletedLogBuffers = true;
    }

    LogBuffer &operator[] (LogFile f)
    {
        return *descriptors[f]->buff;
    }

    LogDescriptor *getDescriptor (LogFile f)
    {
        return descriptors[f];
    }

    void flush ()
    {
        descriptors.flush();
    }

    LogFile openLog (FILE *f)
    {
        return descriptors.alloc(new LogDescriptor(stdout, f, NULL));
    }

    LogFile openLog (ILogOutput *output)
    {
        return descriptors.alloc(new LogDescriptor(stdout, NULL, output));
    }

    LogFile reopenLog (LogFile f)
    {
        return descriptors.alloc(new LogDescriptor(getDescriptor(f)));
    }

    void closeLog (LogFile f)
    {
        assert_always((int) f > (int) LOG_SYS, "Cannot close pre-defined log files");
        descriptors.close(f);
    }

    LogVector descriptors;
};

static LogBuffers &t_logbuffs ()
{
    static LogBuffers buffs;
    return buffs;
}

struct InitLogBuffers
{
    InitLogBuffers () 
    { 
        t_logbuffs(); 
    }
} initLogBuffers;

// Used in Thread.cpp
void createLogBuffers ()
{
    t_logbuffs();
}

////////////////////////////////////////////////////////////////////////////////
//
// ctime helper
//
////////////////////////////////////////////////////////////////////////////////
static const char *_ctime ()
{
    static __thread char ctime_buff[64];
    time_t ltime;
    ltime = time(NULL);
#ifdef _MSC_VER
    ctime_s(ctime_buff, 64, &ltime);
#else
    ctime_r(&ltime, ctime_buff);
#endif
    ctime_buff[strlen(ctime_buff) - 1] = 0;
    return ctime_buff;
}

////////////////////////////////////////////////////////////////////////////////
//
// logHeader()
//
////////////////////////////////////////////////////////////////////////////////
void logHeader (LogFile f)
{
    const char *t = _ctime();

#ifdef __USE_BSD
    char h[512];
    gethostname(h, sizeof(h));
    log(f, "#\n#  Log started at %s on host '%s'\n#\n", t, h);
#else
    log(f, "#\n#  Log started at %s\n#\n", t);
#endif
}

/////////////////////////////////////////////////////////////////
//
// logHeader
//
/////////////////////////////////////////////////////////////////
void setLogHeader (void (*fnLogHeader) ())
{
    g_logHeader = fnLogHeader;
}

/////////////////////////////////////////////////////////////////
//
// logConsoleOutput
//
/////////////////////////////////////////////////////////////////
void setLogConsoleOutput (void (*fnLogConsole) (const char *, FILE *))
{
    g_logConsole = fnLogConsole;
}

////////////////////////////////////////////////////////////////////////
//
// initLog()
//
// (re)Create the main log file.
//
////////////////////////////////////////////////////////////////////////
void initLog (const char *filename, int csz, char **rgsz)
{
    closeLog();
    bool append = logParams.append;
#if defined(F_SETLK)
    int fd;
    int flags = O_WRONLY | O_CREAT;
    if (append)
        flags |= O_APPEND;
    if( (fd = open(filename, flags, 0666)) < 0)
        die("Could not open log file\n");

    // Try to acquire an exclusive flock on the logfile.
    struct flock flk;
    flk.l_type = F_WRLCK;
    flk.l_whence = SEEK_SET;
    flk.l_start = 0;
    flk.l_len = 0;              // whole file
    if (fcntl(fd, F_SETLK, &flk) < 0)
    {
        // Somebody else has this file open.
        // Complain to cerr but don't trash the file.
        close(fd);
        std::cerr << "Couldn't get a lock on " << filename << ".  Output will not be written there." << std::endl;
        g_logfile = NULL;
    }
    else
    {
        // We got the lock.  Truncate the file and
        // turn the fd into a FILE*.
        ftruncate(fd, 0);
        if ((g_logfile = fdopen(fd, "w")) == NULL)
            die("Could not open logfile\n");
    }
#else
    // Fallback if there's no fcntl(F_SETLK):
    // It will garble Machine.log if two processes open it,
    // but if you don't have fcntl(F_SETFLK) it's better
    // than nothing...
    if ((g_logfile = fopen(filename, append ? "a" : "w")) == NULL)
        die("Could not open log file\n");
#endif

    if (g_logHeader)
        g_logHeader();
    logHeader(LOG_STDOUT);

    if (csz)
    {
        log("# ");
        for (int i = 0 ; i < csz ; i++)
            log("%s ", rgsz[i]);
        log("\n#\n");
    }

    t_logbuffs().descriptors.captureStandardStreams();
}

////////////////////////////////////////////////////////////////////////////////
//
// descore namespace helper functions
//
////////////////////////////////////////////////////////////////////////////////

void flushLog ()
{
    if (!g_deletedLogBuffers)
    {
        t_logbuffs().flush();
        if (g_logfile)
            fflush(g_logfile);
    }
}

void closeLog ()
{
    flushLog();
    if (g_logfile)
        fclose(g_logfile);
    g_logfile = NULL;
}

void closeLog (descore::LogFile f)
{
    t_logbuffs().closeLog(f);
}

bool getLogQuietMode ()
{
    return logParams.quiet;
}

bool setLogQuietMode (bool quiet)
{
    bool ret = logParams.quiet;
    logParams.quiet = quiet;
    return ret;
}

bool setLogQuietMode (LogFile f, bool quiet)
{
    LogDescriptor *desc = t_logbuffs().getDescriptor(f);
    bool ret = desc->quiet;
    desc->quiet = quiet;
    return ret;
}

bool setLogCopyMode (LogFile f, bool copy_to_main_log)
{
    assert_always(f != LOG_STDOUT && f != LOG_STDERR, "Log copy mode cannot be set for LOG_STDOUT or LOG_STDERR");
    LogDescriptor *desc = t_logbuffs().getDescriptor(f);
    bool ret = desc->copy_to_main_log;
    desc->copy_to_main_log = copy_to_main_log;
    return ret;
}

void setLogPrefix (descore::LogFile f, const string &prefix)
{
    t_logbuffs().getDescriptor(f)->prefix = prefix;
    t_logbuffs().getDescriptor(f)->prefix_fn = NULL;
}

void setLogPrefix (descore::LogFile f, const char * (*prefix_fn) ())
{
    t_logbuffs().getDescriptor(f)->prefix = "";
    t_logbuffs().getDescriptor(f)->prefix_fn = prefix_fn;
}

void setLogConsoleFile (LogFile f, FILE *console_out)
{
    assert_always(f != LOG_STDOUT && f != LOG_STDERR, "Log console file cannot be set for LOG_STDOUT or LOG_STDERR");
    assert_always(console_out == stdout || console_out == stderr, "log console file must be either stdout or stderr");
    t_logbuffs().getDescriptor(f)->console_out = console_out;
}

void setLogSyslogEnabled (LogFile f, bool copy_to_syslog)
{
    assert_always(f != LOG_SYS, "Cannot enable or disable syslog for LOG_SYS");
    t_logbuffs().getDescriptor(f)->copy_to_syslog = copy_to_syslog;

#ifndef _MSC_VER
    if (f == LOG_STDERR)
        logParams.syslog.log_errors = copy_to_syslog;
#endif
}

void setLogSyslogIdentity (LogFile f, const string &identity)
{
    assert_always(f != LOG_STDERR, "syslog identity cannot be set for LOG_STDERR");
    t_logbuffs().getDescriptor(f)->syslog_identity = identity;
}

void setLogSyslogFacility (LogFile f, int facility)
{
    assert_always(f != LOG_STDERR, "syslog facility cannot be set for LOG_STDERR");
    t_logbuffs().getDescriptor(f)->syslog_facility = facility;
}

void setLogSyslogLevel (LogFile f, int level)
{
    assert_always(f != LOG_STDERR, "syslog level cannot be set for LOG_STDERR");
    t_logbuffs().getDescriptor(f)->syslog_level = level;
}

static LogFile createLog (const string &filename, const char *mode)
{
    FILE *f = fopen(*filename, mode);
    assert_always(f, "Could not open %s", *filename);
    return t_logbuffs().openLog(f);
}

LogFile openLog (const string &filename)
{
    return createLog(filename, "w");
}

LogFile appendLog (const string &filename)
{
    return createLog(filename, "a");
}

LogFile openLog (ILogOutput *output)
{
    return t_logbuffs().openLog(output);
}
LogFile reopenLog (LogFile f)
{
    return t_logbuffs().reopenLog(f);
}

END_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// Global namespace logging functions
//
////////////////////////////////////////////////////////////////////////////////

void log (const char *message, ...)
{
    va_list args;
    va_start(args, message);
    logv(descore::LOG_STDOUT, message, args);
    va_end(args);
}

void logerr (const char *message, ...)
{
    va_list args;
    va_start(args, message);
    logv(descore::LOG_STDERR, message, args);
    va_end(args);
}

void logsys (const char *message, ...)
{
    va_list args;
    va_start(args, message);
    logv(descore::LOG_SYS, message, args);
    va_end(args);
}

void log (descore::LogFile f, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    logv(f, message, args);
    va_end(args);
}

void logv (const char *message, va_list args)
{
    logv(descore::LOG_STDOUT, message, args);
}

void logerrv (const char *message, va_list args)
{
    logv(descore::LOG_STDERR, message, args);
}

void logsysv (const char *message, va_list args)
{
    logv(descore::LOG_SYS, message, args);
}

void logv (descore::LogFile f, const char *message, va_list args)
{
    if (descore::g_deletedLogBuffers)
        vfprintf((f == descore::LOG_STDERR) ? stderr : stdout, message, args);
    else
        descore::t_logbuffs()[f].vappend(message, args);
}

void log_puts (const char *sz)
{
    log_puts(descore::LOG_STDOUT, sz);
}

void log_puts (descore::LogFile f, const char *sz)
{
    if (descore::g_deletedLogBuffers)
        fputs(sz, (f == descore::LOG_STDERR) ? stderr : stdout);
    else
        descore::t_logbuffs()[f].puts(sz);
}

std::ostream &log ()
{
    return log(descore::LOG_STDOUT);
}
std::ostream &logerr ()
{
    return log(descore::LOG_STDERR);
}
std::ostream &logsys ()
{
    return log(descore::LOG_SYS);
}
std::ostream &log (descore::LogFile f)
{
    return *descore::t_logbuffs().getDescriptor(f)->stream;
}

