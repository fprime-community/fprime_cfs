// ======================================================================
// \title  CfsRouterTestMain.cpp
// \brief  cpp file for CfsRouter component test main function
// ======================================================================

#include "CfsRouterTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, RouteFprimeCommand)
{
    COMMENT("F Prime commands are copied to the configured command port and the buffer returned");
    REQUIREMENT("FPRIMECFS-CFSROUTER-001");
    REQUIREMENT("FPRIMECFS-CFSROUTER-002");
    REQUIREMENT("FPRIMECFS-CFSROUTER-008");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteFprimeCommand();
}

TEST(Nominal, RouteFprimeCommandSecHdr)
{
    COMMENT("The cFS command secondary header is excluded from copied F Prime command data");
    REQUIREMENT("FPRIMECFS-CFSROUTER-007");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteFprimeCommandSecHdr();
}

TEST(OffNominal, BadChecksum)
{
    FPrimeCfs::CfsRouterTester tester;
    tester.testBadChecksum();
}

TEST(OffNominal, NoRouteFunctionCode)
{
    FPrimeCfs::CfsRouterTester tester;
    tester.testNoRouteFunctionCode();
}

TEST(OffNominal, RouteFprimeCommandTooLarge)
{
    COMMENT("Oversized F Prime command data emits a serialization error and returns the buffer");
    REQUIREMENT("FPRIMECFS-CFSROUTER-010");
    REQUIREMENT("FPRIMECFS-CFSROUTER-011");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteFprimeCommandTooLarge();
}

TEST(Nominal, RouteCfsCommand)
{
    COMMENT("cFS commands route with function code and payload; ownership returns via bufferReturnIn");
    REQUIREMENT("FPRIMECFS-CFSROUTER-003");
    REQUIREMENT("FPRIMECFS-CFSROUTER-009");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteCfsCommand();
}

TEST(Nominal, RouteCfsTelemetry)
{
    COMMENT("cFS telemetry routes with parsed time and payload; ownership returns via bufferReturnIn");
    REQUIREMENT("FPRIMECFS-CFSROUTER-004");
    REQUIREMENT("FPRIMECFS-CFSROUTER-009");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteCfsTelemetry();
}

TEST(Nominal, RouteFile)
{
    COMMENT("File packets route to the file output; ownership returns via fileBufferReturnIn");
    REQUIREMENT("FPRIMECFS-CFSROUTER-015");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteFile();
}

TEST(Nominal, RouteUnknown)
{
    COMMENT("Unconfigured APIDs route to the unknown output with their context");
    REQUIREMENT("FPRIMECFS-CFSROUTER-005");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRouteUnknown();
}

TEST(Nominal, DataReturnIn)
{
    COMMENT("Buffers returned via dataReturnIn are forwarded on dataReturnOut with the original context");
    REQUIREMENT("FPRIMECFS-CFSROUTER-016");
    FPrimeCfs::CfsRouterTester tester;
    tester.testDataReturnIn();
}

TEST(Nominal, DataReturnInUntracked)
{
    COMMENT("Untracked buffers returned via dataReturnIn are forwarded with the supplied context");
    REQUIREMENT("FPRIMECFS-CFSROUTER-016");
    FPrimeCfs::CfsRouterTester tester;
    tester.testDataReturnInUntracked();
}

TEST(OffNominal, MissingSecondaryHeader)
{
    COMMENT("cFS routes without a secondary header go to unknown with a warning event");
    REQUIREMENT("FPRIMECFS-CFSROUTER-006");
    FPrimeCfs::CfsRouterTester tester;
    tester.testMissingSecondaryHeader();
}

TEST(OffNominal, ShortSecondaryHeader)
{
    COMMENT("cFS routes whose data is too small for the secondary header go to unknown with a warning");
    REQUIREMENT("FPRIMECFS-CFSROUTER-006");
    FPrimeCfs::CfsRouterTester tester;
    tester.testShortSecondaryHeader();
}

TEST(OffNominal, DisconnectedOutputs)
{
    COMMENT("Disconnected route outputs return the buffer rather than assert or leak");
    REQUIREMENT("FPRIMECFS-CFSROUTER-012");
    FPrimeCfs::CfsRouterTester tester(false);
    tester.testDisconnectedOutputs();
}

TEST(Nominal, CommandResponseNoop)
{
    COMMENT("cmdResponseIn is accepted as a no-op");
    REQUIREMENT("FPRIMECFS-CFSROUTER-013");
    FPrimeCfs::CfsRouterTester tester;
    tester.testCommandResponseNoop();
}

TEST(Random, Routing)
{
    COMMENT("Randomized routing across all categories returns every buffer");
    REQUIREMENT("FPRIMECFS-CFSROUTER-010");
    FPrimeCfs::CfsRouterTester tester;
    tester.testRandomized();
}

TEST(Random, MalformedFuzz)
{
    COMMENT("Malformed packets (short/oversized headers, bad checksums, type-mismatched and arbitrary APIDs) are "
            "routed or rejected with at most one diagnostic, never forwarded unverified, and always returned once");
    REQUIREMENT("FPRIMECFS-CFSROUTER-001a");
    REQUIREMENT("FPRIMECFS-CFSROUTER-010");
    FPrimeCfs::CfsRouterTester tester;
    tester.testMalformedFuzz();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

TEST(Nominal, LiteralChecksumVector)
{
    COMMENT("A hand-computed CFE_MSG checksum vector is accepted and a one-bit payload corruption is rejected");
    REQUIREMENT("FPRIMECFS-CFSROUTER-003");
    FPrimeCfs::CfsRouterTester tester;
    tester.testLiteralChecksumVector();
}
