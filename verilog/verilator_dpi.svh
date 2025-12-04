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

/////////////////////////////////////////////////////////////////
//
// verilator_dpi.svh
//
// Verilator-compatible DPI interface for Cascade co-simulation.
// `include this file from SystemVerilog to use Cascade CModules
// with Verilator.
//
/////////////////////////////////////////////////////////////////

`ifndef CASCADE_VERILATOR_DPI_SVH
`define CASCADE_VERILATOR_DPI_SVH

//
// setCModuleParam()
//
// Call this function zero or more times before createCModule to
// set the values of the CModule parameters.
//
import "DPI-C" function void setCModuleParam(input string name, input int value);

//
// createCModule()
//
// Call this function within a wrapper module in order to instantiate
// a CModule and bind its ports to the wrapper's ports.
//
import "DPI-C" function chandle createCModule(input string name, input string verilogName);

//
// clockCModule()
//
// Call on the rising edge of a clock to advance the C++ simulation.
//
import "DPI-C" function void clockCModule(input chandle scope, input string clockName);
`define CLOCK_CMODULE(clkName) clockCModule(cmodule, `"clkName`")

//
// setCModuleTraces()
//
// Enables tracing within the CModules.
//
import "DPI-C" function void setCModuleTraces(input string traces);

//
// dumpCModuleVars()
//
// Create waves from the CModule signals.
//
import "DPI-C" function void dumpCModuleVars(input string dumps);

//
// disableCAssertion()
//
// Disable an assertion within the CModules.
//
import "DPI-C" function void disableCAssertion(input string message);

//
// setCParameter()
//
// Set one of the C++ parameters to the specified value.
//
import "DPI-C" function void setCParameter(input string name, input string value);

//
// ignoreCPort()
//
// Skip a port during push/pop sequence (for multi-clock designs)
//
import "DPI-C" function void ignoreCPort(input chandle scope, input string name, input int isInput);
`define IGNORE_TO_C(port)   ignoreCPort(cmodule, `"port`", 1)
`define IGNORE_FROM_C(port) ignoreCPort(cmodule, `"port`", 0)

//
// Push/Pop functions for various bit widths
//
import "DPI-C" function void pushToC32(input chandle scope, input bit [31:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC64(input chandle scope, input bit [63:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC96(input chandle scope, input bit [95:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC128(input chandle scope, input bit [127:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC160(input chandle scope, input bit [159:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC192(input chandle scope, input bit [191:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC224(input chandle scope, input bit [223:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC256(input chandle scope, input bit [255:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC288(input chandle scope, input bit [287:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC320(input chandle scope, input bit [319:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC352(input chandle scope, input bit [351:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC384(input chandle scope, input bit [383:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC416(input chandle scope, input bit [415:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC448(input chandle scope, input bit [447:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC480(input chandle scope, input bit [479:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void pushToC512(input chandle scope, input bit [511:0] b, input string portName, input int sizeInBits);

import "DPI-C" function void popFromC32(input chandle scope, output bit [31:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC64(input chandle scope, output bit [63:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC96(input chandle scope, output bit [95:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC128(input chandle scope, output bit [127:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC160(input chandle scope, output bit [159:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC192(input chandle scope, output bit [191:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC224(input chandle scope, output bit [223:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC256(input chandle scope, output bit [255:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC288(input chandle scope, output bit [287:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC320(input chandle scope, output bit [319:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC352(input chandle scope, output bit [351:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC384(input chandle scope, output bit [383:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC416(input chandle scope, output bit [415:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC448(input chandle scope, output bit [447:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC480(input chandle scope, output bit [479:0] b, input string portName, input int sizeInBits);
import "DPI-C" function void popFromC512(input chandle scope, output bit [511:0] b, input string portName, input int sizeInBits);

//
// PUSH_TO_C macro - push input values to C++
//
`define PUSH_TO_C(port) \
    if ($bits(port) <= 32) pushToC32(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 64) pushToC64(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 96) pushToC96(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 128) pushToC128(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 160) pushToC160(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 192) pushToC192(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 224) pushToC224(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 256) pushToC256(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 288) pushToC288(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 320) pushToC320(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 352) pushToC352(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 384) pushToC384(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 416) pushToC416(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 448) pushToC448(cmodule, port, `"port`", $bits(port)); \
    else if ($bits(port) <= 480) pushToC480(cmodule, port, `"port`", $bits(port)); \
    else pushToC512(cmodule, port, `"port`", $bits(port))

//
// POP_FROM_C macro - pop output values from C++
//
`define POP_FROM_C(port) \
    begin \
        logic [$bits(port)-1:0] tmp_``port; \
        if ($bits(port) <= 32) popFromC32(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 64) popFromC64(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 96) popFromC96(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 128) popFromC128(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 160) popFromC160(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 192) popFromC192(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 224) popFromC224(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 256) popFromC256(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 288) popFromC288(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 320) popFromC320(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 352) popFromC352(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 384) popFromC384(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 416) popFromC416(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 448) popFromC448(cmodule, tmp_``port, `"port`", $bits(port)); \
        else if ($bits(port) <= 480) popFromC480(cmodule, tmp_``port, `"port`", $bits(port)); \
        else popFromC512(cmodule, tmp_``port, `"port`", $bits(port)); \
        port = tmp_``port; \
    end

`endif // CASCADE_VERILATOR_DPI_SVH
