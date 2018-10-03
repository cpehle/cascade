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
// Statistics.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Statistics.hpp"
#include "MapIterators.hpp"
#include "Thread.hpp" 
#include "PrintTable.hpp"

/////////////////////////////////////////////////////////////////
//
// Statistic
//
/////////////////////////////////////////////////////////////////
void descore::Statistic::recordStat (float val)
{
    if (num++)
    {
        sum += val;
        if (val < min)
            min = val;
        if (val > max)
            max = val;
        float Anm1 = An;
        An = Anm1 + (val - Anm1)/num;
        Qn = Qn + (val - Anm1)*(val - An);
    }
    else
    {
        sum = min = max = An = val;
        Qn = 0;
    }
}

float descore::Statistic::average () const
{
    return sum / num;
}

float descore::Statistic::stddev () const 
{
    return sqrt(Qn / num);
}

/////////////////////////////////////////////////////////////////
//
// Static collection of statistics being recorded
//
/////////////////////////////////////////////////////////////////
typedef std::map<string, descore::Statistic> StatMap;
static StatMap s_stats;
static descore::Mutex lock;

/////////////////////////////////////////////////////////////////
//
// Record a statistic
//
/////////////////////////////////////////////////////////////////
void Statistics::recordStat (float val, const string &description)
{
    descore::ScopedLock slock(lock);
    s_stats[description].recordStat(val);
}

/////////////////////////////////////////////////////////////////
//
// logStats()
//
/////////////////////////////////////////////////////////////////
void Statistics::logStats (const char* format_str)
{
    descore::ScopedLock slock(lock);
    logStatsNoClear(format_str);
    s_stats.clear();
}

void Statistics::logStatsNoClear (const char* format_str)
{
    descore::Table t(" Description|NOLPAD", "Average", "Stddev", "Minimum", "Maximum");
    {
        descore::ScopedLock slock(lock);
        string header = "";
        for (MapItem<StatMap> it : s_stats)
        {
            string description = it.key;
            string::size_type index = description.find(':');
            if (index == string::npos)
                header = "";
            else
            {
                string currHeader = description.substr(0, index + 1);
                if (currHeader != header)
                {
                    t.addRow(currHeader);
                    header = currHeader;
                }
                description = description.substr(index + 1);
            }
            t.addRow(description, str(format_str, it.value.average()), str(format_str, it.value.stddev()), 
                     str(format_str, it.value.min), str(format_str, it.value.max));
        }
    }
    t.print();
}

