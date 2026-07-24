import cocotb
from cocotb.triggers import ClockCycles, Combine, Timer, with_timeout


_ww_clock_state_clock_main = {"raw": 0, "mode": "run"}


def _ww_apply_clock_clock_main(dut):
    mode = _ww_clock_state_clock_main["mode"]
    if mode == "disabled":
        dut.clk.value = "X"
    elif mode == "gated":
        dut.clk.value = 0
    else:
        dut.clk.value = _ww_clock_state_clock_main["raw"]


async def clock_clock_main(dut):
    _ww_clock_state_clock_main["raw"] = 0
    _ww_apply_clock_clock_main(dut)
    while True:
        _ww_clock_state_clock_main["raw"] = 1
        _ww_apply_clock_clock_main(dut)
        await Timer(5000, unit="ps")
        _ww_clock_state_clock_main["raw"] = 0
        _ww_apply_clock_clock_main(dut)
        await Timer(5000, unit="ps")


async def clock_overrides_clock_main(dut):
    await Timer(170000, unit="ps")
    _ww_clock_state_clock_main["mode"] = "gated"
    _ww_apply_clock_clock_main(dut)
    await Timer(20000, unit="ps")
    _ww_clock_state_clock_main["mode"] = "run"
    _ww_apply_clock_clock_main(dut)
    await Timer(10000, unit="ps")
    _ww_clock_state_clock_main["mode"] = "disabled"
    _ww_apply_clock_clock_main(dut)
    await Timer(10000, unit="ps")
    _ww_clock_state_clock_main["mode"] = "run"
    _ww_apply_clock_clock_main(dut)


async def event_ack_low_a(dut):
    # Initial acknowledge
    assert int(dut.ack.value) == 0, "expected 0"

async def event_data_idle_a(dut):
    # Initial data
    dut.data.value = 0x00

async def event_req_low_a(dut):
    # Initial request
    dut.req.value = 0

async def event_reset_low(dut):
    # Assert reset
    dut.reset_n.value = 0

async def event_state_idle_a(dut):
    # Initial state
    assert int(dut.state.value) == 0, "expected IDLE"

async def event_reset_high(dut):
    # Release reset
    await Timer(40000, unit="ps")
    dut.reset_n.value = 1

async def event_data_payload(dut):
    # Drive payload
    await Timer(80000, unit="ps")
    dut.data.value = 0x35

async def event_req_high(dut):
    # Raise request
    await Timer(80000, unit="ps")
    dut.req.value = 1

async def event_state_wait(dut):
    # Wait state
    await Timer(80000, unit="ps")
    assert int(dut.state.value) == 1, "expected WAIT_ACK"

async def event_ack_high(dut):
    # Acknowledge request
    await Timer(110000, unit="ps")
    assert int(dut.ack.value) == 1, "expected 1"

async def event_req_low_b(dut):
    # Drop request
    await Timer(130000, unit="ps")
    dut.req.value = 0

async def event_ack_low_b(dut):
    # Complete acknowledge
    await Timer(150000, unit="ps")
    assert int(dut.ack.value) == 0, "expected 0"

async def event_data_idle_b(dut):
    # Clear payload
    await Timer(150000, unit="ps")
    dut.data.value = 0x00

async def event_state_done(dut):
    # Done state
    await Timer(150000, unit="ps")
    assert int(dut.state.value) == 2, "expected DONE"

async def scenario_duration():
    await Timer(220000, unit="ps")


@cocotb.test()
async def test_scenario_handshake(dut):
    """Request / acknowledge"""
    dut._log.info("Wave Workbench scenario: Request / acknowledge")
    cocotb.start_soon(clock_clock_main(dut))
    cocotb.start_soon(clock_overrides_clock_main(dut))
    tasks = [
        cocotb.start_soon(event_ack_low_a(dut)),
        cocotb.start_soon(event_data_idle_a(dut)),
        cocotb.start_soon(event_req_low_a(dut)),
        cocotb.start_soon(event_reset_low(dut)),
        cocotb.start_soon(event_state_idle_a(dut)),
        cocotb.start_soon(event_reset_high(dut)),
        cocotb.start_soon(event_data_payload(dut)),
        cocotb.start_soon(event_req_high(dut)),
        cocotb.start_soon(event_state_wait(dut)),
        cocotb.start_soon(event_ack_high(dut)),
        cocotb.start_soon(event_req_low_b(dut)),
        cocotb.start_soon(event_ack_low_b(dut)),
        cocotb.start_soon(event_data_idle_b(dut)),
        cocotb.start_soon(event_state_done(dut)),
        cocotb.start_soon(scenario_duration()),
    ]
    await with_timeout(Combine(*tasks), 440000, "ps")
    dut._log.info("Wave Workbench scenario completed")
