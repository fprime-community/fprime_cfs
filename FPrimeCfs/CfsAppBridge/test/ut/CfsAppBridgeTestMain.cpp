// ======================================================================
// \title  CfsAppBridgeTestMain.cpp
// \brief  cpp file for CfsAppBridge component test main function
// ======================================================================

#include "CfsAppBridgeTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, CfsData)
{
    COMMENT("Raw payloads on cfsDataIn are copied and sent with the source-built frame context");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testCfsData();
}

TEST(Nominal, CfsCommand)
{
    COMMENT("A cFS command is forwarded with the configured APID and function code");
    REQUIREMENT("FPRIMECFS-CFSAPPBRIDGE-001");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testCfsCommand();
}

TEST(Nominal, ComDescriptorMapping)
{
    COMMENT("A com buffer's packet descriptor selects the APID");
    REQUIREMENT("FPRIMECFS-CFSAPPBRIDGE-002");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testComDescriptorMapping();
}

TEST(OffNominal, UnknownDescriptor)
{
    COMMENT("An unknown descriptor maps to the unknown APID with a warning");
    REQUIREMENT("FPRIMECFS-CFSAPPBRIDGE-003");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testUnknownDescriptor();
}

TEST(OffNominal, AllocationFailure)
{
    COMMENT("An undersized allocation drops the data");
    REQUIREMENT("FPRIMECFS-CFSAPPBRIDGE-004");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testAllocationFailure();
}

TEST(OffNominal, CfsDataAllocationFailure)
{
    COMMENT("An undersized allocation drops raw cfsDataIn data and returns it with its context");
    REQUIREMENT("FPRIMECFS-CFSAPPBRIDGE-004");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testCfsDataAllocationFailure();
}

TEST(Nominal, DataReturn)
{
    COMMENT("dataReturnIn deallocates the outgoing buffer");
    REQUIREMENT("FPRIMECFS-CFSAPPBRIDGE-005");
    FPrimeCfs::CfsAppBridgeTester tester;
    tester.testDataReturn();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
