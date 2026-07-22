# Svc::PollingTimer

A polling-based rate group driver. `PollingTimer` implements the `Drv.Tick` interface: it emits
`CycleOut` calls at a configured interval, suitable for driving an `Svc::RateGroupDriver` or an
`Svc::PassiveRateGroup` in environments without a hardware or OS timer callback — such as a cFS
application run loop, where the hosting application repeatedly calls `cycle()` and the timer decides
when a full interval has elapsed.

`PollingTimer` is a **passive** component: all methods execute on the caller's thread.

## Usage Examples

```cpp
Svc::PollingTimer timer("timer");
timer.init(INSTANCE_ID);

// Start a 1 Hz timer
timer.startTimer(Fw::TimeInterval(1, 0));

// In the hosting application's run loop (called at a rate faster than the interval):
while (running) {
    timer.cycle();  // Emits CycleOut when the interval has elapsed
}

// When cycling should end
timer.stop();
```

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| output | `CycleOut` | `Svc.Cycle` | Cycle output emitted when the configured interval elapses (from `Drv.Tick`) |
| time get | `timeCaller` | `Fw.Time` | Port for requesting the current time |

## Requirements

| Name | Description | Validation |
|---|---|---|
| FPRIMECFS-POLLINGTIMER-001 | `PollingTimer` shall emit no cycles before `startTimer()` is called and shall not emit a cycle before the configured interval has elapsed | Unit test |
| FPRIMECFS-POLLINGTIMER-002 | `PollingTimer` shall emit exactly one `CycleOut` call, timestamped with the current time, per elapsed interval when `cycle()` is polled | Unit test |
| FPRIMECFS-POLLINGTIMER-003 | When polling is delayed across multiple intervals, `PollingTimer` shall emit a single cycle and reschedule the next cycle one full interval later (no burst catch-up) | Unit test |
| FPRIMECFS-POLLINGTIMER-004 | `PollingTimer` shall treat a zero (or sub-resolution) interval passed to `startTimer()` as a stop request | Unit test |
| FPRIMECFS-POLLINGTIMER-005 | `PollingTimer` shall emit no cycles after `stop()` is called and shall support being restarted with a subsequent `startTimer()` call | Unit test |

## Design

The timer tracks a next-cycle deadline using `std::chrono::steady_clock` (monotonic; immune to wall
clock adjustments). `startTimer()` converts the supplied `Fw::TimeInterval` to the clock's duration
and schedules the first cycle one interval in the future. Each `cycle()` call compares the current
time against the deadline; when reached, a `CycleOut` is emitted with an `Os::RawTime` timestamp and
the deadline advances by one interval. If polling was delayed past more than one interval, the
deadline is rescheduled relative to the current time so that missed intervals do not produce a burst
of cycles.

## Unit Testing

The unit tests exercise the timer against the real monotonic clock with short (20 ms) intervals,
covering start/cycle behavior, repeated cycles, missed-interval handling, the zero-interval stop
request, and stop/restart.

```bash
fprime-util generate --ut
fprime-util build --ut -j"$(nproc)"
fprime-util check --coverage
```

Coverage: 100% lines, 100% functions.

## Change Log

| Date | Description |
|---|---|
| 2026-07-22 | Initial SDD with requirements and unit tests |
