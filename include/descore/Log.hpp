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
// Log.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Basic logging functions
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Log_hpp_
#define descore_Log_hpp_

#include "descore.hpp"

////////////////////////////////////////////////////////////////////////////////
//
// Log files
//
////////////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_DESCORE

enum LogFile
{
    LOG_STDOUT,
    LOG_STDERR,
    LOG_SYS
};

END_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// Global namespace logging functions
//
////////////////////////////////////////////////////////////////////////////////

// Logging with printf semantics to stdout and the log file.  
void log (const char *message, ...) ATTRIBUTE_PRINTF(1);
void logv (const char *message, va_list args);

// Logging with printf semantics to stderr and the log file.
void logerr (const char *message, ...) ATTRIBUTE_PRINTF(1);
void logerrv (const char *message, va_list args);

// Logging with printf semantics to stderr, the log file, and syslog
void logsys (const char *message, ...) ATTRIBUTE_PRINTF(1);
void logsysv (const char *message, va_list args);

// Logging with printf semantics to a specified LogFile
void log (descore::LogFile f, const char *message, ...) ATTRIBUTE_PRINTF(2);
void logv (descore::LogFile f, const char *message, va_list args);

// Log a raw string without printf semantics
void log_puts (const char *sz);
void log_puts (descore::LogFile f, const char *sz);

// ostream support
std::ostream &log ();
std::ostream &logerr ();
std::ostream &logsys ();
std::ostream &log (descore::LogFile f);

////////////////////////////////////////////////////////////////////////////////
//
// descore namespace helper functions
//
////////////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_DESCORE

//----------------------------------
// Global helper functions
//----------------------------------

// Create the log file; optionally include the command line in the log header
void initLog (const char *filename, int csz = 0, char **rgsz = NULL);

// Register a function that gets called by initLog to log a header.
// Call with NULL to un-register the function.
void setLogHeader (void (*logHeader) ());

// Register a function to replace console output.  Instead of
// producing output to stdout/stderr, this function will be called with
// the string that would have been written and the file (stdout/stderr)
// to which it would have been written.  Call with NULL to un-register
// the function.
void setLogConsoleOutput (void (*logConsole) (const char *, FILE *));

// Call fflush on stdout and all log files
void flushLog();

// Helper structure to copy the command line in case it is destructively
// parsed before calling initLog()
struct CopyArgs
{
    CopyArgs (int _csz, char **_rgsz);
    CopyArgs (const CopyArgs &rhs);
    ~CopyArgs ();

    int csz;
    char **rgsz;

private:
    void copy (int _csz, char **_rgsz);
    void operator= (const CopyArgs &);
};

//----------------------------------
// Per-log-file helper functions
//----------------------------------

// Close a log file
void closeLog ();
void closeLog (LogFile f);

// Enable/disable stderr/stdout (output will still be logged to file).
// Returns the current quiet mode setting.
bool getLogQuietMode ();
bool setLogQuietMode (bool quiet);
bool setLogQuietMode (LogFile f, bool quiet);

// By default, output to all log files is copied to the main log file.
// This function enabled/disables the copy on a per-log-file basis.
// Returns the current copy setting.
// Cannot be used for LOG_STDOUT or LOG_STDERR.
bool setLogCopyMode (LogFile f, bool copy_to_main_log);

// Specify a prefix for each line
void setLogPrefix (LogFile f, const string &prefix);
void setLogPrefix (LogFile f, const char * (*prefeix_fn) ());

// Specify whether console output should go to stdout or stderr
// (f must be stdout or stderr).
void setLogConsoleFile (LogFile f, FILE *console_out);

// Support for copying a log file to the syslog
void setLogSyslogEnabled  (LogFile f, bool copy_to_syslog);
void setLogSyslogIdentity (LogFile f, const string &identity);
void setLogSyslogFacility (LogFile f, int facility);
void setLogSyslogLevel    (LogFile f, int level);

// Create a log file
struct ILogOutput
{
    virtual ~ILogOutput () {}
    virtual void write (const char *line) = 0;
    virtual void close () {}
};
LogFile openLog (const string &filename);
LogFile appendLog (const string &filename);
LogFile openLog (ILogOutput *output);
LogFile reopenLog (LogFile f);

// Output a standard header to the log file
void logHeader (LogFile f);

END_NAMESPACE_DESCORE

#endif
