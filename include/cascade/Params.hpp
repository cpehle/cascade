/*
Copyright 2009, D. E. Shaw Research.
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
// Params.hpp
//
// Copyright (C) 2009 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 10/04/2009
//
// Cascade parameters
//
//////////////////////////////////////////////////////////////////////

#ifndef Params_hpp_091004095249
#define Params_hpp_091004095249

#include <descore/Parameter.hpp>

BEGIN_NAMESPACE_CASCADE

struct CascadeParams
{
    // Timing
    UintParameter   (DefaultClockPeriod,    1000,       "Default clock period in picoseconds");
    UintParameter   (ClockRounding,         5,          
        "Rising clock edges within this many picoseconds of an even number of nanoseconds will be "
        "rounded to the even number of nanoseconds");

    // Tracing
    StringParameter (Traces,                "",         "Specify a set of traces using the same format as the -trace command-line directive");
    Uint64Parameter (TraceStartTime,        0,          "Tracing is disabled before SimTraceStartTime (ns)");
    Uint64Parameter (TraceStopTime,         0xffffffff, "Tracing is disabled after SimTraceStopTime (ns)");

    // Waves dumping
    StringParameter (DumpSignals,           "",         "Specify a set of signals to dump using the same format as the -dump command-line directive");
    StringParameter (WavesFilename,         "sim.vcd",  "Filename used for waves dumping");
    StringParameter (WavesTimescale,        "1 ps",     "Timescale string for VCD file");
    UintParameter   (WavesDT,               10,         "Minimum time increment (ps) between succcessive times in the VCD file")

    // Checkpointing
    UintParameter   (CheckpointInterval,    0,          "Simulated time (ns) between archive checkpoints, or 0 to disable checkpoints");
    StringParameter (CheckpointName,        "sim",      "Base name of checkpoint files (full name is <name>_<time>.ckp)");
    StringParameter (RestoreFromCheckpoint, "",         "Checkpoint file from which simulation should be restored after initialization");
    StringParameter (ValidateCheckpoint,    "",         "Secondary checkpoint file against which first checkpoint file should be validated");
    BoolParameter   (SafeCheckpoint,        false,      "Create archive checkpoints in safe mode (see descore documentation)");

    // Misc
    BoolParameter   (ExactPortNames,        false,      "When binding to a Verilog port, require that the port name (appropriately translated) matches exactly");
    BoolParameter   (Verbose,               false,      "Display additional information during initialization");
    IntParameter    (MaxResetIterations,    10,         
                     "If greater than one, calls to reset() will iterate until all output ports quiesce, "
                     "up to the specified maximum number of iterations.  An error is generated if the output "
                     "ports are still changing after the maximum number of iterations.");
    UintParameter   (Timeout,               0,          "If non-zero, abort the simulation with an error at the specified timeout (in ns)");
    UintParameter   (Finish,                0,          "If non-zero, end the simulation at the specified time (in ns)");
    BoolParameter   (FifoSizeWarnings,      true,       "Print a warning message if a fifo size is too small to sustain full throughput");
    IntParameter    (NumThreads,            1,          "Number of threads to use for simulation.  Set to -1 to use maximum number of threads.");
};

// Defined in SimGlobals.cpp
extern ParameterGroup(CascadeParams, params, "cascade");

END_NAMESPACE_CASCADE

// Log if the verbose flag is enabled
#define logInfo(...) (void) (params.Verbose && (log(__VA_ARGS__), 1))

#endif // #ifndef Params_hpp_091004095249

