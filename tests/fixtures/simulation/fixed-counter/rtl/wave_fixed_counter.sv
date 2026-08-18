`timescale 1ps/1ps

module wave_fixed_counter (
    input  logic       clk_i,
    input  logic       rst_i,
    output logic [3:0] count_o,
    output logic       pulse_o
);
    initial count_o = '0;

    always_ff @(posedge clk_i) begin
        if (rst_i)
            count_o <= '0;
        else
            count_o <= count_o + 4'd1;
    end

    assign pulse_o = count_o[0];
endmodule
