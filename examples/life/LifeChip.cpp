/*
Copyright 2013, D. E. Shaw Research.
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

#include "LifeChip.hpp"

////////////////////////////////////////////////////////////////////////
//
// LifeChip::LifeChip()
//
////////////////////////////////////////////////////////////////////////
LifeChip::LifeChip (IMPL_CTOR) : m_cells(8, 8)
{
    static int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};

    // Connect controller inputs
    m_controller.in_pattern_select << in_pattern_select;

    // Connect ROM inputs
    m_rom.in_pattern_select << m_controller.out_pattern_select;
    m_rom.in_row_select << m_controller.out_rom_row_select;

    // Connect Cell inputs and LifeChip outputs
    for (int x = 0 ; x < 8 ; x++)
    {
        for (int y = 0 ; y < 8 ; y++)
        {
            out_state[x][y] <= m_cells(x, y).out_state;
            m_cells(x, y).in_initialize << m_controller.out_cell_row_init[y];
            m_cells(x, y).in_initVal << m_rom.out_data[x];
            m_cells(x, y).in_run << m_controller.out_run;

            // neighbours
            for (int i = 0 ; i < 8 ; i++)
                m_cells(x, y).in_neighbour[i] <=
                    m_cells((x+dx[i])&7, (y+dy[i])&7).out_state;
        }
    }
}
