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
// FifoPorts.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 07/27/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include "FifoPorts.hpp"
#include "SimArchive.hpp"

BEGIN_NAMESPACE_CASCADE

FifoBitbucketTarget fifoBitbucketTarget;

// gcc has problems with offsetof in a static assertion
#ifdef _MSC_VER
#define FIFO_STATIC_ASSERT STATIC_ASSERT
#else
#define FIFO_STATIC_ASSERT assert_always
#endif

/////////////////////////////////////////////////////////////////
//
// initialize()
//
/////////////////////////////////////////////////////////////////
void GenericFifo::initialize (unsigned _dataSize, 
                              unsigned _size, 
                              unsigned _delay, 
                              bool _noflow,
                              intptr_t _target,
                              ClockDomain *producer,
                              ClockDomain *consumer)
{
    CascadeValidate(_size <= CASCADE_MAX_FIFO_SIZE, "Fifo size (%d) out of bounds", _size);
    CascadeValidate(_delay <= CASCADE_MAX_FIFO_DELAY, "Fifo delay (%d) out of bounds", _delay);
    CascadeValidate(!(_target & TRIGGER_ITRIGGER) || _noflow, "Fifo has both a trigger and flow control");
    CascadeValidate((_size % _dataSize) == 0, "Fifo storage size is not a multiple of the data size");
    CascadeValidate(!_delay || _size || !_target, "Fifo has delay but zero size and a trigger");

    FIFO_STATIC_ASSERT(descore_offsetof(Fifo<byte>,   data) == sizeof(GenericFifo));
    FIFO_STATIC_ASSERT(descore_offsetof(Fifo<uint64>, data) == sizeof(GenericFifo));
    FIFO_STATIC_ASSERT(descore_offsetof(Fifo<u128>,   data) == sizeof(GenericFifo));

    Sim::updateChecksum("FifoSize", _size);
    head = 0;
    tail = 0;
    freeCount = _size ? _size / _dataSize : CASCADE_MAX_FIFO_SIZE;
    fullCount = 0;
    size = _size;
    dataSize = _dataSize;
    minFree = freeCount;
    delay = _delay;
    noflow = _noflow ? 1 : 0;
    target = _target;
    producerClockDomain = producer;
    consumerClockDomain = consumer;
}

/////////////////////////////////////////////////////////////////
//
// reset()
//
/////////////////////////////////////////////////////////////////
void GenericFifo::reset ()
{
    head = 0;
    tail = 0;
    freeCount = size ? size / dataSize : CASCADE_MAX_FIFO_SIZE;
    fullCount = 0;
    minFree = freeCount;
}

/////////////////////////////////////////////////////////////////
//
// archive()
//
/////////////////////////////////////////////////////////////////
void GenericFifo::archive (Archive &ar)
{
    byte *data = GENERIC_FIFO_DATA(this);

    ar | head | tail | freeCount | fullCount | minFree;
    ar.archiveData(data, size);
}

END_NAMESPACE_CASCADE
