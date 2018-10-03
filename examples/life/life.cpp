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
#include <descore/Parameter.hpp>

int main (int csz, char *rgsz[])
{
    descore::parseTraces(csz, rgsz);
    Parameter::parseCommandLine(csz, rgsz);
    LifeChip chip;
    Sim::init();
    chip.in_pattern_select = 0;
    while (1)
    {
        char buff[64];
        printf("> ");
        fgets(buff, 64, stdin);
        if (*buff == 'q')
            return 0;
        if (*buff == 'l')
        {
            SimArchive::loadSimulation("life.dat");
            printf("Simulation restored from life.dat\n");
        }
        else if (*buff == 's')
        {
            SimArchive::saveSimulation("life.dat");
            printf("Simulation saved to life.dat\n");
            continue;
        }
        else
        {
            if (*buff >= '0' && *buff <= '3')
            {
                chip.in_pattern_select = *buff - '0';
                Sim::reset();
            }
            Sim::run();
        }
        for (int y = 7 ; y >= 0 ; y--)
        {
            for (int x = 0 ; x < 8 ; x++)
                printf("%c", chip.out_state[x][y] ? 'o' : '.');
            printf("\n");
        }
    }
}
