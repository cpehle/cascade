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
// AllocTracker.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 05/14/2010
//
// Automatically detect memory leaks by counting the number of times a type
// is constructed/destructed.  Prints an error message at exit time if a
// memory leak is detected.  To track allocations for a type T, inherit from
// AllocTracker<T>.  Example:
//
// class Node : public descore::AllocTracker<Node>
// {
//     ...
//
// Alloc tracker does not increase the memory footprint of the type being
// tracked.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef AllocTracker_hpp_100514213929
#define AllocTracker_hpp_100514213929

#include "type_traits.hpp"
#include "assert.hpp"
#include "Thread.hpp"

BEGIN_NAMESPACE_DESCORE

template <typename T>
struct AllocTracker
{
    // Helper structure to maintain the allocation count and check for
    // memory leaks at program termination.
    struct AllocCounter
    {
        AllocCounter () : count (0)
        {
        }
        ~AllocCounter ()
        {
            try
            {
                // Check for memory leaks unless we're in a fatal error
                // condition (which can cause all sorts of leaks).
                if (!g_error)
                    AllocTracker<T>::verifyAllDeleted();
            }
            catch (descore::runtime_error &e)
            {
                e.reportFatal();
            }
        }
        int count;
    };

    // Static per-type counter
    static AllocCounter _alloc_tracker_counter;

    // Keep track of constructions/destructions
    AllocTracker ()
    {
        atomicIncrement(_alloc_tracker_counter.count);
    }
    AllocTracker (const AllocTracker &)
    {
        atomicIncrement(_alloc_tracker_counter.count);
    }
    ~AllocTracker ()
    {
        assert(_alloc_tracker_counter.count > 0,
            "**** Unexpected deletion of type %s ****", 
            type_traits<T>::name());
        atomicDecrement(_alloc_tracker_counter.count);
    }

    // Normally AllocTracker will check for a memory leak at program termination.
    // Call this function at an intermediate point when there should be no
    // active instances of the given type to verify that all objects have been
    // deleted.
    static void verifyAllDeleted ()
    {
        assert(_alloc_tracker_counter.count != 1,
            "**** Memory leak detected ****\n"
            "1 instance of %s was not deleted.",
            type_traits<T>::name());

        assert(!_alloc_tracker_counter.count, 
            "**** Memory leak detected ****\n"
            "%d instances of %s were not deleted.",
            _alloc_tracker_counter.count, type_traits<T>::name());
    }
};

template <typename T>
typename AllocTracker<T>::AllocCounter AllocTracker<T>::_alloc_tracker_counter;

END_NAMESPACE_DESCORE

#endif // #ifndef AllocTracker_hpp_100514213929

