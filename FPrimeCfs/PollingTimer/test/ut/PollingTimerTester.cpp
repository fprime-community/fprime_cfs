// ======================================================================
// \title  PollingTimerTester.cpp
// \brief  cpp file for PollingTimer component test harness implementation class
// ======================================================================

#include "PollingTimerTester.hpp"
#include <chrono>
#include <thread>

namespace Svc {

static const U32 INTERVAL_MS = 20;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PollingTimerTester ::PollingTimerTester()
    : PollingTimerGTestBase("PollingTimerTester", PollingTimerTester::MAX_HISTORY_SIZE), component("PollingTimer") {
    this->initComponents();
    this->connectPorts();
}

PollingTimerTester ::~PollingTimerTester() {}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void PollingTimerTester ::sleepMs(U32 milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PollingTimerTester ::testStartAndCycle() {
    // Before the timer is started, cycles produce no output
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(0);

    this->component.startTimer(Fw::TimeInterval(0, INTERVAL_MS * 1000));

    // Before the interval elapses, no cycle is emitted
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(0);

    // After the interval elapses, exactly one cycle is emitted
    this->sleepMs(INTERVAL_MS + INTERVAL_MS / 2);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(1);

    // Immediately polling again does not emit another cycle
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(1);
}

void PollingTimerTester ::testRepeatedCycles() {
    this->component.startTimer(Fw::TimeInterval(0, INTERVAL_MS * 1000));

    for (U32 i = 1; i <= 3; i++) {
        this->sleepMs(INTERVAL_MS + INTERVAL_MS / 2);
        this->component.cycle();
        ASSERT_from_CycleOut_SIZE(i);
    }
}

void PollingTimerTester ::testMissedCycles() {
    this->component.startTimer(Fw::TimeInterval(0, INTERVAL_MS * 1000));

    // Sleep across several intervals: only a single (non-bursting) cycle is emitted
    this->sleepMs(INTERVAL_MS * 4);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(1);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(1);

    // The next cycle is rescheduled a full interval after the late cycle
    this->sleepMs(INTERVAL_MS + INTERVAL_MS / 2);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(2);
}

void PollingTimerTester ::testZeroInterval() {
    this->component.startTimer(Fw::TimeInterval(0, 0));
    this->sleepMs(INTERVAL_MS);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(0);
}

void PollingTimerTester ::testStopAndRestart() {
    this->component.startTimer(Fw::TimeInterval(0, INTERVAL_MS * 1000));
    this->component.stop();

    // A stopped timer emits no cycles
    this->sleepMs(INTERVAL_MS + INTERVAL_MS / 2);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(0);

    // The timer can be restarted after a stop
    this->component.startTimer(Fw::TimeInterval(0, INTERVAL_MS * 1000));
    this->sleepMs(INTERVAL_MS + INTERVAL_MS / 2);
    this->component.cycle();
    ASSERT_from_CycleOut_SIZE(1);
}

}  // namespace Svc
