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
// Coverage.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 05/17/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Coverage.hpp"
#include "Iterators.hpp"
#include "MapIterators.hpp"

using namespace std;

#define COVERAGE_ID_INVALID -1

/////////////////////////////////////////////////////////////////
//
// Static map from (file, line) to coverage data
//
/////////////////////////////////////////////////////////////////

// line number => coverage assertion data
typedef map<int, CoverageAssertionData *> FileCoverageAssertionMap;

// filename => file-specific coverage assertions
typedef map<string, FileCoverageAssertions *> GlobalCoverageAssertionMap;

// line number (end of section) => coverage section
typedef map<int, CoverageAssertionSection *> FileCoverageAssertionSectionMap;

// Keep track of the sections and assertions within a file
struct FileCoverageAssertions
{
    FileCoverageAssertionMap assertions;
    FileCoverageAssertionSectionMap sections;
    const char *file;
};

// Make sure we delete all coverage data on shutdown
struct GlobalCoverageAssertions
{
    ~GlobalCoverageAssertions ()
    {
        for_map_values (FileCoverageAssertions *itFile, s_map)
        {
            for_map_values (CoverageAssertionData *data, itFile->assertions)
                delete data;
            delete itFile;
        }
    }

    GlobalCoverageAssertionMap s_map;
};

// Singleton buried in a static accessor function to ensure that it
// gets constructed before it gets used.
static inline GlobalCoverageAssertionMap &getGlobalCoverageAssertionMap ()
{
    static GlobalCoverageAssertions s_assertions;
    return s_assertions.s_map;
}

// It's common for this function to be called multiple times in a row for
// the same file, so cache the last return result.
static inline FileCoverageAssertions *getFileCoverageAssertions (const char *file)
{
    static const char *lastFile = NULL;
    static FileCoverageAssertions *cache = NULL;

    if (file != lastFile)
    {
        GlobalCoverageAssertionMap &assertions = getGlobalCoverageAssertionMap();
        GlobalCoverageAssertionMap::iterator it = assertions.find(file);
        if (it == assertions.end())
        {
            cache = assertions[file] = new FileCoverageAssertions;
            cache->file = file;
        }
        else
            cache = it->second;
        lastFile = file;
    }

    return cache;
}

/////////////////////////////////////////////////////////////////
//
// Coverage sections
//
/////////////////////////////////////////////////////////////////

// Multimap of section name => section
class CoverageAssertionSection;
typedef multimap<string, CoverageAssertionSection *> CoverageAssertionSectionMap;
static inline CoverageAssertionSectionMap &getCoverageAssertionSectionMap ()
{
    static CoverageAssertionSectionMap s_map;
    return s_map;
}

CoverageAssertionSection *CoverageAssertionSection::s_currSection = NULL;
const char *CoverageAssertionSection::s_currName = NULL;
const char *CoverageAssertionSection::s_currFile = NULL;

// Start a new section
CoverageAssertionSection::CoverageAssertionSection (const char *name, const char *file, int line, int id)
: firstLine(line), firstId(id), enabled(false)
{
    assert_always(!s_currSection, "Missing END_COVERAGE() for coverage section \"%s\"\n"
        "    defined at %s(%d)", s_currName, s_currFile, s_currSection->firstLine);
    s_currSection = this;
    s_currName = name;
    s_currFile = file;
}

// End a section
void CoverageAssertionSection::endSection (const char *file, int line, int id)
{
    assert_always(s_currSection, "Missing BEGIN_COVERAGE() before END_COVERAGE()\n"
        "    at %s(%d)", s_currFile, line);
    assert_always(!strcmp(s_currFile, file),  "Missing END_COVERAGE() for coverage section \"%s\"\n"
        "    defined at %s(%d)", s_currName, s_currFile, s_currSection->firstLine);
    s_currSection->endSection(line, id);
    s_currSection = NULL;
}

void CoverageAssertionSection::endSection (int line, int id)
{
    // Check to see if this section already exists
    CoverageAssertionSectionMap &assertions = getCoverageAssertionSectionMap();
    for (Iterator<CoverageAssertionSectionMap> it(assertions, s_currName, s_currName) ; it ; it++)
    {
        CoverageAssertionSection *section = *it;
        if (section->firstLine == firstLine)
        {
            // Found a match.  Check the counter; don't register a duplicate section.
            if (section->lineCoverage.size() && (section->firstId != firstId))
            {
                logerr("Warning: inconsistent counter detected in converage section %s\n    of file %s.\n"
                    "   Line coverage has been disabled for this section.", s_currName, s_currFile);
                section->lineCoverage.clear();
            }
            return;
        }
    }

    // No match - finalize this section and add it.
    lastLine = line;
    lastId = id;
    if (lastId >= firstId)
        lineCoverage.assign(lastId - firstId + 1, 0);
    assertions.insert(CoverageAssertionSectionMap::value_type(s_currName, this));
    fileAssertions = getFileCoverageAssertions(s_currFile);
    fileAssertions->sections.insert(FileCoverageAssertionSectionMap::value_type(line, this));
}

/////////////////////////////////////////////////////////////////////////////
//
// CoverageAssertion
//
/////////////////////////////////////////////////////////////////////////////
CoverageAssertion::CoverageAssertion (const char *file, int line, int id, int numValues)
{
    FileCoverageAssertions *fileAssertions = getFileCoverageAssertions(file);
    FileCoverageAssertionMap::iterator it = fileAssertions->assertions.find(line);
    if (it == fileAssertions->assertions.end())
    {
        // Create and initialize the coverage assertion data
        data = fileAssertions->assertions[line] = new CoverageAssertionData;
        data->assertion = this;
        data->covered.assign(numValues, false);

        // Find the section that we're in and see if we're enabled
        FileCoverageAssertionSectionMap::iterator it = fileAssertions->sections.upper_bound(line);
        assert_always((it != fileAssertions->sections.end()) && (it->second->firstLine < line) && (it->second->lastLine > line),
            "Coverage assertion must appear within a coverage section\n    %s(%d)", file, line);
        data->numUncovered = it->second->enabled ? numValues : 0;
        data->id = id - it->second->firstId;
    }
    else
    {
        // There is already another instance of this assertion and we're sharing
        // the data.  Make sure that we agree on how many coverage values there are.
        data = it->second;
        assert_always((int) data->covered.size() == numValues, 
            "Inconsistent number of coverage values for coverage assertion\n    %s(%d)", file, line);
    }
}

bool CoverageAssertion::checkAndReset ()
{
    bool ret = true;
    CoverageAssertionSectionMap &coverageMap = getCoverageAssertionSectionMap();

    // Iterate over the coverage sections
    for (MapItem<CoverageAssertionSectionMap> itSection : coverageMap)
    {
        CoverageAssertionSection *section = itSection.value;
        if (section->enabled)
        {
            const char *file = section->fileAssertions->file;

            // Check coverage assertions
            Iterator<FileCoverageAssertionMap> it(section->fileAssertions->assertions, section->firstLine, section->lastLine);
            for ( ; it ; it++)
            {
                CoverageAssertionData *data = *it;
                vector<bool> &covered = data->covered;
                if (data->numUncovered)
                {
                    ret = false;
                    CoverageAssertion *assertion = data->assertion;
                    logerr("Coverage assertion failed in section '%s':\n", *itSection.key);
                    logerr("    %s(%d): %s\n", file, it.key(), *assertion->strAssertion());
                    if (covered.size() > 1)
                    {
                        logerr("    Missing cases:\n");
                        int numMissing = 0;
                        for (unsigned i = 0 ; i < covered.size() ; i++)
                        {
                            if (!covered[i])
                            {
                                if (numMissing++ == 100)
                                {
                                    logerr("        (additional missing cases omitted)\n");
                                    break;
                                }
                                else
                                    logerr("        %s\n", *assertion->strValue(i));
                            }
                        }
                    }
                }
                if (section->lineCoverage.size())
                    section->lineCoverage[data->id] = it.key();
                covered.assign(covered.size(), false);
            }

            // Check line coverage
            vector<int> &covered = section->lineCoverage;
            int lastId = -1;
            int lastLine = 0;
            int firstMissing = -1;
            for (int id = 0 ; id <= (int) covered.size() ; id++)
            {
                if ((id == (int) covered.size()) || covered[id])
                {
                    if (firstMissing != -1)
                    {
                        ret = false;
                        logerr("Coverage assertion failed in section '%s':\n", *itSection.key);
                        if (firstMissing == id - 1)
                            logerr("    Missing coverage point %d in file %s\n", firstMissing + 1, file);
                        else
                            logerr("    Missing coverage points %d-%d in file %s\n", firstMissing + 1, id, file);
                        firstMissing = -1;
                        if (lastId != -1)
                            logerr("    Note: coverage point %d is at line %d\n", lastId + 1, lastLine);
                        else
                            logerr("    Note: coverage section begins at line %d\n", section->firstLine);
                    }
                    lastId = id;
                    if (id < (int) covered.size())
                    {
                        lastLine = covered[id];
                        covered[id] = 0;
                    }
                }
                else if (firstMissing == -1)
                    firstMissing = id;
            }
            covered.assign(covered.size(), 0);

            // Reset predicated cross-coverage assertions
            for (unsigned i = 0 ; i < section->initializedFlags.size() ; i++)
                *(section->initializedFlags[i]) = false;
        }
    }
    return ret;
}

void CoverageAssertion::_internal_enable (const char *section)
{
    Iterator<CoverageAssertionSectionMap> it(getCoverageAssertionSectionMap(), section, section);
    assert_always(it, "Coverage section \"%s\" does not exist", section);
    for ( ; it ; it++)
    {
        CoverageAssertionSection *section = *it;
        if (!section->enabled)
        {
            section->enabled = true;
            section->lineCoverage.assign(section->lineCoverage.size(), 0);

            // Reset the coverage assertions
            for (Iterator<FileCoverageAssertionMap> it2(section->fileAssertions->assertions, section->firstLine, section->lastLine) ; it2 ; it2++)
            {
                CoverageAssertionData *data = *it2;
                data->numUncovered = (int) data->covered.size();
                data->covered.assign(data->numUncovered, false);
            }

            // Reset predicated cross-coverage assertions
            for (unsigned i = 0 ; i < section->initializedFlags.size() ; i++)
                *(section->initializedFlags[i]) = false;
        }
    }
}

void CoverageAssertion::_internal_disable (const char *section)
{
    Iterator<CoverageAssertionSectionMap> it(getCoverageAssertionSectionMap(), section, section);
    assert_always(it, "Coverage section \"%s\" does not exist", section);
    for ( ; it ; it++)
    {
        CoverageAssertionSection *section = *it;
        if (section->enabled)
        {
            section->enabled = false;

            // Mark the coverage assertions as covered to reduce overhead
            for (Iterator<FileCoverageAssertionMap> it2(section->fileAssertions->assertions, section->firstLine, section->lastLine) ; it2 ; it2++)
                (*it2)->numUncovered = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////
//
// Item coverage
//
////////////////////////////////////////////////////////////////////////
ItemCoverageAssertion::ItemCoverageAssertion (const char *file, int line, int id, CoverageItem *item)
: CoverageAssertion(file, line, id, item->numValues()), m_item(item)
{
}

void ItemCoverageAssertion::cover ()
{
    if (m_item->valid())
    {
        int index = m_item->index();
        if (!data->covered[index])
        {
            data->covered[index] = true;
            data->numUncovered--;
        }
    }
}

string ItemCoverageAssertion::strAssertion () const
{
    return m_item->strItem();
}

string ItemCoverageAssertion::strValue (int index) const
{
    return m_item->strValue(index);
}

////////////////////////////////////////////////////////////////////////
//
// Cross-coverage (explicit items)
//
// index = (index0 * size1 * size2 * ... * sizeN) + (index1 * size2 * ... * sizeN) + ... + indexN
//
////////////////////////////////////////////////////////////////////////
CrossCoverageAssertion::CrossCoverageAssertion (const char *file, int line, int id, int numItems, CoverageItem **items)
                                                : CoverageAssertion(file, line, id, numValues(file, line, numItems, items)), m_items(items)
{
}

int CrossCoverageAssertion::numValues (const char *file, int line, int numItems, CoverageItem **items)
{
    int64 size = 1;
    for (int i = 0 ; i < numItems ; i++)
    {
        size *= items[i]->numValues();
        assert_always(size < MAX_CROSS_COVERAGE_SIZE, 
            "Maximum size (0x%x) exceeded for cross-coverage assertion at\n"
            "    %s(%d)\n", MAX_CROSS_COVERAGE_SIZE, file, line);
    }
    return (int) size;
}

void CrossCoverageAssertion::cover (int numItems)
{
    int index = 0;
    for (int i = 0 ; i < numItems ; i++)
    {
        if (!m_items[i]->valid())
            return;
        index = index * m_items[i]->numValues() + m_items[i]->index();
    }
    if (!data->covered[index])
    {
        data->covered[index] = true;
        data->numUncovered--;
    }
}

string CrossCoverageAssertion::strAssertion (int numItems) const
{
    ostrcaststream oss;
    oss << m_items[0]->strItem();
    for (int i = 1 ; i < numItems ; i++)
        oss << str(" x %s", *m_items[i]->strItem());
    return oss.str();
}

string CrossCoverageAssertion::strValue (int numItems, int index, int *itemIndex) const
{
    int i;
    for (i = numItems - 1 ; i >= 0 ; i--)
    {
        itemIndex[i] = index % m_items[i]->numValues();
        index /= m_items[i]->numValues();
    }
    ostrcaststream oss;
    oss << m_items[0]->strValue(itemIndex[0]);
    for (i = 1 ; i < numItems ; i++)
        oss << str(", %s", *m_items[i]->strValue(itemIndex[i]));
    return oss.str();
}

/////////////////////////////////////////////////////////////////
//
// Predicated cross-coverage
//
/////////////////////////////////////////////////////////////////
PredicatedCrossCoverageAssertion::PredicatedCrossCoverageAssertion (const char *file, int line, int id, int numItems, CoverageItem **items, const char *predicate)
: CrossCoverageAssertion(file, line, id, numItems, items), m_predicate(predicate), m_initialized(false)
{
    FileCoverageAssertions *fileAssertions = getFileCoverageAssertions(file);
    FileCoverageAssertionSectionMap::iterator it = fileAssertions->sections.upper_bound(line);
    it->second->initializedFlags.push_back(&m_initialized);
}
