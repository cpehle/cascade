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
// Coverage.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 05/16/2007
//
//
// 1. Overview
//    --------
//
// A coverage assertion ensures that one or more conditions are met at some 
// point during program execution.  Unlike a regular assertion which must be
// true every time it is evaluated, a coverage assertion keeps track of the
// various states of interest that are encountered during execution, and is
// not actually tested for success/failure until after the program has been
// run.  Each coverage assertion is associated with a (file, line) pair and
// updates its state whenever execution reaches that particular line.
//
// All coverage assertions must appear within coverage assertion *sections*
// which are delineated by the following macros:
//
//    BEGIN_COVERAGE(<section name>);
//    ...
//    <code and coverage assertions>
//    ...
//    END_COVERAGE();
//
// These macros must appear at file scope.  Coverage sections cannot be
// nested or overlapped.
//
//
// 2. Line coverage
//    -------------
//
// The simplest type of coverage assertion is a line coverage assertion:
//
//    coverLine();
//
// This assertion simply validates that the program execution at some point
// encountered that specific line.
//
//
// 3. Basic coverage
//    --------------
//
// The following coverage assertions each verify that a single variable
// or expression takes on all specified values during the course of program
// execution.  The assertions support only 32-bit integer values (except for
// coverBool which supports booleans).
//
//    coverAssert(expr);
//
// Asserts that expr is true at least once.
//
//    coverBool(expr);
//
// Asserts that the boolean expression takes on both possible values
// (true, false) at least once.
//
//    coverRange(expr, low, high)
//
// Asserts that the integer expression takes on every value from low to 
// high (inclusive) at least once.
//
//    coverRangePredicated(expr, low, high, predicate)
//
// Asserts that the integer expression takes on every value from low to
// high (inclusive) for which the predicate is true.  'predicate' is an
// expression which contains a single variable named "value" and is evaluated 
// for every integer in [low, high].  It should evaluate to a boolean; an 
// integer is included in the set of coverage values if 'predicate' evaluates 
// to true with the integer assigned to "value".
//
//    coverValues(expr, value1, value2, ...)
//
// Asserts that the integer expression takes on every explicitly listed
// value at least once.  There is no limit to the number of values which
// can be supplied.
//
// examples:
//
//    coverAssert(x > 7);
//    coverBool(op == OP_NOP);
//    coverRange((adress >> 4) & 0x3f, 0, 63);
//    coverRangePredicated(base + offset, 0, 127, (value & 0xf) != 0);
//    coverValues(p, 2, 3, 5, 7, 11, 13, 17, 19);
//
// As a convenience, the following macros, which correspond exactly to the 
// five macros above, can be used to simultaneously return a result from a 
// function and provide coverage on its value:
//
//    returnCovered(expr)
//    returnCoveredBool(expr)
//    returnCoveredRange(expr, low, high)
//    returnCoveredRangePredicated(expr, low, high, predicate)
//    returnCoveredValues(expr, value1, value2, ...)
//
// The return type should be bool for returnCoveredBool and should be an
// integer type with at most 32 bits for the others.  These macros expand to 
// a block enclosed by curly braces, e.g.
//
//    returnCoveredBool(isZero);
//
// expands to
//
//    {
//        bool _returnValue = isZero;
//        coverBool(_returnValue);
//        return _returnValue;
//    }
//
// As such, it is safe to use these macros in single-line if clauses, but the
// semicolon at the end of the line must be omitted if there is a subsequent else:
//
//    if (done)
//        returnCoveredBool(isZero)
//    else
//        return false;
//
// Finally, also as a convenience, the following macro can be used to  
// simultaneously begin an if clause and cover the boolean conditional
// expression:
//
//    cover_if (expr)
//        ...
//
// This macro expands to
//
//    coverBool(expr);
//    if (expr)
//        ....
//
// So there are two restrictions on its use:
//
// 1) expr cannot have side effects.
//
//    cover_if (++n == 7)   // WRONG!!!
//        ...
//
//    ++n;
//    cover_if (n == 7)     // Correct
//        ...
//
// 2) cover_if cannot appear in a single-line clause.
//
//    for (i = 0 ; i < 10 ; i++) // WRONG!!!
//        cover_if (active[i])  
//            numActive++;
//
//    for (i = 0 ; i < 10 ; i++) // Correct
//    {
//        cover_if (active[i])  
//            numActive++;
//    }
//
//
// 4. Cross coverage
//    --------------
//
// A cross coverage assertion verifies that multiple variables or expressions
// take on all possible combinations of values during program execution.  A
// cross coverage assertion is constructed in two steps.  First, two or more
// named coverage items must be declared: each coverage item defines a set
// of values of interest for a single variable or expression.  Second, the
// cross coverage assertion itself is declared which specifies a set of items.
// 
// Coverage items are declared using the following macros:
//
//    CoverageItemBool(name, expr)
//    CoverageItemRange(name, expr, low, high)
//    CoverageItemRangePredicated(name, expr, low, high, predicate)
//    CoverageItemValues(name, expr, value1, value2, ...)
//
// These macros have the same format and meaning as the corresponding coverage
// assertions but with two differences.  First, they all take an initial 'name'
// parameter which is used to declare a coverage item variable having that 
// name (so in particular the name cannot collide with any other variable
// names).  Second, rather than asserting that 'expr' must take on all 
// prescribed values, they simply define the set of values for later use.
//
// Once two or more items, have been defined, a cross coverage assertion
// can be declared as follows:
//
//    coverCross(item1, item2, ...)
//
// The items are specified by name, which must match expactly the names used 
// in the CoverageItem macros.  There is no limit to the number of coverage
// items which can be specified in a cross coverage assertion.
//
// example:
//
//    CoverageItemBool(item_carry, carry);
//    CoverageItemValues(item_op, op, OP_ADD, OP_SUB, OP_MUL);
//    CoverageItemBool(item_compare, a > b);
//    coverCross(item_carry, item_op, item_compare);
//
// A cross coverage assertion can also supply a predicate which indicates
// which combinations of values should be covered:
//
//    coverCrossPredicated(predicate, item1, item2, ...)
//
// 'predicate' is an expression which contains the variables item1, item2, ...
// Note that this differs from the predicate supplied to coverRangePredicated
// which contains a single variable named "value".  In this case the items
// already have names, so these names are used directly within the predicate.
// Only those combinations of values for which the predicate holds are
// considered for coverage.  The following example covers all combinations
// of two integers where the sum of the integers is odd:
//
//    CoverageItemRange(item_a, 0, 10);
//    CoverageItemRange(item_b, 10, 20);
//    coverCrossPredicated((item_a ^ item_b) & 1, item_a, item_b);
//
//
// 5. Controlling coverage
//    --------------------
//
// The preprocessor symbol _COVERAGE must be defined to support coverage
// assertions.  All coverage assertions are initially disabled; assertions 
// can be dynamically enabled or disabled on a section-by-section basis
// using the following functions:
//
//    CoverageAssertion::enable(const char *section);
//    CoverageAssertion::disable(const char *section);
//
// These functions enable/disable the specified coverage section(s), where
// 'section' must match exactly the section name specified in the
// BEGIN_COVERAGE() macro.  If multiple sections have the same name, then 
// they are enabled/disabled as a group.
//
// Enabling or disabling coverage assertions for a section has the effect of 
// resetting all the assertions in that section, so these calls should only be
// used between runs to set up assertions for the next run.
//
// At the end of a run, the coverage assertions must be explicitly evaluted
// to generate a report of failed assertions using the following function call:
//
//    CoverageAssertion::checkAndReset();
//
// This function validates all coverage assertions in all files with assertions
// enabled, then resets these assertions.  Any coverage assertion failures will 
// be logged and reported to stderr.  For coverage assertions with multiple
// values, the missing cases will be listed explicitly up to a maximum of
// 100 cases.  This function returns true if all coverage assertions were
// satisfied and false otherwise.
//
// An important technical note is that the __COUNTER__ mechanism, supported in
// Microsoft builds, is the only mechanism which can detect coverage assertions
// that are never visited.  In Microsoft builds, these missed assertions will
// be reported as coverage assertion failures with the message
//
// "Missing coverage point <#> in file <filename>"
//
// Every line, basic or cross coverage assertion in a section is a coverage point;
// the coverage points in a section are numbered sequentially starting with 1.
// In non-Microsoft builds, missing coverage points will result in silent
// failures.
//
//
// 6. Other error messages
//    --------------------
//
// "Missing BEGIN_COVERAGE() before END_COVERAGE()"
//
// Indicates an END_COVERAGE() macro with no corresponding BEGIN_COVERAGE().
//
// "Missing END_COVERAGE() for coverage section <name>"
//
// Indicates a BEGIN_COVERAGE() macro with no corresponding END_COVERAGE().
// Note that coverage sections cannot be nested.
//
// "Warning: inconsistent counter detected in file <filename>
//      Line coverage has been disabled for this file."
//
// Indicates that a header file with line coverage has a different value
// of __COUNTER__ depending on which source file it is being included from.
// This problem can be fixed by ensuring that whenever two source files
// include a common header file which contains coverage assertions, then
// they include exactly the same set of header files with coverage assertions
// and in the same order.  Where possible, this problem can also be avoided
// by moving the code (along with the coverage assertions) to a source file.
//
// "Coverage assertion must appear within a coverage section"
//
// Indicates that the specified coverage assertion is not in a coverage 
// section defined by BEGIN_COVERAGE()/END_COVERAGE().
//
// "Maximum size (0x10000000) exceeded for cross-coverage assertion"
//
// The total number of value combinations in a cross-coverage assertion
// is not allowed to exceed 2^28 (~256M combinations).
//
//
// 7. Templates
//    ---------
//
// A coverage assertion within a templated function (or class) will in 
// general give rise to multiple instances which share the same underlying
// bookeeping.  It is important to ensure that the coverage values do not
// depend on the template parameters, but the variable/expression which
// is being covered (and takes on these values) may do so.  For example,
// the following is incorrect:
//
// template <int n> void someFunction (int k)
// {
//     coverValues(k, n-1, n, n+1);   // WRONG
// }
//
// whereas the following is correct:
//
// template <int n> void someFunction (int k)
// {
//     coverValues(k-n, -1, 0, 1);
// }
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Coverage_hpp_
#define descore_Coverage_hpp_

#include <string.h>
#include "strcast.hpp"
#include "Iterators.hpp"
#include <vector>

/////////////////////////////////////////////////////////////////
//
// Coverage sections
//
// A coverage assertion section is defined by a range of line numbers
// within a file.
//
/////////////////////////////////////////////////////////////////

struct FileCoverageAssertions;

class CoverageAssertionSection
{
    friend class CoverageAssertion;
    friend class PredicatedCrossCoverageAssertion;
public:
    // Constructor called by BEGIN_COVERAGE
    CoverageAssertionSection (const char *name, const char *file, int line, int id);

    // Called by END_COVERAGE
    static void endSection (const char *file, int line, int id);

private:
    void endSection (int line, int id);

private:
    // Pointer to the coverage assertions in the file containing this section
    FileCoverageAssertions *fileAssertions;

    // The range of line numbers defining this section
    int firstLine;
    int lastLine;

    // The range of assertion ids in this section
    int firstId;
    int lastId;

    // Flags indicating whether or not the assertions in this section are enabled
    bool enabled;

    // id => line number for line coverage (size 0 if line coverage is disabled)
    std::vector<int> lineCoverage;

    // Vector of pointers to 'initialized' flags for predicated cross-coverage
    // assertions, which must be cleared whenever this section is enabled.
    std::vector<bool *> initializedFlags;

    // Coverage assertion section currently being defined
    static CoverageAssertionSection *s_currSection;
    static const char *s_currName;
    static const char *s_currFile;
};

struct EndCoverageSection
{
    EndCoverageSection (const char *file, int line, int id)
    {
        CoverageAssertionSection::endSection(file, line, id);
    }
};

// This macro must appear at the start of a source file in order to enable
// line coverage for that file
#define BEGIN_COVERAGE(name) \
    static CoverageAssertionSection CONCAT(s_beginLineCoverage,__LINE__)(name, __FILE__, __LINE__, __COUNTER__ + 1)

// This macro must appear at the end of a source file in order to enable
// line coverage for that file.
#define END_COVERAGE() \
    static EndCoverageSection CONCAT(s_beginLineCoverage,__LINE__)(__FILE__, __LINE__, __COUNTER__ - 1)


////////////////////////////////////////////////////////////////////////
//
// Item base class
//
// A coverage item is a variable or expression that takes on a finite
// number of distinct coverage values.  The base class exposes this number.
// Derived classes implement the enumeration of the values by mapping from a
// value to an index (between 0 and size-1) or from an index to a value.
// At run time, each time the coverage item is encountered the variable
// or expression is evaluated, and the index of the current value is stored 
// for retrieval by any coverage assertions which reference the item.
//
////////////////////////////////////////////////////////////////////////

class CoverageItem
{
    DECLARE_NOCOPY(CoverageItem);
public:
    CoverageItem (const char *expr, int numValues) : m_expr(expr), m_numValues(numValues) 
    {
    }
    virtual ~CoverageItem ()
    {
    }

    // Get/set the index of the variable/expression's current value
    inline int index () const 
    { 
        return m_index; 
    }
    inline void setIndex (int index)
    {
        m_index = index;
    }

    // The coverage item is valid if the variable/expression's value
    // is one of the prescribed coverage values.  If the item is invalid
    // then the index is guaranteed to be negative or >= m_numValues.
    inline bool valid () const 
    { 
        return (unsigned) m_index < (unsigned) m_numValues; 
    }

    // Return the number of distinct coverage values for this item
    inline int numValues () const 
    { 
        return m_numValues; 
    }

    // Return a string representation of the specified value
    virtual string strValue (int index) const
    {
        if (strchr(m_expr, ' '))
            return str("(%s) = %d", m_expr, value(index));
        return str("%s = %d", m_expr, value(index));
    }

    // Return a string representing the coverage item as a whole
    virtual string strItem () const
    {
        return "";
    }

    // Translation from index to actual value
    virtual int value (int index) const = 0;

    // Conversion to int for predicated cross-coverage
    operator int () const
    {
        return value(m_index);
    }

protected:
    const char *m_expr; // Variable/expression string captured by item macro
    int m_numValues;    // Number of distinct coverage values
    int m_index;        // Index of current value (< 0 or >= m_size when invalid)
};

// Structure used to capture a variable length list of items in coverCross
struct CoverageItemPtr 
{
    CoverageItemPtr (CoverageItem &item) : ptr(&item) {}
    CoverageItem *ptr;
};

////////////////////////////////////////////////////////////////////////
//
// Range Item
//
// An item whose coverage values are [low, high] inclusive.
//
////////////////////////////////////////////////////////////////////////
class CoverageItemRange : public CoverageItem
{
    DECLARE_NOCOPY(CoverageItemRange);
public:
    CoverageItemRange (const char *expr, int low, int high) 
        : CoverageItem(expr, high - low + 1), m_low(low) 
    {
    }
    inline void setValue (int val) 
    { 
        m_index = val - m_low; 
    }
    virtual int value (int index) const 
    { 
        return index + m_low; 
    }
    virtual string strItem () const
    {
        return str("(%s from %d to %d)", m_expr, m_low, m_low + m_numValues - 1);
    }

private:
    int m_low;
};

// Macro
#define CoverageItemRange(name, expr, low, high) \
    static CoverageItemRange name(#expr, low, high); \
    name.setValue(expr)

////////////////////////////////////////////////////////////////////////
//
// Predicated Range Item
//
// An item whose coverage values are a subset of [low, high], as 
// determined by a supplied predicate.
//
////////////////////////////////////////////////////////////////////////
class CoverageItemRangePredicated : public CoverageItem
{
    DECLARE_NOCOPY(CoverageItemRangePredicated);
public:
    CoverageItemRangePredicated (const char *expr, int low, int high, const char *predicate) 
        : CoverageItem(expr, 0), m_low(low), m_high(high), m_predicate(predicate), m_initialized(false)
    {
        m_indexMap = new int[high - low + 1];
    }
    virtual ~CoverageItemRangePredicated ()
    {
        delete[] m_indexMap;
    }

    // Function called by declaring macro to initialize index map.  Called
    // once for every value in [low, high] to indicate whether or not the value
    // is a valid coverage value.
    void setPredicate (int val, bool p)
    {
        m_indexMap[val - m_low] = p ? m_numValues++ : (m_high + 1);
    }

    inline void setValue (int val) 
    { 
        m_index = (val >= m_low && val <= m_high) ? m_indexMap[val] : m_high; 
    }
    virtual int value (int index) const 
    { 
        for (int val = m_low ; val <= m_high ; val++)
        {
            if (m_indexMap[val] == index)
                return val;
        }
        return m_high + 1;
    }
    virtual string strItem () const
    {
        return str("(%s from %d to %d where %s)", m_expr, m_low, m_high, m_predicate);
    }

    // This function returns true the first time it is called to indicate that
    // the index map and number of values need to be initialized.
    inline bool initialized ()
    {
        if (m_initialized)
            return true;
        m_initialized = true;
        return false;
    }

private:
    int m_low;
    int m_high;
    const char *m_predicate;    // Predicate string captured by coverage item macro
    int *m_indexMap;            // Map from value to index
    bool m_initialized;
};

// Macro
#define CoverageItemRangePredicated(name, expr, low, high, predicate) \
    static CoverageItemRangePredicated name(#expr, low, high, #predicate); \
{ \
    if (!name.initialized()) \
    { \
        for (int value = low ; value <= high ; value++) \
        name.setPredicate(value, predicate); \
    } \
} \
    name.setValue(expr)


////////////////////////////////////////////////////////////////////////
//
// Bool Item
//
// An item with two values: true (1) and false (0).
//
////////////////////////////////////////////////////////////////////////
class CoverageItemBool : public CoverageItem
{
    DECLARE_NOCOPY(CoverageItemBool);
public:
    CoverageItemBool (const char *expr) : CoverageItem(expr, 2) 
    {
    }
    inline void setValue (int val) 
    { 
        m_index = val ? 1 : 0; 
    }
    virtual string strValue (int index) const
    {
        if (strchr(m_expr, ' '))
            return str("(%s) is %s", m_expr, index ? "true" : "false");
        return str("%s is %s", m_expr, index ? "true" : "false");
    }
    virtual string strItem () const
    {
        return str("(%s ?)", m_expr);
    }
    virtual int value (int index) const
    {
        return index;
    }
};

// Macro
#define CoverageItemBool(name, expr) \
    static CoverageItemBool name(#expr); \
    name.setValue(expr)

////////////////////////////////////////////////////////////////////////
//
// Values Item
//
// An item whose values are explicitly specified by the programmer.
//
////////////////////////////////////////////////////////////////////////

template <int n>
class CoverageItemValues : public CoverageItem
{
    DECLARE_NOCOPY(CoverageItemValues);
public:
    CoverageItemValues (const char *expr, const int *values) 
        : CoverageItem(expr, n), m_values(values) 
    {
    }
    inline void setValue (int val)
    {
        for (m_index = 0 ; (m_values[m_index] != val) && (m_index < n) ; m_index++);
    }
    virtual int value (int index) const 
    { 
        return m_values[index]; 
    }
    virtual string strItem () const
    {
        ostrcaststream oss;
        oss << str("(%s in {", m_expr);
        for (int i = 0 ; i < n ; i++)
            oss << str("%s%d", i ? ", " : "", m_values[i]);
        oss << str("})");
        return oss.str();
    }

private:
    const int *m_values;    // Explicit list of values
};

// Macro
#define CoverageItemValues(name, expr, ...) \
    static const int name##__values[] = { __VA_ARGS__ }; \
    static const int name##__numValues = sizeof(name##__values) / sizeof(int); \
    static CoverageItemValues<name##__numValues> name(#expr, name##__values); \
    name.setValue(expr)

/////////////////////////////////////////////////////////////////
//
// Data structure used to maintain the dynamic state of a
// coverage assertion, shared by all instances of the assertion
// at the same (file, line).
//
/////////////////////////////////////////////////////////////////

class CoverageAssertion;

struct CoverageAssertionData
{
    int id;                          // id for line coverage
    int numUncovered;                // number of items that still need to be covered
    std::vector<bool> covered;       // bitmask of covered items
    CoverageAssertion *assertion;    // first assertion instance a this (file, line)
};

/////////////////////////////////////////////////////////////////////////////
//
// Coverage assertion base class
//
/////////////////////////////////////////////////////////////////////////////
class CoverageAssertion
{
    DECLARE_NOCOPY(CoverageAssertion);
public:
    CoverageAssertion (const char *file, int line, int id, int numValues);
    virtual ~CoverageAssertion( ) { }

    static bool checkAndReset ();

#ifdef _COVERAGE
    static void enable (const char *section)
    {
        _internal_enable(section);
    }
    static void disable (const char *section)
    {
        _internal_disable(section);
    }
#else
    static void enable (const char * /* section */) {}
    static void disable (const char * /* section */) {}
#endif

    // Return a string representation of the assertion
    virtual string strAssertion () const = 0;

    // Return a string representation of the specified value
    virtual string strValue (int /* index */) const
    {
        return "";
    }

    // Return the number of coverage values
    inline int numValues () const
    {
        return (int) data->covered.size();
    }

private:
    static void _internal_enable (const char *section);
    static void _internal_disable (const char *section);

public:
    CoverageAssertionData *data;    // shared coverage assertion state
};

/////////////////////////////////////////////////////////////////////////////
//
// Basic coverage assertion: remembers whether or note the condition has
// been met.
//
/////////////////////////////////////////////////////////////////////////////
class BasicCoverageAssertion : public CoverageAssertion
{
    DECLARE_NOCOPY(BasicCoverageAssertion);
public:
    BasicCoverageAssertion (const char *file, int line, int id, const char *expr)
        : CoverageAssertion(file, line, id, 1), m_expr(expr)
    {
    }

    virtual string strAssertion () const
    {
        return m_expr;
    }

protected:
    const char *m_expr;
};

// Basic coverage macro
#define coverAssert(expr) \
{ \
    static BasicCoverageAssertion ca(__FILE__, __LINE__, __COUNTER__, #expr); \
    if (ca.data->numUncovered) \
        ca.data->numUncovered = !(expr); \
}

/////////////////////////////////////////////////////////////////
//
// Line coverage
//
/////////////////////////////////////////////////////////////////
struct LineCoverageAssertion : public CoverageAssertion
{
    LineCoverageAssertion (const char *file, int line, int id)
        : CoverageAssertion(file, line, id, 1)
    {
    }

    virtual string strAssertion () const
    {
        return "Line coverage assertion";
    }
};

// Line coverage macro
#define coverLine() \
{ \
    static LineCoverageAssertion ca(__FILE__, __LINE__, __COUNTER__); \
    ca.data->numUncovered = 0; \
}

/////////////////////////////////////////////////////////////////
//
// Single-item coverage
//
/////////////////////////////////////////////////////////////////
class ItemCoverageAssertion : public CoverageAssertion
{
    DECLARE_NOCOPY(ItemCoverageAssertion);
public:
    ItemCoverageAssertion (const char *file, int line, int id, CoverageItem *item);
    void cover ();
    string strAssertion () const;
    string strValue (int index) const;

private:
    CoverageItem *m_item;
};

// Internal item coverage macro
#define _coverItem(item) \
{ \
    static ItemCoverageAssertion ca(__FILE__, __LINE__, __COUNTER__, &item); \
    if (ca.data->numUncovered) \
        ca.cover(); \
}

// Single-item macros (implicit items)
#define coverRange(expr, low, high) \
{ \
    CoverageItemRange(item, expr, low, high); \
    _coverItem(item); \
} 

#define coverRangePredicated(expr, low, high, predicate) \
{ \
    CoverageItemRangePredicated(item, expr, low, high, predicate); \
    _coverItem(item); \
}

#define coverBool(expr) \
{ \
    CoverageItemBool(item, expr); \
    _coverItem(item); \
}

#define coverValues(expr, ...) \
{ \
    CoverageItemValues(item, expr, __VA_ARGS__); \
    _coverItem(item); \
}

#define cover_if(expr) \
    coverBool(expr) \
    if (expr)

////////////////////////////////////////////////////////////////////////
//
// Cross-coverage (explicit items)
//
////////////////////////////////////////////////////////////////////////

#define MAX_CROSS_COVERAGE_SIZE  0x10000000

// The base class is fully functional with the exception that it doesn't
// know how many coverage items there are.  Some template magic is used
// to supply this information at compile time.
class CrossCoverageAssertion : public CoverageAssertion
{
    DECLARE_NOCOPY(CrossCoverageAssertion);
public:
    CrossCoverageAssertion (const char *file, int line, int id, int numItems, CoverageItem **items);

    // Compute the total number of combinations of values (i.e. the product
    // of the number of coverage values in each item).
    static int numValues (const char *file, int line, int numItems, CoverageItem **items);

    // Mark the current combination of values as covered
    void cover (int numItems);

    // Return a string representing the entire cross coverage assertion
    string strAssertion (int numItems) const;

    // Return a string representing a single combination of values
    string strValue (int numItems, int index, int *itemIndex) const;

protected:
    CoverageItem **m_items;
};

// Templated derived class used to statically supply the number
// of coverage items at compile time.
template <int numItems>
class CrossCoverageAssertionN : public CrossCoverageAssertion
{
    DECLARE_NOCOPY(CrossCoverageAssertionN);
public:
    CrossCoverageAssertionN (const char *file, int line, int id, CoverageItem **items)
        : CrossCoverageAssertion(file, line, id, numItems, items)
    {
    }
    inline void cover ()
    {
        CrossCoverageAssertion::cover(numItems);
    }
    virtual string strAssertion () const
    {
        return CrossCoverageAssertion::strAssertion(numItems);
    }
    virtual string strValue (int index) const
    {
        int itemIndex[numItems];
        return CrossCoverageAssertion::strValue(numItems, index, itemIndex);
    }
};

// Cross-coverage macro
#define coverCross(...) \
{ \
    static const CoverageItemPtr items[] = { __VA_ARGS__ }; \
    const int numItems = sizeof(items) / sizeof(CoverageItemPtr); \
    static CrossCoverageAssertionN<numItems> ca(__FILE__, __LINE__, __COUNTER__, (CoverageItem **) items); \
    if (ca.data->numUncovered) \
        ca.cover(); \
}

/////////////////////////////////////////////////////////////////
//
// Predicated cross-coverage
//
// Same as cross-coverage, but (1) use a predicate to initially
// mark combinations as covered, and (2) store the predicate
// as a string.
//
/////////////////////////////////////////////////////////////////
class PredicatedCrossCoverageAssertion : public CrossCoverageAssertion
{
    DECLARE_NOCOPY(PredicatedCrossCoverageAssertion);
public:
    PredicatedCrossCoverageAssertion (const char *file, int line, int id, int numItems, CoverageItem **items, const char *predicate);

    // Return a string representing the entire predicated cross coverage assertion
    string strAssertion (int numItems) const
    {
        return str("%s where %s", *CrossCoverageAssertion::strAssertion(numItems), m_predicate);
    }

    // This function returns true the first time it is called to indicate that 
    // the assertion needs to be initialized by marking values which do not 
    // satisfy the predicate as covered.
    inline bool initialized ()
    {
        if (m_initialized)
            return true;
        m_initialized = true;
        copyIndices(true);
        return false;
    }
    virtual void copyIndices (bool save) = 0;

protected:
    const char *m_predicate;
    bool m_initialized;
};

// Templated derived class used to statically supply the number
// of coverage items at compile time.
template <int numItems>
class PredicatedCrossCoverageAssertionN : public PredicatedCrossCoverageAssertion
{
    DECLARE_NOCOPY(PredicatedCrossCoverageAssertionN);
public:
    PredicatedCrossCoverageAssertionN (const char *file, int line, int id, CoverageItem **items, const char *predicate)
        : PredicatedCrossCoverageAssertion(file, line, id, numItems, items, predicate)
    {
    }
    inline void cover ()
    {
        PredicatedCrossCoverageAssertion::cover(numItems);
    }
    virtual string strAssertion () const
    {
        return PredicatedCrossCoverageAssertion::strAssertion(numItems);
    }
    virtual string strValue (int index) const
    {
        int itemIndex[numItems];
        return PredicatedCrossCoverageAssertion::strValue(numItems, index, itemIndex);
    }

    // Copy indices is called twice at initialization time.  The first time it
    // is called with save == true.  The item indices are saved and reset to
    // zero.  This allows the item indices to be set iteratively when computing
    // which combinations satisfy the predicate.  The second time it is called 
    // with save == false, and the item indices are restored to their saved
    // values.
    virtual void copyIndices (bool save)
    {
        static int index[numItems];
        for (int i = 0 ; i < numItems ; i++)
        {
            if (save)
            {
                index[i] = m_items[i]->index();
                m_items[i]->setIndex(0);
            }
            else
                m_items[i]->setIndex(index[i]);
        }
    }

    // This function is used to iterate over all possible combinations of
    // item indices when computing the predicate.  It returns true if it
    // has advanced to the next combination of indices or false if it
    // has reached the end of the valid combinations.
    bool nextIndices ()
    {
        for (int i = 0 ; i < numItems ; i++)
        {
            int index = m_items[i]->index() + 1;
            if (index < m_items[i]->numValues())
            {
                m_items[i]->setIndex(index);
                return true;
            }
            m_items[i]->setIndex(0);
        }
        copyIndices(false);
        return false;
    }
};

// Predicated cross-coverage
#define coverCrossPredicated(predicate, ...) \
{ \
    static const CoverageItemPtr items[] = { __VA_ARGS__ }; \
    const int numItems = sizeof(items) / sizeof(CoverageItemPtr); \
    static PredicatedCrossCoverageAssertionN<numItems> ca(__FILE__, __LINE__, __COUNTER__, (CoverageItem **) items, #predicate); \
    if (!ca.initialized()) \
    { \
        do \
        { \
            if (!(predicate)) \
                ca.cover(); \
        } while (ca.nextIndices()); \
    } \
    if (ca.data->numUncovered) \
        ca.cover(); \
}

/////////////////////////////////////////////////////////////////
//
// Convenience macros for covering return values
//
/////////////////////////////////////////////////////////////////
#define returnCovered(expr) \
{ \
    int _returnValue = expr; \
    coverAssert(_returnValue); \
    return _returnValue; \
}

#define returnCoveredBool(expr) \
{ \
    bool _returnValue = expr; \
    coverBool(_returnValue); \
    return _returnValue; \
}

#define returnCoveredRange(expr, low, high) \
{ \
    int _returnValue = expr; \
    coverRange(_returnValue, low, high); \
    return _returnValue; \
}

#define returnCoveredRangePredicated(expr, low, high, predicate) \
{ \
    int _returnValue = expr; \
    coverRangePredicated(_returnValue, low, high, predicate); \
    return _returnValue; \
}

#define returnCoveredValues(expr, ...) \
{ \
    int _returnValue = expr; \
    coverValues(_returnValue, __VA_ARGS__); \
    return _returnValue; \
}

/////////////////////////////////////////////////////////////////
//
// Only support coverage when _COVERAGE is defined
//
/////////////////////////////////////////////////////////////////

#ifndef _COVERAGE

#undef CoverageItemRange
#undef CoverageItemRangePredicated
#undef CoverageItemBool
#undef CoverageItemValues

#undef BEGIN_COVERAGE
#undef END_COVERAGE

#undef coverLine
#undef coverAssert
#undef coverCross
#undef coverRange
#undef coverRangePredicated
#undef coverBool
#undef coverValues

#undef returnCovered
#undef returnCoveredBool
#undef returnCoveredRange
#undef returnCoveredRangePredicated
#undef returnCoveredValues

#define CoverageItemRange(...)
#define CoverageItemRangePredicated(...)
#define CoverageItemBool(...)
#define CoverageItemValues(...)

#define BEGIN_COVERAGE(x)
#define END_COVERAGE()

#define coverLine()
#define coverAssert(...)
#define coverCross(...)
#define coverRange(...)
#define coverRangePredicated(...)
#define coverBool(...)
#define coverValues(...)

#define returnCovered(expr) return expr;
#define returnCoveredBool(expr) return expr;
#define returnCoveredRange(expr, ...) return expr;
#define returnCoveredRangePredicated(expr, ...) return expr; 
#define returnCoveredValues(expr, ...) return expr;

#endif // #ifndef _COVERAGE

#endif
