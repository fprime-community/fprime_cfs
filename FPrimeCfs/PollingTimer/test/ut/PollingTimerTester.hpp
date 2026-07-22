// ======================================================================
// \title  PollingTimerTester.hpp
// \brief  hpp file for PollingTimer component test harness implementation class
// ======================================================================

#ifndef Svc_PollingTimerTester_HPP
#define Svc_PollingTimerTester_HPP

#include "PollingTimerGTestBase.hpp"
#include "FPrimeCfs/PollingTimer/PollingTimer.hpp"

namespace Svc {

class PollingTimerTester final : public PollingTimerGTestBase {
  public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Construct object PollingTimerTester
    PollingTimerTester();

    //! Destroy object PollingTimerTester
    ~PollingTimerTester();

    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! A started timer emits a cycle only after the interval elapses
    void testStartAndCycle();

    //! A started timer emits repeated cycles at the configured interval
    void testRepeatedCycles();

    //! Missed intervals produce a single cycle, not a burst
    void testMissedCycles();

    //! A zero interval stops the timer
    void testZeroInterval();

    //! A stopped timer emits no cycles; the timer can be restarted
    void testStopAndRestart();

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Sleep for the given number of milliseconds
    static void sleepMs(U32 milliseconds);

    void connectPorts();    //!< Connects all ports for the component under test (auto-generated)
    void initComponents();  //!< Initializes the component under test (auto-generated)

    //! The component under test
    PollingTimer component;
};

}  // namespace Svc

#endif
