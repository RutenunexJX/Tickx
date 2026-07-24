`timescale 1ps/1ps

module wave_workbench_assertions_scenario_handshake(
  input logic ack,
  input logic clk,
  input logic [7:0] data,
  input logic req,
  input logic reset_n,
  input logic [1:0] state
);
  timeunit 1ps;
  timeprecision 1ps;

  // Assertions generated from losslessly representable Wave Workbench relations.

  // ack must rise within 1..4 cycles after req
  property p_relation_req_ack;
    @(posedge clk) disable iff ((reset_n == 0) || $isunknown(clk))
      $rose(req) |-> ##[1:4] $rose(ack);
  endproperty
  assert property (p_relation_req_ack) else $error("Relation relation-req-ack failed");

endmodule
