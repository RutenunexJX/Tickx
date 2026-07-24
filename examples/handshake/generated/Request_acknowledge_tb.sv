`timescale 1ps/1ps

module wave_workbench_tb;
  timeunit 1ps;
  timeprecision 1ps;

  logic ack; // ack
  logic clk; // clk
  logic [7:0] data; // data[7:0]
  logic req; // req
  logic reset_n; // reset_n
  logic [1:0] state; // state
  logic __ww_clock_clock_main_raw;
  logic __ww_clock_clock_main_gated;
  logic __ww_clock_clock_main_disabled;
  assign clk = __ww_clock_clock_main_disabled ? 1'bx : __ww_clock_clock_main_gated ? 1'b0 : __ww_clock_clock_main_raw;

  initial begin : clock_clock_main
    __ww_clock_clock_main_raw = 1'b0;
    #0;
    forever begin
      __ww_clock_clock_main_raw = 1'b1; #5000;
      __ww_clock_clock_main_raw = 1'b0; #5000;
    end
  end

  initial begin : clock_overrides_clock_main
    __ww_clock_clock_main_gated = 1'b0;
    __ww_clock_clock_main_disabled = 1'b0;
    #170000;
    __ww_clock_clock_main_gated = 1'b1;
    __ww_clock_clock_main_disabled = 1'b0;
    #20000;
    __ww_clock_clock_main_gated = 1'b0;
    __ww_clock_clock_main_disabled = 1'b0;
    #10000;
    __ww_clock_clock_main_gated = 1'b0;
    __ww_clock_clock_main_disabled = 1'b1;
    #10000;
    __ww_clock_clock_main_gated = 1'b0;
    __ww_clock_clock_main_disabled = 1'b0;
  end

  initial begin : execute_scenario_handshake
    $display("Wave Workbench scenario: Request / acknowledge");
    fork : scenario_and_timeout
      begin : scenario_steps
        fork
          begin : event_ack_low_a
            // Initial acknowledge
            if (ack !== 1'b0) $error("ack mismatch at %0t: expected 0, got %b", $time, ack);
          end
          begin : event_data_idle_a
            // Initial data
            data = 8'h00;
          end
          begin : event_req_low_a
            // Initial request
            req = 1'b0;
          end
          begin : event_reset_low
            // Assert reset
            reset_n = 1'b0;
          end
          begin : event_state_idle_a
            // Initial state
            if (state !== 2'd0) $error("state mismatch at %0t: expected IDLE, got %b", $time, state);
          end
          begin : event_reset_high
            // Release reset
            #40000;
            reset_n = 1'b1;
          end
          begin : event_data_payload
            // Drive payload
            #80000;
            data = 8'h35;
          end
          begin : event_req_high
            // Raise request
            #80000;
            req = 1'b1;
          end
          begin : event_state_wait
            // Wait state
            #80000;
            if (state !== 2'd1) $error("state mismatch at %0t: expected WAIT_ACK, got %b", $time, state);
          end
          begin : event_ack_high
            // Acknowledge request
            #110000;
            if (ack !== 1'b1) $error("ack mismatch at %0t: expected 1, got %b", $time, ack);
          end
          begin : event_req_low_b
            // Drop request
            #130000;
            req = 1'b0;
          end
          begin : event_ack_low_b
            // Complete acknowledge
            #150000;
            if (ack !== 1'b0) $error("ack mismatch at %0t: expected 0, got %b", $time, ack);
          end
          begin : event_data_idle_b
            // Clear payload
            #150000;
            data = 8'h00;
          end
          begin : event_state_done
            // Done state
            #150000;
            if (state !== 2'd2) $error("state mismatch at %0t: expected DONE, got %b", $time, state);
          end
          begin : scenario_duration
            #220000;
          end
        join
        $display("Wave Workbench scenario completed");
        $finish;
      end
      begin : timeout_guard
        #440000;
        $fatal(1, "Wave Workbench scenario timeout");
      end
    join_any
    disable scenario_and_timeout;
  end

  // Assertions generated from losslessly representable Wave Workbench relations.

  // ack must rise within 1..4 cycles after req
  property p_relation_req_ack;
    @(posedge clk) disable iff ((reset_n == 0) || $isunknown(clk))
      $rose(req) |-> ##[1:4] $rose(ack);
  endproperty
  assert property (p_relation_req_ack) else $error("Relation relation-req-ack failed");

endmodule
