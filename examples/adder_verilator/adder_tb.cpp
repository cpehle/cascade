/*
Verilator C++ testbench for the CAdder module.
*/

#include <verilated.h>
#include "VCAdder.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    // Instantiate the Verilated model
    VCAdder *top = new VCAdder;

    // Initialize inputs
    top->clk = 0;
    top->i_a = 0;
    top->i_b = 0;

    int errors = 0;

    // Run simulation
    for (int cycle = 0; cycle < 40; cycle++)
    {
        // Toggle clock
        top->clk = !top->clk;
        top->eval();

        // On rising edge, check results and update inputs
        if (top->clk)
        {
            // The cascade adder is combinational - output is sum of current inputs
            uint32_t expected = (uint32_t)top->i_a + (uint32_t)top->i_b;

            // Check result (skip first few cycles for warmup)
            if (cycle > 4)
            {
                printf("a=%04x b=%04x sum=%05x expected=%05x %s\n",
                       top->i_a, top->i_b, top->o_sum, expected,
                       (top->o_sum == expected) ? "OK" : "MISMATCH");

                if (top->o_sum != expected)
                    errors++;
            }

            // Update inputs with random values for next cycle
            top->i_a = rand() & 0xFFFF;
            top->i_b = rand() & 0xFFFF;
        }
    }

    printf("\nTest completed with %d errors\n", errors);

    delete top;
    return errors ? 1 : 0;
}
