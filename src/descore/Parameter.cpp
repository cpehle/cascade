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

/////////////////////////////////////////////////////////////////////
//
// Parameter.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Parameter.hpp"
#include "Archive.hpp"
#include "MapIterators.hpp"
#include "Wildcard.hpp"
#include "StringBuffer.hpp"
#include "Trace.hpp"
#include "PrintStrings.hpp"
#include "AssertParams.hpp"

TraceKey(_checkpoint);
/////////////////////////////////////////////////////////////////
//
// Parameter groups
//
/////////////////////////////////////////////////////////////////
string Parameter::parameterGroup (bool update, const string &name)
{
    static string currGroup = "";
    static std::vector<string> prevGroup;

    if (update)
    {
        if (name == "")
        {
            assert(prevGroup.size());
            currGroup = prevGroup.back();
            prevGroup.pop_back();
        }
        else
        {
            prevGroup.push_back(currGroup);
            currGroup += name + ".";
        }
    }
    return currGroup;
}

ParameterGroupHelper::ParameterGroupHelper (const char *name, const char *alt_name)
{
    if (alt_name)
        name = alt_name;
    Parameter::parameterGroup(true, name);
}

typedef std::map<string, IParameterGroupVector *> ParameterGroupVectorMap;

static ParameterGroupVectorMap &getParameterGroupVectorMap ()
{
    static ParameterGroupVectorMap vectors;
    return vectors;
}

void IParameterGroupVector::registerGroupVector (const string &name)
{
    ParameterGroupVectorMap &vectors = getParameterGroupVectorMap();
    assert(vectors.find(name) == vectors.end(), "Parameter group vector %s redefined", *name);
    vectors[name] = this;
}

/////////////////////////////////////////////////////////////////
//
// getParamMap()
//
/////////////////////////////////////////////////////////////////
static ParamMap &getParamMap ()
{
    static ParamMap params;
    return params;
}

static std::vector<string> expandParameterSpecifierString (const string &specifier)
{
    std::vector<string> strings = descore::expandSpecifierString(specifier);
    std::vector<string> ret;
    for (const string &s : strings)
    {
        ret.push_back(s);
        ret.push_back(s + "[*");
        ret.push_back(s + ".*");
        ret.push_back("*." + s);
        ret.push_back("*." + s + "[*");
        ret.push_back("*." + s + ".*");
    }
    return ret;
}

static bool match (const string &name, const std::vector<string> &includes, const std::vector<string> &excludes)
{
    for (const string &s : excludes)
    {
        if (wildcardMatch(*name, *s, false))
            return false;
    }
    for (const string &s : includes)
    {
        if (wildcardMatch(*name, *s, false))
            return true;
    }
    return false;
}

ParamMap Parameter::getParameters (const string &include, const string &exclude)
{
    std::vector<string> includes = expandParameterSpecifierString(include);
    std::vector<string> excludes = expandParameterSpecifierString(exclude);
    ParamMap params;
    for_map_values (Parameter *param, getParamMap())
    {
        if (match(param->getName(), includes, excludes))
            params[param->getName()] = param;
    }
    return params;
}

const ParamMap &Parameter::getParameters ()
{
    return getParamMap();
}

Parameter *Parameter::findParameter (const string &name)
{
    ParamMap &params = getParamMap();
    ParamMap::iterator it = params.find(name);
    if (it == params.end())
        return NULL;
    return it->second;
}

static void foreach_parameter (const string &include, const string &exclude, void (Parameter::*f) ())
{
    std::vector<string> includes = expandParameterSpecifierString(include);
    std::vector<string> excludes = expandParameterSpecifierString(exclude);
    for_map_values (Parameter *param, getParamMap())
    {
        if (match(param->getName(), includes, excludes))
            (param->*f)();
    }
}

/////////////////////////////////////////////////////////////////
//
// getParameter()
//
/////////////////////////////////////////////////////////////////
static Parameter *getParameter (const string &name, bool write = false, bool logerrors = true)
{
    if (write)
    {
        // See if we need to dynamically grow any parameter arrays
        ParameterGroupVectorMap &vectors = getParameterGroupVectorMap();
        const char *szName = *name;
        const char *ch1, *ch2;
        for (ch1 = szName ; (ch1 = strchr(ch1, '[')) != NULL && (ch2 = strchr(ch1, ']')) != NULL ; ch1 = ch2 + 1)
        {
            string group = name.substr(0, ch1 - szName);
            int index = stringTo<int>(name.substr(ch1 - szName + 1, ch2 - ch1 - 1));
            if (vectors.find(group) == vectors.end())
                break;
            if (!vectors[group]->validateIndex(index, logerrors))
                return NULL;
        }
    }

    ParamMap &params = getParamMap();
    ParamMap::iterator it = params.find(name);
    if (it == params.end())
    {
        if (logerrors)
            logerr("Unknown parameter: %s\n", *name);
        return NULL;
    }
    return it->second;
}

/////////////////////////////////////////////////////////////////
//
// setName()
//
/////////////////////////////////////////////////////////////////
void Parameter::setName (const string &name)
{
    m_name = parameterGroup() + name;
    ParamMap &paramMap = getParamMap();
    assert(paramMap.find(m_name) == paramMap.end(), "Parameter %s redefined", *m_name);
    paramMap[m_name] = this;
}

////////////////////////////////////////////////////////////////////////////////
//
// initialize()
//
////////////////////////////////////////////////////////////////////////////////
bool Parameter::initialize (const char *type, const char *description, const char *file, int line)
{
    static std::set<void *, descore::allow_ptr<void *>> s_initializedParameters;

    if (s_initializedParameters.count(this))
        return true;

    if (!strcmp(type, "int") || !strcmp(type, "unsigned") || !strcmp(type, "int64") || !strcmp(type, "uint64"))
        m_type = "integer";
    else if (!strcmp(type, "string") || !strcmp(type, "std::string"))
        m_type = "string";
    else if (!strcmp(type, "bool"))
        m_type = "boolean";
    else if (!strcmp(type, "float") || !strcmp(type, "double"))
        m_type = "float";
    else
        m_type = type;

    if (description)
        m_description = description;
    m_file = file;
    m_line = line;

    s_initializedParameters.insert(this);
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// parseIndex()
//
////////////////////////////////////////////////////////////////////////////////
typedef std::pair<string,string> stringpair_t;
stringpair_t parseIndex (const string &name)
{
    // Skip past group names
    const char *szName = *name;
    const char *ch = szName;
    const char *ch2;
    while ((ch2 = strchr(ch, '.')) != NULL)
        ch = ch2 + 1;
    ch = strchr(ch, '[');
    if (!ch)
        return stringpair_t(name, "");
    int pos = ch - szName;
    return stringpair_t(name.substr(0, pos), name.substr(pos));
}

////////////////////////////////////////////////////////////////////////////////
//
// splitIndex()
//
////////////////////////////////////////////////////////////////////////////////
BEGIN_NAMESPACE_DESCORE

bool splitIndex (const string &index, string &index0, string &index1, string *error_message)
{
    delimited_string val("]");
    istrcaststream iss(index);
    iss >> "[" >> val >> "]";
    if (!iss)
    {
        if (error_message)
            *error_message = str("Could not parse index %s", *index);
        return false;
    }
    index0 = val.val;
    index1 = index.substr(index0.length() + 2);
    return true;
}

END_NAMESPACE_DESCORE

/////////////////////////////////////////////////////////////////
//
// parseCommandLine()
//
/////////////////////////////////////////////////////////////////
static std::vector<stringpair_t> s_commandLineOverrides;
void Parameter::parseCommandLine (int &csz, char *rgsz[])
{
    int numArgs = csz;
    char **srcArgs = rgsz;

    csz = 1;
    for (int i = 1 ; i < numArgs ; i++)
    {
        bool showparams = !strcmp(rgsz[i], "-showparams");
        bool showconfig = !strcmp(rgsz[i], "-showconfig");
        if (showparams || showconfig)
        {
            string name = "*";
            string exclude = "log.*;assert.*";
            if (++i < numArgs)
            {
                name = rgsz[i];
                exclude = "";
            }
            else
                printf("\nUse -showparams '*' to see help for descore parameters\n");
            Parameter::help(name, exclude, showconfig);
            exit(0);
        }

        const char *arg = srcArgs[i];
        if (!strcmp(arg, "-quiet"))
            arg = "-log.quiet";
        if (!strcmp(arg, "-loadparams"))
        {
            assert_always(i < numArgs - 1, "-loadparams requires a filename (-loadparams <file>)");
            Parameter::parseFile(rgsz[i+1]);
            i++;
            continue;
        }

        // Check for -setparam/-tryparam modifiers
        bool setparam = false;
        bool tryparam = false;
        if (!strcmp(arg, "-setparam"))
        {
            assert_always(i < numArgs - 1, "-setparam must be followed by a parameter setting");
            arg = rgsz[++i];
            setparam = true;
        }
        else if (!strcmp(arg, "-tryparam"))
        {
            assert_always(i < numArgs - 1, "-tryparam must be followed by a parameter setting");
            arg = rgsz[++i];
            tryparam = true;
        }

        if (*arg == '-' || setparam || tryparam)
        {
            if (*arg == '-')
                arg++;
            string name = arg;
            size_t pos = name.find('=');
            if (pos == string::npos)
            {
                Parameter *param = getParameter(name, true, false);
                if (param)
                {
                    if (strcmp(param->m_type, "boolean"))
                        die("%s is not a boolean parameter, so a value must be specified", *name);
                    param->setValueInternal("true");
                    s_commandLineOverrides.push_back(stringpair_t(name, "true"));
                    continue;
                }
            }
            else
            {
                string val = name.substr(pos+1);
                name = name.substr(0, pos);
                stringpair_t name_index = parseIndex(name);
                Parameter *param = getParameter(name, true, false);
                if (param)
                {
                    if (param->setValueInternal(val, name_index.second))
                        s_commandLineOverrides.push_back(stringpair_t(name, val));
                    else
                        die("Failed to set %s to %s", *name, *val);
                    continue;
                }
            }
            assert_always(!setparam, "%s is not a valid parameter", *name);
            if (tryparam)
                continue;
        }
        rgsz[csz++] = srcArgs[i];
    }
}

/////////////////////////////////////////////////////////////////
//
// setValueByString()
//
/////////////////////////////////////////////////////////////////
bool Parameter::setValueByString (const string &name, const string &val, bool logerrors)
{
    stringpair_t name_index = parseIndex(name);
    Parameter *param = getParameter(name_index.first, true, logerrors);
    if (!param)
        return false;
    bool orig_disable_value = descore::assertParams.disableDebugBreakpoint;
    descore::assertParams.disableDebugBreakpoint = true;
    bool did_set_param = param->setValueInternal(val, name_index.second, logerrors);
    descore::assertParams.disableDebugBreakpoint = orig_disable_value;
    return did_set_param;
}

/////////////////////////////////////////////////////////////////
//
// getValueAsString()
//
/////////////////////////////////////////////////////////////////
string Parameter::getValueAsString (const string &name)
{
    Parameter *param = getParameter(name);
    if (!param)
        return "";
    return param->getValueAsString();
}

/////////////////////////////////////////////////////////////////
//
// help()
//
/////////////////////////////////////////////////////////////////
static std::vector<string> split (string s)
{
    std::vector<string> ret;
    string::size_type n;
    while ((n = s.find('.')) != string::npos)
    {
        ret.push_back(s.substr(0, n));
        s = s.substr(n+1);
    }
    ret.push_back(s);
    return ret;
}

static void indent (int n)
{
    for (int k = 0 ; k < 4 * n ; k++)
        putchar(' ');
}

static void printSummary (const char *ch, int width)
{
    if (*ch && width >= 20)
    {
        printf("  // ");
        width -= 6;

        const char *end = strchr(ch, '\n');
        if (!end)
            end = ch + strlen(ch);

        // Do we have room for everything?
        if ((!*end || (*end == '\n' && !end[1])) && end - ch <= width)
        {
            printf(ch);
            if (!*end)
                putchar('\n');
            return;
        }

        // Print as many words as we can, followed by "..."
        width -= 3;
        while (1)
        {
            // Parse next word
            const char *space;
            for (space = ch ; *space > ' ' ; space++);

            // Is there room?
            if (space - ch + 1 > width)
                break;

            width -= (space - ch + 1);
            for ( ; ch < space ; ch++)
                putchar(*ch);
            for (ch++ ; *ch && (*ch != '\n') && (*ch <= ' ') ; ch++);
            if (!*ch || *ch == '\n')
                break;
            putchar(' ');
        }
        printf("...\n");
    }
    else if (*ch && width > 8)
        printf("  // ...\n");
    else
        putchar('\n');
}

static string scope (const string &name)
{
    size_t index = name.rfind('.');
    if (index != string::npos)
        return name.substr(0, index);
    return "";
}

void Parameter::help (const string &include, const string &exclude, bool brief)
{
    ParamMap params = getParameters(include, exclude);
    bool first = true;
    std::vector<string> prev;
    printf("\n");

    // Compute the column widths
    int maxNameLen = 4;
    int maxTypeLen = 4;
    struct Group
    {
        int maxNameLen;
        int minValLen;
        int maxValLen;
        char fmt[32];
    };
    std::map<string, Group> groups;
    string curr_scope = "!!invalid!!";
    Group *group = &groups[""];
    for_map_values (Parameter *param, params)
    {
        if (!param->m_hidden)
        {
            // Non-brief accounting
            string name = param->m_name;
            int nameLen = strlen(*name);
            maxNameLen = max(nameLen, maxNameLen);
            int typeLen = strlen(param->m_type);
            maxTypeLen = max(typeLen, maxTypeLen);

            // Brief accounting
            string s = scope(name);
            if (s != "")
                nameLen -= strlen(*s) + 1;
            if (s != curr_scope)
            {
                curr_scope = s;
                group = &groups[name];
                group->maxNameLen = 0;
                group->minValLen = 1000;
                group->maxValLen = 0;
            }
            group->maxNameLen = max(nameLen, group->maxNameLen);
            int valLen = strlen(*param->getDefaultAsString());
            if (valLen < 2)
                valLen = 2;
            group->minValLen = min(valLen, group->minValLen);
            group->maxValLen = min(group->minValLen + 16, max(valLen, group->maxValLen));
        }
    }
    char fmt[32];
    sprintf(fmt, "%%-%ds  %%-%ds  %%s\n", maxNameLen, maxTypeLen);
    for_map_values (Group &g, groups)
        sprintf(g.fmt, "%%-%ds = %%-%ds", g.maxNameLen, g.maxValLen);

    // Print the help
    curr_scope = "!!invalid!!";
    for_map_values (Parameter *param, params)
    {
        if (!param->m_hidden)
        {
            if (brief)
            {
                string name = param->m_name;
                string s = scope(name);
                if (s != curr_scope)
                {
                    curr_scope = s;
                    group = &groups[name];
                }

                unsigned level = 0;
                std::vector<string> curr = split(name);
                for ( ; level < prev.size() && level < curr.size() && prev[level] == curr[level] ; level++);
                
                // Handle the screw case where a parameter group has the same name as a parameter
                if (level == prev.size() && level == curr.size() - 1)
                    level--;
                
                // End scopes
                for (int j = prev.size() - 1 ; j > (int) level ; j--)
                {
                    indent(j-1);
                    printf("}\n");
                }
                
                // Begin scopes
                for (unsigned j = level ; j < curr.size() - 1 ; j++)
                {
                    indent(j);
                    printf("%s\n", *curr[j]);
                    indent(j);
                    printf("{\n");
                }
                
                // Print parameter
                level = curr.size() - 1;
                indent(level);
                name = curr.back();
                string val = param->getDefaultAsString();
                if (val == "")
                    val = "\"\"";
                string nameval = str(group->fmt, *name, *val);
                printf("%s", *nameval);
                
                // Print description
                int width = descore::getConsoleWidth() - 4 * level - nameval.length();
                const char *ch = *param->m_description;
                printSummary(ch, width);
                
                // Done
                prev = curr;
            }
            else
            {
                if (first)
                {
                    printf(fmt, "name", "type", "default");
                    printf(fmt, "----", "----", "-------");
                    first = false;
                }
                param->helpInternal(fmt);
            }
        }
    }
    if (brief)
    {
        for (int j = prev.size() - 1 ; j > 0 ; j--)
        {
            indent(j-1);
            printf("}\n");
        }

    }
    printf("\n");
}

/////////////////////////////////////////////////////////////////
//
// archiveParameters()
//
/////////////////////////////////////////////////////////////////
void Parameter::archiveParameters (Archive &ar, const string &include, const string &exclude)
{
    string end = "";
    if (ar.isLoading())
    {
        while (1)
        {
            string name, value;
            ar >> name >> value;
            if (name == end)
                break;
            Parameter *param = getParameter(name, true);
            assert(param, "Could not load parameters from archive");
            param->setValueInternal(value);
        }
    }
    else
    {
        ParamMap params = getParameters(include, exclude);
        for_map_values (Parameter *param, params)
            ar << param->getName() << param->getValueAsString();
        ar << end << end;
    }
}

/////////////////////////////////////////////////////////////////
//
// resetParameters()
//
/////////////////////////////////////////////////////////////////
void Parameter::resetParameters (const string &include, const string &exclude)
{
    std::vector<string> includes = expandParameterSpecifierString(include);
    std::vector<string> excludes = expandParameterSpecifierString(exclude);

    // Restore default values
    foreach_parameter(include, exclude, &Parameter::reset);

    // Clear dynamic parameter group arrays
    for_map_values (IParameterGroupVector *group, getParameterGroupVectorMap())
    {
        if (match(group->m_name, includes, excludes)) 
            group->reset();
    }

    // Re-apply command-line overrides
    for (stringpair_t p : s_commandLineOverrides)
    {
        if (match(p.first, includes, excludes)) 
        {
            Parameter *param = Parameter::findParameter(p.first);
            if (param) 
                Parameter::setValueByString(p.first, p.second);
        }
    }
}

static ParameterCheckPointState *s_checkpoint_state;
////////////////////////////////////////////////////////////////////////////////
//
// checkpointParameters()
//
////////////////////////////////////////////////////////////////////////////////
void Parameter::checkpointParameters (ParameterCheckPointState &state, const string &include, const string &exclude)
{
    s_checkpoint_state = &state;
    foreach_parameter(include, exclude, &Parameter::createCheckpoint);
    s_checkpoint_state = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// restoreParametersFromCheckpoint()
//
////////////////////////////////////////////////////////////////////////////////
void Parameter::restoreParametersFromCheckpoint (ParameterCheckPointState &state, const string &include, const string &exclude)
{
    s_checkpoint_state = &state;
    foreach_parameter(include, exclude, &Parameter::restoreFromCheckpoint);
    s_checkpoint_state = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// createCheckpoint()
//
////////////////////////////////////////////////////////////////////////////////
void Parameter::createCheckpoint ()
{
    trace(_checkpoint, "Checkpoing %s modified: %d, value: %s\n",
          *m_name, m_modified, *getValueAsString());
    s_checkpoint_state->m_checkpointModified[m_name] = m_modified;
    s_checkpoint_state->m_checkpointValue[m_name] = getValueAsString();
}

////////////////////////////////////////////////////////////////////////////////
//
// restoreFromCheckpoint()
//
////////////////////////////////////////////////////////////////////////////////
void Parameter::restoreFromCheckpoint ()
{
    assert(s_checkpoint_state->m_checkpointModified.count(m_name), 
           "Cannot restore %s from checkpoint: no checkpoint exists for this parameter", *m_name);
    if (getValueAsString() != s_checkpoint_state->m_checkpointValue[m_name])
        trace(_checkpoint, "restore checkpoint %s %s=>%s\n", *m_name, *getValueAsString(), *s_checkpoint_state->m_checkpointValue[m_name]);
    setValueInternal(s_checkpoint_state->m_checkpointValue[m_name]);
    m_modified = s_checkpoint_state->m_checkpointModified[m_name];
}

/////////////////////////////////////////////////////////////////
//
// getOptionsAsString()
//
/////////////////////////////////////////////////////////////////
string Parameter::getOptionsAsString () const
{
    string s;
    for (int i = 0 ; i < numOptionsInternal() ; i++)
    {
        if (i)
            s += ", ";
        s += getOptionAsString(i);
    }
    return s;
}

/////////////////////////////////////////////////////////////////
//
// helpInternal()
//
/////////////////////////////////////////////////////////////////
void Parameter::helpInternal (const char *fmt)
{
    string val = getDefaultAsString();
    if (val == "")
        val = "\"\"";
    printf("\n");
    printf(fmt, *m_name, m_type, *val);
    if (numOptionsInternal())
    {
        printf("    (");
        printHelpString(*getOptionsAsString(), 5);
        printf(")\n");
    }
    if (**m_description)
    {
        printf("    ");
        printHelpString(*m_description, 4);
        printf("\n");
    }
}

/////////////////////////////////////////////////////////////////
//
// printHelpString()
//
/////////////////////////////////////////////////////////////////
void Parameter::printHelpString (const char *sz, int indent)
{
    int col = indent;
    int currIndent = indent;
    const char *ch1, *ch2;
    int width = descore::getConsoleWidth();
    while (*sz)
    {
        // ch1 = start of next word; ch2 = past end of next word
        for (ch1 = sz ; *ch1 && (*ch1 <= ' ') ; ch1++)
        {
            // Tab sets a temporary indent point that is reset by an explicit carriage return
            if (*ch1 == '\t')
            {
                for (ch1 = sz ; *ch1 != '\t' ; ch1++)
                    putchar(*ch1);
                currIndent = col + (ch1 - sz);
                sz = ++ch1;
                col = currIndent;
            }

            // Carriage return forces a new line and restores the original indent
            if (*ch1 == '\n')
            {
                // Ignore trailing carriage returns
                if (ch1[1])
                    putchar(*ch1);
                for (int i = 0 ; i < indent ; i++)
                    putchar(' ');
                sz = ch1 + 1;
                col = indent;
                currIndent = indent;
            }
        }
        for (ch2 = ch1 ; *ch2 > ' ' ; ch2++);
        int len = ch2 - sz;
        if (len + col >= width)
        {
            putchar('\n');
            for (int i = 0 ; i < currIndent ; i++)
                putchar(' ');
            col = currIndent;
            sz = ch1;
        }
        for ( ; sz < ch2 ; sz++, col++)
            putchar(*sz);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// parseFile()
//
////////////////////////////////////////////////////////////////////////////////

#define parse_assert(exp, ...) assert_always(exp, "%s:%d: %s", *s_filename, line_num, *str(__VA_ARGS__))

static FILE *f;
static std::vector<string> prefix;
static std::vector<string> array_name;
static std::vector<int>    array_index;
static char prefetchBuffer[1024];
static char *nextLine;
static strbuff line;
static const char *ch;
static char name[128];
static int line_num = 0;
static string s_filename;

static void prefetch ()
{
    prefetchBuffer[0] = 0;
    nextLine = prefetchBuffer;
    while (!feof(f) && fgets(prefetchBuffer, 1024, f))
    {
        // Strip comments
        char *ch = strstr(prefetchBuffer, "//");
        if (ch)
            *ch = 0;

        // Skip whitespace
        for (nextLine = prefetchBuffer ; *nextLine && *nextLine <= ' ' ; nextLine++);

        // Return if the line is non-empty
        if (*nextLine)
        {
            // Strip whitespace
            for (ch = nextLine + strlen(nextLine) - 1 ; *ch <= ' ' ; *ch-- = 0);
            return;
        }
    }
}

static bool readLine ()
{
    if (!*nextLine)
        return false;

    line.clear();
    line.puts(nextLine);
    prefetch();
    ch = line;
    line_num++;
    return true;
}

static void appendLine ()
{
    assert_always(nextLine, "Unexpected end of file");
    line.putch('\n');
    line.puts(nextLine);
    prefetch();
    line_num++;
}

#define DIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define UPPER(ch) ((ch) >= 'A' && (ch) <= 'Z')
#define LOWER(ch) ((ch) >= 'a' && (ch) <= 'z')
#define ALPHA(ch) (UPPER(ch) || LOWER(ch) || ((ch) == '_'))
#define ALPHANUM(ch) (DIGIT(ch) || ALPHA(ch)) 

static bool parseName ()
{
    if (!ALPHANUM(*ch))
        return false;

    descore::delimited_string val("=");
    istrcaststream iss(ch);
    iss >> val;
    if (!iss)
        return false;
    strcpy(name, *val.val);
    ch += strlen(name);
    for ( ; *ch && *ch <= ' ' ; ch++);
    return true;
}

static void skipws ()
{
    for (ch++ ; *ch && *ch <= ' ' ; ch++);
}

#define ARRAY_PREFIX "<array>"

void Parameter::parseFile (const string &filename, IParseCallback *callback)
{
    s_filename = filename;
    f = fopen(*filename, "r");
    assert_always(f, "Could not open %s", *filename);

    // Initialize
    line_num = 0;
    prefix.clear();
    prefix.push_back("");
    prefetch();

    // Parse
    readLine();
    while (*ch || readLine())
    {
        // End of group?
        if (*ch == '}')
        {
            parse_assert(prefix.back() != ARRAY_PREFIX, "Expected ']'");
            parse_assert(prefix.size() > 1, "Unexpected '}'");
            prefix.pop_back();
            skipws();
            continue;
        }

        // End of array?
        if (*ch == ']')
        {
            parse_assert(prefix.back() == ARRAY_PREFIX, "Unexpected ']'");
            prefix.pop_back();
            array_name.pop_back();
            array_index.pop_back();
            skipws();
            continue;
        }

        bool parsingArray = (prefix.back() == ARRAY_PREFIX);
        if (parsingArray || parseName())
        {
            // Beginning of group array?
            if (*ch == '[' || (!*ch && *nextLine == '['))
            {
                parse_assert(!parsingArray);
                array_name.push_back(prefix.back() + name);
                array_index.push_back(0);
                prefix.push_back(ARRAY_PREFIX);
                if (!*ch)
                    readLine();
                skipws();
                continue;
            }

            // Beginning of group?
            if (*ch == '{' || (!*ch && *nextLine == '{'))
            {
                if (parsingArray)
                    prefix.push_back(str("%s[%d].", *array_name.back(), array_index.back()++));
                else
                    prefix.push_back(prefix.back() + name + ".");
                if (!*ch)
                    readLine();
                skipws();
                continue;
            }

            // Assignment?
            if (!parsingArray && *ch == '=')
            {
                // Get the parameter name and find the start of the value
                string paramName = name;
                skipws();
                int ival = ch - line;

                // Get the parameter
                paramName = prefix.back() + paramName;
                stringpair_t name_index = parseIndex(paramName);
                Parameter *parameter = getParameter(name_index.first, true, false);
                parse_assert(parameter, "Unknown parameter '%s'", *name_index.first);
                
                // Keep parsing lines until we've read the value and balanced all parentheses ((), [], {}).
                int icurr = ival;
                int depth = 0;
                for (;;)
                {
                    for ( ; line[icurr] ; icurr++)
                    {
                        if (line[icurr] == '(' || line[icurr] == '[' || line[icurr] == '{')
                            depth++;
                        else if (line[icurr] == ')' || line[icurr] == ']' || line[icurr] == '}')
                        {
                            if (!depth)
                            {
                                parse_assert(icurr != ival, "Expected value");
                                break;
                            }
                            depth--;
                        }
                    }
                    if (!depth && (icurr != ival))
                        break;
                    appendLine();
                    icurr++;
                    if (line[ival] == '\n')
                        ival++;
                }

                // Set the parameter
                string strline = *line;
                string value = strline.substr(ival, icurr - ival);
                bool ret = parameter->setValueInternal(value, name_index.second);
                parse_assert(ret, "Failed to set %s", *paramName);
                ch = line + icurr;
                continue;
            }
        }

        // User-defined?
        bool ret = callback && callback->parseLine(line);
        parse_assert(ret, "Failed to parse '%s'", *line);
    }
    parse_assert(prefix.size() == 1, "Unexpected end of file");

    fclose(f);
}

////////////////////////////////////////////////////////////////////////////////
//
// generateFile()
//
////////////////////////////////////////////////////////////////////////////////
void Parameter::generateFile (const string &filename, const string &include, const string &exclude)
{
    ParamMap params = getParameters(include, exclude);
    FILE *f = fopen(*filename, "w");
    assert_always(f, "Could not open %s", *filename);
    for_map_values (Parameter *param, params)
        fprintf(f, "%s = %s\n", *param->getName(), *param->getValueAsString());
    fclose(f);
}

////////////////////////////////////////////////////////////////////////////////
//
// Callbacks
//
////////////////////////////////////////////////////////////////////////////////
void Parameter::addCallback (IParameterChangeCallback *callback)
{
    m_callbacks.push_back(callback);
}

void Parameter::notifyChange ()
{
    for (unsigned i = 0 ; i < m_callbacks.size() ; i++)
        m_callbacks[i]->notifyChange(this);
}
