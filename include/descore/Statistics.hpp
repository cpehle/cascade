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
// Statistics.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Infrastructure for automatic collection of statistics.
//
// Use:
//
// 1. Declare normal member variables to keep track of statistics, e.g.
//
//        int m_numCacheHits[NUM_CACHE_LINES];
//        int m_numCacheMisses[NUM_CACHE_LINES];
//
// 2. In each object which contains statistics, add a recordStats() function
//    to record the statistics (you can use a different function name).
//    Within this function, use the ADD_STAT macro to record an individual
//    named statistic:
//
//        ADD_STAT(<value>, <description>)
//
//    <value> can be any numeric expression.  Simple example:
//
//        void recordStats ()
//        {
//            for (int i = 0 ; i < NUM_CACHE_LINES ; i++)
//            {
//                ADD_STAT(m_numCacheMisses[i], "Mem: Number of cache misses");
//                ADD_STAT(m_numCacheHits[i],   "Mem: Number of cache hits");
//            }
//        }
//
//    If the string description is of the form "Header: description", then when
//    the statistics are displayed all statistics with the same header will
//    be grouped together.
//
// 3. At the end of a run, call recordStats() for each object.
//
//        for (int i = 0 ; i < numCaches ; i++)
//            m_cache[i]->recordStats();
//
// 4. Call Statistics::logStats() to output the statistics.  This call
//    also clears the statistics, so it is necessary to re-record the
//    statistics before they can be logged again.
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Statistics_hpp_
#define descore_Statistics_hpp_

#include "defines.hpp"
#include <string>
using std::string;

/////////////////////////////////////////////////////////////////
//
// Helper macro
//
/////////////////////////////////////////////////////////////////
#define ADD_STAT(statistic, description) Statistics::recordStat((float) (statistic), description)

/////////////////////////////////////////////////////////////////
//
// Statistics namespace
//
/////////////////////////////////////////////////////////////////
namespace Statistics 
{
    // Function called by the ADD_STAT macro
    void recordStat (float val, const string &description);

    // Output & reset the statistics
    void logStats (const char* format_str = "%.2f");

    // Output statistics without resetting them
    void logStatsNoClear (const char* format_str = ".2f");
};

/////////////////////////////////////////////////////////////////
//
// Individual statistic utility, used by Statistics, but also 
// stands alone just fine.
//
/////////////////////////////////////////////////////////////////
BEGIN_NAMESPACE_DESCORE

struct Statistic
{
    //NOTE: all but num are initialized on the first recordStat call, but it is
    // nice not to get uninitialized value warnings from tools like valgrind in
    // the case of an empty Statistic
    Statistic () : sum(0), min(0), max(0), An(0), Qn(0), num(0) {}

    void recordStat (float val);
    float average () const;
    float stddev () const;

    float sum;
    float min;
    float max;
    float An; //state for computing running stddev, see wikipedia...
    float Qn; //state for computing running stddev, see wikipedia...
    int num;
};

END_NAMESPACE_DESCORE

#endif

