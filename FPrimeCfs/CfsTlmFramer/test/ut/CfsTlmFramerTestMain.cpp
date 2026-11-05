// ======================================================================
// \title  CfsTlmFramerTestMain.cpp
// \brief  cpp file for CfsTlmFramer component test main function
// ======================================================================

#include "CfsTlmFramerTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, RawPayloadCurrentTime)
{
    COMMENT("Raw cFS payloads on non-F Prime APIDs are time-tagged with the current time without a warning");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testRawPayloadCurrentTime();
}

TEST(Nominal, LogPacketFraming)
{
    COMMENT("An event packet's time tag is extracted into the cFS telemetry secondary header");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-001");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testLogPacketFraming();
}

TEST(Nominal, TlmPacketFraming)
{
    COMMENT("A telemetry channel packet's first time tag is extracted into the secondary header");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-001");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testTlmPacketFraming();
}

TEST(Nominal, PacketizedTlmFraming)
{
    COMMENT("A packetized telemetry packet's time tag is extracted into the secondary header");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-001");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testPacketizedTlmFraming();
}

TEST(OffNominal, UnknownPacketFallback)
{
    COMMENT("An unknown packet type falls back to the current time with a warning");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-002");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testUnknownPacketFallback();
}

TEST(OffNominal, AllocationFailure)
{
    COMMENT("An undersized allocation drops the packet and returns the data");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-003");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testAllocationFailure();
}

TEST(Nominal, ComStatusPassthrough)
{
    COMMENT("comStatusIn is passed through to comStatusOut");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-004");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testComStatusPassthrough();
}

TEST(Nominal, DataReturnPassthrough)
{
    COMMENT("dataReturnIn deallocates the wrapped buffer");
    REQUIREMENT("FPRIMECFS-CFSTLMFRAMER-005");
    FPrimeCfs::CfsTlmFramerTester tester;
    tester.testDataReturnPassthrough();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
