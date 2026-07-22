// ======================================================================
// \title  CfsBridgeTester.cpp
// \brief  cpp file for CfsBridge component test harness implementation class
// ======================================================================

#include "CfsBridgeTester.hpp"
#include <STest/Pick/Pick.hpp>
#include <cstring>
#include <limits>

namespace FPrimeCfs {

static const CFE_SB_MsgId_Atom_t CMD_MID_FOR_APID_0 = 0x1800;
static const CFE_SB_MsgId_Atom_t TLM_MID_FOR_APID_1 = 0x0801;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CfsBridgeTester ::CfsBridgeTester()
    : CfsBridgeGTestBase("CfsBridgeTester", CfsBridgeTester::MAX_HISTORY_SIZE), component("CfsBridge") {
    CfeStub::reset();
    this->initComponents();
    this->connectPorts();
}

CfsBridgeTester ::~CfsBridgeTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void CfsBridgeTester ::configureAndSubscribe(ComCfg::Apid::T apid, bool paused) {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", paused), CFE_SUCCESS);
    ASSERT_EQ(this->component.subscribe(apid), CFE_SUCCESS);
}

void CfsBridgeTester ::fillRandom(Fw::Buffer& buffer, FwSizeType maxSize) {
    FwSizeType size = static_cast<FwSizeType>(STest::Pick::lowerUpper(1, static_cast<U32>(maxSize)));
    for (FwSizeType i = 0; i < size; i++) {
        buffer.getData()[i] = static_cast<U8>(STest::Pick::any());
    }
    buffer.setSize(size);
}

void CfsBridgeTester ::sendDataIn(Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
    // dataIn is async: run the component's queue via its public process() method
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    // The buffer ownership is always returned and com status always reports success
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_dataReturnOut(0, buffer, context);
    ASSERT_from_comStatusOut_SIZE(1);
    Fw::Success expected = Fw::Success::SUCCESS;
    ASSERT_from_comStatusOut(0, expected);
}

void CfsBridgeTester ::receiveMessage(ComCfg::Apid::T apid, const U8* payload, FwSizeType size) {
    CFE_SB_MsgId_Atom_t msgIdValue =
        (apid == ComCfg::Apid::FW_PACKET_COMMAND) ? (0x1800 | apid) : (0x0800 | apid);
    CfeStub::queueMessage(msgIdValue, payload, size);
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
}

void CfsBridgeTester ::assertTransmitted(U32 index,
                                         CFE_SB_MsgId_Atom_t expectedMsgId,
                                         FwSizeType expectedHeaderSize,
                                         const Fw::Buffer& expectedPayload) {
    ASSERT_GT(CfeStub::state().transmitCount, index);
    const CfeStub::TransmitCall& call = CfeStub::state().transmitCalls[index];
    ASSERT_EQ(call.msgIdValue, expectedMsgId);
    ASSERT_EQ(call.totalSize, expectedHeaderSize + expectedPayload.getSize());
    ASSERT_EQ(call.payloadSize, expectedPayload.getSize());
    ASSERT_EQ(std::memcmp(call.payload, expectedPayload.getData(), expectedPayload.getSize()), 0);
}

// ----------------------------------------------------------------------
// Tests: configure/subscribe
// ----------------------------------------------------------------------

void CfsBridgeTester ::testConfigure() {
    ASSERT_EQ(this->component.configure(17, "MY_TEST_PIPE"), CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().createPipeCount, 1u);
    ASSERT_EQ(CfeStub::state().createPipeCalls[0].depth, 17);
    ASSERT_STREQ(CfeStub::state().createPipeCalls[0].name, "MY_TEST_PIPE");
}

void CfsBridgeTester ::testConfigureFailure() {
    CfeStub::state().createPipeStatus = CFE_SB_PIPE_CR_ERR;
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE"), CFE_SB_PIPE_CR_ERR);
    // Polling before successful configuration produces no receive attempts
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_EQ(CfeStub::state().receiveCount, 0u);
}

void CfsBridgeTester ::testSubscribe() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE"), CFE_SUCCESS);
    // Command APIDs map into the platform command message id space
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_COMMAND), CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 1u);
    ASSERT_EQ(CfeStub::state().subscribeCalls[0].msgIdValue, CMD_MID_FOR_APID_0);
    // All other APIDs map into the platform telemetry message id space
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_TELEM), CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 2u);
    ASSERT_EQ(CfeStub::state().subscribeCalls[1].msgIdValue, TLM_MID_FOR_APID_1);
}

void CfsBridgeTester ::testSubscribeFailure() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE"), CFE_SUCCESS);
    CfeStub::state().subscribeStatus = CFE_SB_MAX_MSGS_MET;
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_COMMAND), CFE_SB_MAX_MSGS_MET);
    // A failed subscription does not enable polling
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_EQ(CfeStub::state().receiveCount, 0u);
}

// ----------------------------------------------------------------------
// Tests: framing (dataIn -> software bus)
// ----------------------------------------------------------------------

void CfsBridgeTester ::testFrameCommand() {
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    this->fillRandom(buffer, sizeof(storage));
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_COMMAND);

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    this->assertTransmitted(0, CMD_MID_FOR_APID_0, sizeof(CFE_MSG_CommandHeader_t), buffer);
}

void CfsBridgeTester ::testFrameTelemetry() {
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    this->fillRandom(buffer, sizeof(storage));
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_TELEM);

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    this->assertTransmitted(0, TLM_MID_FOR_APID_1, sizeof(CFE_MSG_TelemetryHeader_t), buffer);
}

void CfsBridgeTester ::testFrameInitFailure() {
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    this->fillRandom(buffer, sizeof(storage));
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_TELEM);

    CfeStub::state().msgInitStatus = CFE_SB_BAD_ARGUMENT;
    // Buffer return and com status are still emitted on failure
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 0u);
}

void CfsBridgeTester ::testFrameTransmitFailure() {
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    this->fillRandom(buffer, sizeof(storage));
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_TELEM);

    CfeStub::state().transmitStatus = CFE_SB_BAD_ARGUMENT;
    // Buffer return and com status are still emitted on failure
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
}

// ----------------------------------------------------------------------
// Tests: deframing (software bus -> dataOut)
// ----------------------------------------------------------------------

void CfsBridgeTester ::testDeframe() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);

    U8 payload[32];
    for (FwSizeType i = 0; i < sizeof(payload); i++) {
        payload[i] = static_cast<U8>(STest::Pick::any());
    }
    this->clearHistory();
    this->receiveMessage(ComCfg::Apid::FW_PACKET_COMMAND, payload, sizeof(payload));

    ASSERT_from_dataOut_SIZE(1);
    const Fw::Buffer& outBuffer = this->fromPortHistory_dataOut->at(0).data;
    const ComCfg::FrameContext& outContext = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(outBuffer.getSize(), sizeof(payload));
    ASSERT_EQ(std::memcmp(outBuffer.getData(), payload, sizeof(payload)), 0);
    ASSERT_EQ(outContext.get_apid(), ComCfg::Apid::FW_PACKET_COMMAND);
}

void CfsBridgeTester ::testPreroll() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    this->clearHistory();
    // The first process() call after subscription emits a single com status success (preroll)
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_comStatusOut_SIZE(1);
    Fw::Success expected = Fw::Success::SUCCESS;
    ASSERT_from_comStatusOut(0, expected);
    // Subsequent process() calls do not re-emit the preroll
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_comStatusOut_SIZE(1);
}

void CfsBridgeTester ::testDeframeInvalidApid() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    this->clearHistory();

    U8 payload[8] = {0};
    // Queue a message with a telemetry message id whose APID bits are not a valid ComCfg::Apid
    CfeStub::queueMessage(0x0800 | 0x123, payload, sizeof(payload));
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);

    // Queue a message with a command message id whose APID maps back to a telemetry id
    CfeStub::queueMessage(0x1800 | ComCfg::Apid::FW_PACKET_TELEM, payload, sizeof(payload));
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
}

void CfsBridgeTester ::testDeframeNoMessage() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_GT(CfeStub::state().receiveCount, 0u);
}

void CfsBridgeTester ::testDeframeReceiveError() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    U8 payload[8] = {0};
    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payload, sizeof(payload));
    CfeStub::state().receiveStatus = CFE_SB_PIPE_RD_ERR;
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
}

void CfsBridgeTester ::testDeframeGetMsgIdFailure() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    U8 payload[8] = {0};
    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payload, sizeof(payload));
    CfeStub::state().getMsgIdStatus = CFE_SB_BAD_ARGUMENT;
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
}

void CfsBridgeTester ::testFrameOversize() {
    U8 storage[8] = {0};
    // A buffer whose reported size cannot fit in a cFS message alongside the header
    Fw::Buffer buffer(storage, std::numeric_limits<FwSizeType>::max());
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_TELEM);

    // Buffer return and com status are still emitted; nothing is transmitted
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 0u);
}

void CfsBridgeTester ::testFlowControl() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND, true);

    U8 payloadOne[8];
    U8 payloadTwo[8];
    for (FwSizeType i = 0; i < sizeof(payloadOne); i++) {
        payloadOne[i] = static_cast<U8>(STest::Pick::any());
        payloadTwo[i] = static_cast<U8>(STest::Pick::any());
    }
    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payloadOne, sizeof(payloadOne));
    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payloadTwo, sizeof(payloadTwo));

    // While paused, no messages are deframed
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);

    // A com status success unpauses exactly one message
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(std::memcmp(this->fromPortHistory_dataOut->at(0).data.getData(), payloadOne, sizeof(payloadOne)), 0);
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(1);

    // A com status failure keeps the component paused
    Fw::Success failure = Fw::Success::FAILURE;
    this->invoke_to_comStatusIn(0, failure);
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(1);

    // The next success releases the second message
    this->invoke_to_comStatusIn(0, success);
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(2);
    ASSERT_EQ(std::memcmp(this->fromPortHistory_dataOut->at(1).data.getData(), payloadTwo, sizeof(payloadTwo)), 0);
}

void CfsBridgeTester ::testDataReturn() {
    U8 storage[16];
    Fw::Buffer buffer(storage, sizeof(storage));
    ComCfg::FrameContext context;
    this->clearHistory();
    this->invoke_to_dataReturnIn(0, buffer, context);
    // cFS does not return messages explicitly: no outputs are produced
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(0);
}

void CfsBridgeTester ::testRandomized() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND, true);

    // Shadow state mirroring the component's observable behavior
    bool paused = true;
    bool prerolled = false;
    U32 pending = 0;
    U32 transmits = 0;

    U8 payload[16];
    U8 storage[64];
    for (U32 step = 0; step < 1000; step++) {
        this->clearHistory();
        switch (STest::Pick::lowerUpper(0, 3)) {
            case 0: {  // Queue a software bus message
                if (pending < CfeStub::STUB_MAX_ENTRIES) {
                    for (FwSizeType i = 0; i < sizeof(payload); i++) {
                        payload[i] = static_cast<U8>(STest::Pick::any());
                    }
                    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payload, sizeof(payload));
                    pending++;
                }
                break;
            }
            case 1: {  // Process: preroll once, then deframe when unpaused
                ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
                U32 expectedStatus = prerolled ? 0 : 1;
                prerolled = true;
                ASSERT_from_comStatusOut_SIZE(expectedStatus);
                if (not paused and pending > 0) {
                    ASSERT_from_dataOut_SIZE(1);
                    pending--;
                    paused = true;
                } else {
                    ASSERT_from_dataOut_SIZE(0);
                }
                break;
            }
            case 2: {  // Unpause via com status
                Fw::Success success = Fw::Success::SUCCESS;
                this->invoke_to_comStatusIn(0, success);
                paused = false;
                break;
            }
            default: {  // Frame data out to the software bus
                Fw::Buffer buffer(storage, sizeof(storage));
                this->fillRandom(buffer, sizeof(storage));
                ComCfg::FrameContext context;
                context.set_apid(ComCfg::Apid::FW_PACKET_TELEM);
                this->invoke_to_dataIn(0, buffer, context);
                ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
                ASSERT_from_dataReturnOut_SIZE(1);
                U32 expectedStatus = prerolled ? 1 : 2;
                prerolled = true;
                ASSERT_from_comStatusOut_SIZE(expectedStatus);
                // The process() call also polls the software bus and may deframe a pending message
                if (not paused and pending > 0) {
                    ASSERT_from_dataOut_SIZE(1);
                    pending--;
                    paused = true;
                }
                transmits++;
                // Transmit records cap out at the stub maximum
                if (transmits <= CfeStub::STUB_MAX_ENTRIES) {
                    ASSERT_EQ(CfeStub::state().transmitCount, transmits);
                }
                break;
            }
        }
    }
}

}  // namespace FPrimeCfs
