// ======================================================================
// \title  CfsBridgeTestMain.cpp
// \brief  cpp file for CfsBridge component test main function
// ======================================================================

#include "CfsBridgeTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, Configure) {
    COMMENT("Configure creates the software bus pipe with the supplied settings");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-001");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testConfigure();
}

TEST(OffNominal, ConfigureFailure) {
    COMMENT("Configure returns the software bus error on pipe creation failure");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-001");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testConfigureFailure();
}

TEST(Nominal, Subscribe) {
    COMMENT("Subscribe maps APIDs to the expected command/telemetry message ids");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-002");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribe();
}

TEST(OffNominal, SubscribeFailure) {
    COMMENT("Subscribe returns the software bus error on subscription failure");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-002");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribeFailure();
}

TEST(Nominal, FrameCommand) {
    COMMENT("Command APID data is framed with a command header and transmitted");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-003");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testFrameCommand();
}

TEST(Nominal, FrameTelemetry) {
    COMMENT("Non-command APID data is framed with a telemetry header and transmitted");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-003");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testFrameTelemetry();
}

TEST(OffNominal, FrameInitFailure) {
    COMMENT("Buffer ownership is returned and com status emitted on message init failure");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-004");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testFrameInitFailure();
}

TEST(OffNominal, FrameTransmitFailure) {
    COMMENT("Buffer ownership is returned and com status emitted on transmit failure");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-004");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testFrameTransmitFailure();
}

TEST(Nominal, Deframe) {
    COMMENT("Received software bus messages are deframed and sent out dataOut with the APID context");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-005");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testDeframe();
}

TEST(Nominal, Preroll) {
    COMMENT("process() emits a single com status success (preroll) once subscribed");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-008");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testPreroll();
}

TEST(OffNominal, DeframeInvalidApid) {
    COMMENT("Messages with ids that do not map to a valid APID are dropped");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-006");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testDeframeInvalidApid();
}

TEST(Nominal, DeframeNoMessage) {
    COMMENT("Empty software bus polls produce no output");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-005");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testDeframeNoMessage();
}

TEST(OffNominal, DeframeReceiveError) {
    COMMENT("Software bus receive errors produce no output");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-006");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testDeframeReceiveError();
}

TEST(OffNominal, DeframeGetMsgIdFailure) {
    COMMENT("Messages whose id cannot be read are dropped");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-006");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testDeframeGetMsgIdFailure();
}

TEST(OffNominal, FrameOversize) {
    COMMENT("Data too large for a cFS message is dropped with buffer return and com status");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-004");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testFrameOversize();
}

TEST(Nominal, FlowControl) {
    COMMENT("Flow control gates deframed messages on comStatusIn signals");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-007");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testFlowControl();
}

TEST(Nominal, DataReturn) {
    COMMENT("dataReturnIn accepts returned buffers without action");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-009");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testDataReturn();
}

TEST(Random, Operations) {
    COMMENT("Randomized sequence of uplink/downlink/flow-control operations");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-003");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-005");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-007");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testRandomized();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
