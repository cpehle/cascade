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

//////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author: J.P. Grossman (jp.grossman@deshawresearch.com)
// 
// Description:
//
// Verilog module that parses Cascade-specific plusargs.
//
//
// Plusargs:
// --------
//
// Tracing (see Cascade Programmer's Manual for specifier format).
// Calls $set_traces
//
//   +trace+<specifiers>
//
//
// Waves (see parseDumps() in SimGlobals.hpp for specifier format)
// Calls $dump_cvars
//
//   +dump_cvars+<specifiers>
//
//
// Disable assertions (plus-delimited list of messages)
// Calls $disable_assertion
//
//   +disable_assertions+message1+message2
//
//
// Set parameters (plus-delimited list of parameter=value)
// Calls $set_parameter
//
//   +setparams+param1=value1+param2=value2
//
//
// Special Characters:
// ------------------
//
// To include special characters (space, comma, semicolon, etc),
// either escape them with a backslash, or enclose them in  
// quotes, e.g.
//
//   +setparams+Subboxing="(1,2,4)"
//
//////////////////////////////////////////////////////////////////////

module cascade_plusargs;

function int strlen (input string s);
    begin
        int i;
        i = 0;
        while (s[i]) i++;
        strlen = i;
    end
endfunction

task split (input string s, output string s1, output string s2, input string delim, output logic matched_delim);
    begin
        int i;
        s1 = s;
        s2 = "";
        i = 0;
        matched_delim = 0;
        for ( ; s[i] && s[i] != delim[0] ; i++) begin
            if (s[i] == 92)
              i++;
        end
        if (s[i] == delim[0]) begin
            s1 = s.substr(0, i-1);
            s2 = s.substr(i+1, strlen(s)-1);
            matched_delim = 1;
        end
    end
endtask

function string unescape (input string s);
    begin
        for (int i = 0 ; i < strlen(s) ; i++)
          if (s[i] == 92)
            unescape = unescape({s.substr(0,i-1), s.substr(i+1, strlen(s)-1)});
        unescape = s;
    end
endfunction

initial begin
    string arg;
    logic matched_delim;

    // Tracing
    if ($value$plusargs("trace+%s", arg)) begin
        $display("Tracing %s", arg);
        $set_traces(arg);
    end

    // Waves
    if ($value$plusargs("dump_cvars+%s", arg)) begin
        $display("Dumping %s", arg);
        $dump_cvars(arg);
    end

    // Disable assertions
    if ($value$plusargs("disable_assertions+%s", arg)) begin
        string msg;
        while (arg != "") begin
            split(arg, msg, arg, "+", matched_delim);
            msg = unescape(msg);
            $display("Disabling assertion %s", msg);
            $disable_assertion(msg);
        end
    end

    // Set parameters
    if ($value$plusargs("setparams+%s", arg)) begin
        string param_val, param, val;
        while (arg != "") begin
            split(arg, param_val, arg, "+", matched_delim);
            split(param_val, param, val, "=", matched_delim);
            if (!matched_delim) begin
                val = "true";
            end
            val = unescape(val);
            $display("Setting parameter %s = %s", param, val);
            $set_parameter(param, val);
        end
    end
end

endmodule

