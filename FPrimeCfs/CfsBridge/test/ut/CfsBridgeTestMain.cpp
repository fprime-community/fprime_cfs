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

TEST(OffNominal, ConfigureDepthTooLarge) {
    COMMENT("Configure rejects pipe depths that would truncate in cFE's uint16 depth");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-011");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testConfigureDepthTooLarge();
}

TEST(Nominal, SubscribeCfs) {
    COMMENT("SubscribeCfs maps APIDs to cFS command/telemetry message ids with the secondary header flag set");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-010");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribeCfs();
}

TEST(OffNominal, SubscribeFailure) {
    COMMENT("Subscribe returns the software bus error on subscription failure");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-002");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribeFailure();
}

TEST(Nominal, SubscribeMessageLimit) {
    COMMENT("Subscriptions allow as many queued messages per message ID as the configured pipe depth");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-015");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribeMessageLimit();
}

TEST(Nominal, SubscribeExplicitMessageLimit) {
    COMMENT("Subscriptions with an explicit message limit pass exactly that limit");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-015");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribeExplicitMessageLimit(8);
}

TEST(Nominal, SubscribeZeroMessageLimit) {
    COMMENT("An explicit message limit of 0 is passed through to cFE unchanged");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-015");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSubscribeExplicitMessageLimit(0);
}

TEST(Nominal, TransmitSinglePacket) {
    COMMENT("A single complete space packet is transmitted as one software bus message");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-003");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitSinglePacket();
}

TEST(Nominal, TransmitMultiplePackets) {
    COMMENT("Multiple concatenated space packets are each transmitted as their own message");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-003");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitMultiplePackets();
}

TEST(OffNominal, TransmitFailure) {
    COMMENT("Buffer ownership is returned and com status emitted on transmit failure");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-004");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitFailure();
}

TEST(OffNominal, TransmitTruncated) {
    COMMENT("A packet whose length field exceeds the available data is dropped");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-004");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitTruncated();
}

TEST(OffNominal, TransmitResidual) {
    COMMENT("Residual bytes too small to form a primary header are dropped");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-004");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitResidual();
}

TEST(Nominal, TransmitWrappedCommand) {
    COMMENT("With wrapping enabled, F Prime command packets are transmitted as valid cFS command packets");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-012");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitWrappedCommand();
}

TEST(Nominal, TransmitWrapPassthrough) {
    COMMENT("With wrapping enabled, telemetry and secondary-header command packets pass through unmodified");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-012");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitWrapPassthrough();
}

TEST(OffNominal, TransmitWrapTooLarge) {
    COMMENT("With wrapping enabled, command packets too large to wrap are dropped");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-012");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitWrapTooLarge();
}

TEST(Nominal, TransmitWrapExactBoundary) {
    COMMENT("With wrapping enabled, a command packet exactly filling the wrap storage is transmitted");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-012");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitWrapExactBoundary();
}

TEST(Nominal, TransmitWrapMultiplePackets) {
    COMMENT("With wrapping enabled, wrapping one packet of a multi-packet buffer does not disturb the next");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-012");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitWrapMultiplePackets();
}

TEST(OffNominal, TransmitWrapFailure) {
    COMMENT("A failing software bus transmit on the wrapped command path still returns the buffer");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-012");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testTransmitWrapFailure();
}

TEST(Nominal, Receive) {
    COMMENT("Received software bus messages are sent whole out dataOut with a default context");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-005");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testReceive();
}

TEST(Nominal, SchedIn) {
    COMMENT("A schedIn tick drains the message queue and polls the software bus once");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-005");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testSchedIn();
}

TEST(Nominal, Preroll) {
    COMMENT("process() emits a single com status success (preroll) once subscribed");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-008");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testPreroll();
}

TEST(Nominal, ReceiveNoMessage) {
    COMMENT("Empty software bus polls produce no output");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-005");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testReceiveNoMessage();
}

TEST(OffNominal, ReceiveAllocationFailure) {
    COMMENT("Received messages are dropped when buffer allocation fails");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-014");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testReceiveAllocationFailure();
}

TEST(OffNominal, ReceiveError) {
    COMMENT("Software bus receive errors produce no output");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-006");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testReceiveError();
}

TEST(OffNominal, ReceiveGetSizeFailure) {
    COMMENT("Messages whose size cannot be read are dropped");
    REQUIREMENT("FPRIMECFS-CFSBRIDGE-006");
    FPrimeCfs::CfsBridgeTester tester;
    tester.testReceiveGetSizeFailure();
}

TEST(Nominal, FlowControl) {
    COMMENT("Flow control gates received messages on comStatusIn signals");
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
