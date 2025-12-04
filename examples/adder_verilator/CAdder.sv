// Wrapper for the C++ Adder CModule (Verilator compatible)

module CAdder
(
    input  logic        clk,
    input  logic [15:0] i_a,
    input  logic [15:0] i_b,
    output logic [16:0] o_sum
);

    // DPI-C imports
    import "DPI-C" function chandle createCModule(input string name, input string verilogName);
    import "DPI-C" function void clockCModule(input chandle scope, input string clockName);
    import "DPI-C" function void pushToC32(input chandle scope, input bit [31:0] b, input string portName, input int sizeInBits);
    import "DPI-C" function void popFromC32(input chandle scope, output bit [31:0] b, input string portName, input int sizeInBits);

    // Handle to the C++ module
    chandle cmodule;

    // Temporaries for DPI calls
    bit [31:0] tmp_a, tmp_b, tmp_sum;

    // Initialize the CModule
    initial begin
        cmodule = createCModule("adder", "CAdder");
    end

    // On each rising clock edge:
    // 1. Push inputs to C++
    // 2. Clock the C++ module
    // 3. Pop outputs from C++
    always_ff @(posedge clk) begin
        // Push inputs
        tmp_a = {16'b0, i_a};
        tmp_b = {16'b0, i_b};
        pushToC32(cmodule, tmp_a, "in_a", 16);
        pushToC32(cmodule, tmp_b, "in_b", 16);

        // Clock the module
        clockCModule(cmodule, "clk");

        // Pop outputs
        popFromC32(cmodule, tmp_sum, "out_sum", 17);
        o_sum <= tmp_sum[16:0];
    end

endmodule
