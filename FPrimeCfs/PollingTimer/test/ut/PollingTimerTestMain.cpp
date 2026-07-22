// ======================================================================
// \title  PollingTimerTestMain.cpp
// \brief  cpp file for PollingTimer component test main function
// ======================================================================

#include "PollingTimerTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, StartAndCycle) {
    COMMENT("A started timer emits a cycle only after the interval elapses");
    REQUIREMENT("FPRIMECFS-POLLINGTIMER-001");
    REQUIREMENT("FPRIMECFS-POLLINGTIMER-002");
    Svc::PollingTimerTester tester;
    tester.testStartAndCycle();
}

TEST(Nominal, RepeatedCycles) {
    COMMENT("A started timer emits repeated cycles at the configured interval");
    REQUIREMENT("FPRIMECFS-POLLINGTIMER-002");
    Svc::PollingTimerTester tester;
    tester.testRepeatedCycles();
}

TEST(OffNominal, MissedCycles) {
    COMMENT("Missed intervals produce a single cycle, not a burst");
    REQUIREMENT("FPRIMECFS-POLLINGTIMER-003");
    Svc::PollingTimerTester tester;
    tester.testMissedCycles();
}

TEST(OffNominal, ZeroInterval) {
    COMMENT("A zero interval stops the timer");
    REQUIREMENT("FPRIMECFS-POLLINGTIMER-004");
    Svc::PollingTimerTester tester;
    tester.testZeroInterval();
}

TEST(Nominal, StopAndRestart) {
    COMMENT("A stopped timer emits no cycles; the timer can be restarted");
    REQUIREMENT("FPRIMECFS-POLLINGTIMER-005");
    Svc::PollingTimerTester tester;
    tester.testStopAndRestart();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
