/*
Copyright 2013, D. E. Shaw Research.
All rights reserved.

C++ implementation of a simple adder for Verilator co-simulation.
*/

#include <cascade/Cascade.hpp>

class Adder : public Component
{
    DECLARE_COMPONENT(Adder);
public:
    Adder(COMPONENT_CTOR) {}

    Clock(clk);
    Input(u16, in_a);
    Input(u16, in_b);
    Output(u17, out_sum);

    void update()
    {
        out_sum = in_a + in_b;
    }
};

// Register the component with the DPI factory
#ifdef _VERILOG_DPI

namespace Cascade {

class DPIModuleFactory {
public:
    typedef Component *(*ConstructorFunc)();
    static std::map<string, ConstructorFunc>& factories();
    static void registerFactory(const char *name, ConstructorFunc func) {
        factories()[name] = func;
    }
};

} // namespace Cascade

static Component *construct_adder() { return new Adder; }

// Static registration
static struct RegisterAdder {
    RegisterAdder() {
        Cascade::DPIModuleFactory::registerFactory("adder", construct_adder);
    }
} s_register_adder;

#endif // _VERILOG_DPI
