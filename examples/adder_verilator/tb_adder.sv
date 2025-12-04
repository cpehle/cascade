/*
Testbench for the Verilator-compatible CAdder module.
*/

module tb_adder;

    logic clk = 0;
    always #5 clk = ~clk;  // 100MHz clock

    logic [15:0] a;
    logic [15:0] b;
    logic [16:0] sum;
    logic [16:0] expected;

    // Instantiate the C++ adder wrapped in Verilog
    CAdder adder(
        .clk(clk),
        .i_a(a),
        .i_b(b),
        .o_sum(sum)
    );

    // Test sequence
    initial begin
        a = 0;
        b = 0;
        expected = 0;

        // Wait for a few cycles
        repeat(2) @(posedge clk);

        // Run some tests
        repeat(20) begin
            @(posedge clk);
            $display("a=%04x b=%04x sum=%05x expected=%05x %s",
                     a, b, sum, expected,
                     (sum == expected) ? "OK" : "MISMATCH");
            expected = a + b;
            a = $random;
            b = $random;
        end

        $display("Test completed!");
        $finish;
    end

endmodule
