// ======================================================================
// \title  CfsBridgeTester.cpp
// \brief  cpp file for CfsBridge component test harness implementation class
// ======================================================================

#include "CfsBridgeTester.hpp"
#include "config/FppConstantsAc.hpp"
#include <STest/Pick/Pick.hpp>
#include <cstring>
#include <limits>

namespace FPrimeCfs {

static const CFE_SB_MsgId_Atom_t CMD_MID_FOR_APID_0 = 0x1000;  // Space packet stream id: command type bit + APID 0
static const CFE_SB_MsgId_Atom_t TLM_MID_FOR_APID_1 = 0x0001;  // Space packet stream id: telemetry, APID 1
static const CFE_SB_MsgId_Atom_t CFS_CMD_MID_FOR_APID_0 = 0x1800;  // cFS command stream id: type bit + sec hdr flag + APID 0
static const CFE_SB_MsgId_Atom_t CFS_TLM_MID_FOR_APID_1 = 0x0801;  // cFS telemetry stream id: sec hdr flag + APID 1

static const FwSizeType HEADER_SIZE = CFS_BRIDGE_SPACE_PACKET_HEADER_SIZE;

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

Fw::Buffer CfsBridgeTester ::from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size) {
    this->pushFromPortEntry_bufferAllocate(size);
    if (this->m_failAllocation or (size > sizeof(this->m_allocStorage))) {
        return Fw::Buffer();
    }
    return Fw::Buffer(this->m_allocStorage, size);
}

void CfsBridgeTester ::configureAndSubscribe(ComCfg::Apid::T apid, bool paused) {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", paused), CFE_SUCCESS);
    ASSERT_EQ(this->component.subscribe(apid), CFE_SUCCESS);
}

FwSizeType CfsBridgeTester ::makePacket(U8* dest, U16 streamIdValue, const U8* payload, FwSizeType payloadSize) {
    dest[0] = static_cast<U8>((streamIdValue >> 8) & 0xFF);
    dest[1] = static_cast<U8>(streamIdValue & 0xFF);
    dest[2] = 0xC0;  // Sequence flags: unsegmented user data
    dest[3] = 0x00;
    FwSizeType lengthToken = payloadSize - 1;
    dest[4] = static_cast<U8>((lengthToken >> 8) & 0xFF);
    dest[5] = static_cast<U8>(lengthToken & 0xFF);
    (void)std::memcpy(&dest[HEADER_SIZE], payload, payloadSize);
    return HEADER_SIZE + payloadSize;
}

void CfsBridgeTester ::fillRandom(U8* data, FwSizeType size) {
    for (FwSizeType i = 0; i < size; i++) {
        data[i] = static_cast<U8>(STest::Pick::any());
    }
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
        (apid == ComCfg::Apid::FW_PACKET_COMMAND) ? (0x1000 | apid) : apid;
    CfeStub::queueMessage(msgIdValue, payload, size);
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
}

void CfsBridgeTester ::assertTransmitted(U32 index,
                                         CFE_SB_MsgId_Atom_t expectedMsgId,
                                         const U8* expectedPayload,
                                         FwSizeType expectedPayloadSize) {
    ASSERT_GT(CfeStub::state().transmitCount, index);
    const CfeStub::TransmitCall& call = CfeStub::state().transmitCalls[index];
    ASSERT_EQ(call.msgIdValue, expectedMsgId);
    ASSERT_EQ(call.totalSize, HEADER_SIZE + expectedPayloadSize);
    ASSERT_EQ(call.payloadSize, expectedPayloadSize);
    ASSERT_EQ(std::memcmp(call.payload, expectedPayload, expectedPayloadSize), 0);
    ASSERT_FALSE(call.incrementSequenceCount);
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
    // Command APIDs map to a stream identifier with the command type bit set
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_COMMAND), CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 1u);
    ASSERT_EQ(CfeStub::state().subscribeCalls[0].msgIdValue, CMD_MID_FOR_APID_0);
    // All other APIDs map to a telemetry (type bit clear) stream identifier
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_TELEM), CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 2u);
    ASSERT_EQ(CfeStub::state().subscribeCalls[1].msgIdValue, TLM_MID_FOR_APID_1);
}

void CfsBridgeTester ::testConfigureDepthTooLarge() {
    ASSERT_EQ(this->component.configure(static_cast<FwSizeType>(std::numeric_limits<uint16>::max()) + 1, "TEST_PIPE"),
              CFE_SB_BAD_ARGUMENT);
    // The pipe was never created
    ASSERT_EQ(CfeStub::state().createPipeCount, 0u);
}

void CfsBridgeTester ::testSubscribeCfs() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE"), CFE_SUCCESS);
    // COMMAND sets both the packet type bit and the secondary header flag
    ASSERT_EQ(this->component.subscribeCfs(ComCfg::Apid::FW_PACKET_COMMAND, CfsBridge::CfsMessageType::COMMAND),
              CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 1u);
    ASSERT_EQ(CfeStub::state().subscribeCalls[0].msgIdValue, CFS_CMD_MID_FOR_APID_0);
    // TELEMETRY sets only the secondary header flag
    ASSERT_EQ(this->component.subscribeCfs(ComCfg::Apid::FW_PACKET_TELEM, CfsBridge::CfsMessageType::TELEMETRY),
              CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 2u);
    ASSERT_EQ(CfeStub::state().subscribeCalls[1].msgIdValue, CFS_TLM_MID_FOR_APID_1);
}

void CfsBridgeTester ::testSubscribeFailure() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE"), CFE_SUCCESS);
    CfeStub::state().subscribeStatus = CFE_SB_MAX_MSGS_MET;
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_COMMAND), CFE_SB_MAX_MSGS_MET);
    // A failed subscription does not enable polling
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_EQ(CfeStub::state().receiveCount, 0u);
}

void CfsBridgeTester ::testSubscribeMessageLimit() {
    // Use a depth above the cFE default per-message-ID limit (4) so a default subscription would be caught
    const uint16 depth = 32;
    ASSERT_EQ(this->component.configure(depth, "TEST_PIPE"), CFE_SUCCESS);
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_COMMAND), CFE_SUCCESS);
    ASSERT_EQ(this->component.subscribeCfs(ComCfg::Apid::FW_PACKET_TELEM, CfsBridge::CfsMessageType::TELEMETRY),
              CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 2u);
    for (U32 i = 0; i < 2; i++) {
        const CfeStub::SubscribeCall& call = CfeStub::state().subscribeCalls[i];
        ASSERT_EQ(call.pipeId, CfeStub::state().subscribeCalls[0].pipeId);
        ASSERT_EQ(call.msgLim, depth);
        // Default quality of service
        ASSERT_EQ(call.quality.Priority, 0u);
        ASSERT_EQ(call.quality.Reliability, 0u);
    }
}

void CfsBridgeTester ::testSubscribeExplicitMessageLimit(const U16 limit) {
    const uint16 depth = 32;
    ASSERT_EQ(this->component.configure(depth, "TEST_PIPE"), CFE_SUCCESS);
    ASSERT_EQ(this->component.subscribe(ComCfg::Apid::FW_PACKET_COMMAND, limit), CFE_SUCCESS);
    ASSERT_EQ(this->component.subscribeCfs(ComCfg::Apid::FW_PACKET_TELEM, CfsBridge::CfsMessageType::TELEMETRY, limit),
              CFE_SUCCESS);
    ASSERT_EQ(CfeStub::state().subscribeCount, 2u);
    for (U32 i = 0; i < 2; i++) {
        const CfeStub::SubscribeCall& call = CfeStub::state().subscribeCalls[i];
        ASSERT_EQ(call.msgLim, limit);
        ASSERT_EQ(call.quality.Priority, 0u);
        ASSERT_EQ(call.quality.Reliability, 0u);
    }
}

// ----------------------------------------------------------------------
// Tests: transmit (dataIn -> software bus)
// ----------------------------------------------------------------------

void CfsBridgeTester ::testTransmitSinglePacket() {
    U8 payload[32];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, CMD_MID_FOR_APID_0, payload, sizeof(payload)));
    ComCfg::FrameContext context;

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    this->assertTransmitted(0, CMD_MID_FOR_APID_0, payload, sizeof(payload));
}

void CfsBridgeTester ::testTransmitMultiplePackets() {
    U8 payloadOne[16];
    U8 payloadTwo[24];
    this->fillRandom(payloadOne, sizeof(payloadOne));
    this->fillRandom(payloadTwo, sizeof(payloadTwo));
    U8 storage[128];
    FwSizeType sizeOne = this->makePacket(storage, TLM_MID_FOR_APID_1, payloadOne, sizeof(payloadOne));
    FwSizeType sizeTwo = this->makePacket(&storage[sizeOne], 0x0002, payloadTwo, sizeof(payloadTwo));
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(sizeOne + sizeTwo);
    ComCfg::FrameContext context;

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 2u);
    this->assertTransmitted(0, TLM_MID_FOR_APID_1, payloadOne, sizeof(payloadOne));
    this->assertTransmitted(1, 0x0002, payloadTwo, sizeof(payloadTwo));
}

void CfsBridgeTester ::testTransmitFailure() {
    U8 payload[16];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, TLM_MID_FOR_APID_1, payload, sizeof(payload)));
    ComCfg::FrameContext context;

    CfeStub::state().transmitStatus = CFE_SB_BAD_ARGUMENT;
    // Buffer return and com status are still emitted on failure
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
}

void CfsBridgeTester ::testTransmitTruncated() {
    U8 payload[32];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    FwSizeType packetSize = this->makePacket(storage, TLM_MID_FOR_APID_1, payload, sizeof(payload));
    // Truncate the buffer so the packet length field exceeds the available data
    buffer.setSize(packetSize - 4);
    ComCfg::FrameContext context;

    // Buffer return and com status are still emitted; nothing is transmitted
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 0u);
}

void CfsBridgeTester ::testTransmitResidual() {
    U8 payload[16];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    FwSizeType packetSize = this->makePacket(storage, TLM_MID_FOR_APID_1, payload, sizeof(payload));
    Fw::Buffer buffer(storage, sizeof(storage));
    // Trailing bytes too small to form a primary header are dropped
    buffer.setSize(packetSize + 3);
    ComCfg::FrameContext context;

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    this->assertTransmitted(0, TLM_MID_FOR_APID_1, payload, sizeof(payload));
}

void CfsBridgeTester ::testTransmitWrappedCommand() {
    // Enable F Prime command wrapping: command packets are transmitted as valid cFS command packets
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", false, true), CFE_SUCCESS);

    U8 payload[32];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, CMD_MID_FOR_APID_0, payload, sizeof(payload)));
    ComCfg::FrameContext context;

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    const CfeStub::TransmitCall& call = CfeStub::state().transmitCalls[0];
    // The secondary header flag is set in the stream identifier
    ASSERT_EQ(call.msgIdValue, CMD_MID_FOR_APID_0 | 0x0800);
    // The packet grows by the 2-byte cFS command secondary header, reflected in the length field
    ASSERT_EQ(call.totalSize, HEADER_SIZE + CFS_BRIDGE_CMD_SEC_HDR_SIZE + sizeof(payload));
    ASSERT_EQ(call.payloadSize, CFS_BRIDGE_CMD_SEC_HDR_SIZE + sizeof(payload));
    // The secondary header carries the F Prime passthrough function code, then the checksum
    ASSERT_EQ(call.payload[0], ComCfg::FprimeCommandFunctionCode);
    // The original payload follows the secondary header unmodified
    ASSERT_EQ(std::memcmp(&call.payload[CFS_BRIDGE_CMD_SEC_HDR_SIZE], payload, sizeof(payload)), 0);
    // The checksum is valid per CFE_MSG conventions: XOR of every packet byte with 0xFF equals zero
    U8 checksum = 0xFF;
    U8 header[HEADER_SIZE];
    header[0] = static_cast<U8>(((CMD_MID_FOR_APID_0 | 0x0800) >> 8) & 0xFF);
    header[1] = static_cast<U8>(CMD_MID_FOR_APID_0 & 0xFF);
    header[2] = 0xC0;
    header[3] = 0x00;
    const FwSizeType lengthToken = sizeof(payload) + CFS_BRIDGE_CMD_SEC_HDR_SIZE - 1;
    header[4] = static_cast<U8>((lengthToken >> 8) & 0xFF);
    header[5] = static_cast<U8>(lengthToken & 0xFF);
    for (FwSizeType i = 0; i < HEADER_SIZE; i++) {
        checksum ^= header[i];
    }
    for (FwSizeType i = 0; i < call.payloadSize; i++) {
        checksum ^= call.payload[i];
    }
    ASSERT_EQ(checksum, 0);
}

void CfsBridgeTester ::testTransmitWrapPassthrough() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", false, true), CFE_SUCCESS);

    // Telemetry packets (type bit clear) are transmitted unmodified
    U8 payload[16];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, TLM_MID_FOR_APID_1, payload, sizeof(payload)));
    ComCfg::FrameContext context;
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    this->assertTransmitted(0, TLM_MID_FOR_APID_1, payload, sizeof(payload));

    // Command packets that already carry a secondary header are transmitted unmodified
    const U16 cmdWithSecHdr = CMD_MID_FOR_APID_0 | 0x0800;
    buffer.setData(storage);
    buffer.setSize(this->makePacket(storage, cmdWithSecHdr, payload, sizeof(payload)));
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 2u);
    this->assertTransmitted(1, cmdWithSecHdr, payload, sizeof(payload));
}

void CfsBridgeTester ::testTransmitWrapTooLarge() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", false, true), CFE_SUCCESS);

    // A command packet whose wrapped size exceeds the wrap storage is dropped
    const FwSizeType payloadSize = CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE - HEADER_SIZE - CFS_BRIDGE_CMD_SEC_HDR_SIZE + 1;
    U8 storage[CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE + 64];
    U8 payload[CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE + 64 - HEADER_SIZE];
    this->fillRandom(payload, payloadSize);
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, CMD_MID_FOR_APID_0, payload, payloadSize));
    ComCfg::FrameContext context;

    // Buffer return and com status are still emitted; nothing is transmitted
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 0u);
}

void CfsBridgeTester ::testTransmitWrapExactBoundary() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", false, true), CFE_SUCCESS);

    // A command packet whose wrapped size exactly equals the wrap storage is transmitted
    const FwSizeType payloadSize = CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE - HEADER_SIZE - CFS_BRIDGE_CMD_SEC_HDR_SIZE;
    U8 storage[CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE + 64];
    U8 payload[CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE + 64 - HEADER_SIZE];
    this->fillRandom(payload, payloadSize);
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, CMD_MID_FOR_APID_0, payload, payloadSize));
    ComCfg::FrameContext context;

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
    const CfeStub::TransmitCall& call = CfeStub::state().transmitCalls[0];
    ASSERT_EQ(call.totalSize, CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE);
    ASSERT_EQ(call.payload[0], ComCfg::FprimeCommandFunctionCode);
    ASSERT_EQ(std::memcmp(&call.payload[CFS_BRIDGE_CMD_SEC_HDR_SIZE], payload, payloadSize), 0);
}

void CfsBridgeTester ::testTransmitWrapMultiplePackets() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", false, true), CFE_SUCCESS);

    // A buffer holding a command packet followed by a telemetry packet: the command is wrapped,
    // the telemetry packet after it is still found and transmitted unmodified
    U8 payloadOne[16];
    U8 payloadTwo[24];
    this->fillRandom(payloadOne, sizeof(payloadOne));
    this->fillRandom(payloadTwo, sizeof(payloadTwo));
    U8 storage[128];
    FwSizeType offset = this->makePacket(storage, CMD_MID_FOR_APID_0, payloadOne, sizeof(payloadOne));
    offset += this->makePacket(&storage[offset], TLM_MID_FOR_APID_1, payloadTwo, sizeof(payloadTwo));
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(offset);
    ComCfg::FrameContext context;

    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 2u);
    const CfeStub::TransmitCall& first = CfeStub::state().transmitCalls[0];
    ASSERT_EQ(first.msgIdValue, CMD_MID_FOR_APID_0 | 0x0800);
    ASSERT_EQ(first.totalSize, HEADER_SIZE + CFS_BRIDGE_CMD_SEC_HDR_SIZE + sizeof(payloadOne));
    ASSERT_EQ(std::memcmp(&first.payload[CFS_BRIDGE_CMD_SEC_HDR_SIZE], payloadOne, sizeof(payloadOne)), 0);
    this->assertTransmitted(1, TLM_MID_FOR_APID_1, payloadTwo, sizeof(payloadTwo));
}

void CfsBridgeTester ::testTransmitWrapFailure() {
    ASSERT_EQ(this->component.configure(10, "TEST_PIPE", false, true), CFE_SUCCESS);

    U8 payload[16];
    this->fillRandom(payload, sizeof(payload));
    U8 storage[64];
    Fw::Buffer buffer(storage, sizeof(storage));
    buffer.setSize(this->makePacket(storage, CMD_MID_FOR_APID_0, payload, sizeof(payload)));
    ComCfg::FrameContext context;

    CfeStub::state().transmitStatus = CFE_SB_BAD_ARGUMENT;
    // Buffer return and com status are still emitted when the wrapped transmit fails
    this->sendDataIn(buffer, context);
    ASSERT_EQ(CfeStub::state().transmitCount, 1u);
}

// ----------------------------------------------------------------------
// Tests: receive (software bus -> dataOut)
// ----------------------------------------------------------------------

void CfsBridgeTester ::testReceive() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);

    U8 payload[32];
    this->fillRandom(payload, sizeof(payload));
    this->clearHistory();
    this->receiveMessage(ComCfg::Apid::FW_PACKET_COMMAND, payload, sizeof(payload));

    // The complete message (headers included) is emitted with a default context
    ASSERT_from_dataOut_SIZE(1);
    const Fw::Buffer& outBuffer = this->fromPortHistory_dataOut->at(0).data;
    const ComCfg::FrameContext& outContext = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(outBuffer.getSize(), HEADER_SIZE + sizeof(payload));
    ASSERT_EQ(std::memcmp(outBuffer.getData() + HEADER_SIZE, payload, sizeof(payload)), 0);
    ComCfg::FrameContext defaultContext;
    ASSERT_EQ(outContext, defaultContext);
    // The emitted buffer is an allocated copy, not an alias of software bus memory
    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_EQ(outBuffer.getData(), this->m_allocStorage);
}

void CfsBridgeTester ::testReceiveAllocationFailure() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);

    U8 payload[32];
    this->fillRandom(payload, sizeof(payload));
    this->clearHistory();
    this->m_failAllocation = true;
    this->receiveMessage(ComCfg::Apid::FW_PACKET_COMMAND, payload, sizeof(payload));
    this->m_failAllocation = false;

    // The message is dropped: nothing is emitted and nothing is deallocated
    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_bufferDeallocate_SIZE(0);
}

void CfsBridgeTester ::testSchedIn() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);

    U8 payload[16] = {0};
    this->fillRandom(payload, sizeof(payload));
    this->clearHistory();
    CFE_SB_MsgId_Atom_t msgIdValue = 0x1000 | ComCfg::Apid::FW_PACKET_COMMAND;
    CfeStub::queueMessage(msgIdValue, payload, sizeof(payload));

    // One schedIn tick drains the queue and polls the software bus once
    this->invoke_to_schedIn(0, 0);
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).data.getSize(), HEADER_SIZE + sizeof(payload));
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

void CfsBridgeTester ::testReceiveNoMessage() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_GT(CfeStub::state().receiveCount, 0u);
}

void CfsBridgeTester ::testReceiveError() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    U8 payload[8] = {0};
    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payload, sizeof(payload));
    CfeStub::state().receiveStatus = CFE_SB_PIPE_RD_ERR;
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
}

void CfsBridgeTester ::testReceiveGetSizeFailure() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND);
    U8 payload[8] = {0};
    CfeStub::queueMessage(CMD_MID_FOR_APID_0, payload, sizeof(payload));
    CfeStub::state().getSizeStatus = CFE_SB_BAD_ARGUMENT;
    this->clearHistory();
    ASSERT_NE(this->component.process(), Fw::QueuedComponentBase::MSG_DISPATCH_EXIT);
    ASSERT_from_dataOut_SIZE(0);
}

void CfsBridgeTester ::testFlowControl() {
    this->configureAndSubscribe(ComCfg::Apid::FW_PACKET_COMMAND, true);

    U8 payloadOne[8];
    U8 payloadTwo[8];
    this->fillRandom(payloadOne, sizeof(payloadOne));
    this->fillRandom(payloadTwo, sizeof(payloadTwo));
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
    ASSERT_EQ(std::memcmp(this->fromPortHistory_dataOut->at(0).data.getData() + HEADER_SIZE,
                          payloadOne, sizeof(payloadOne)), 0);
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
    ASSERT_EQ(std::memcmp(this->fromPortHistory_dataOut->at(1).data.getData() + HEADER_SIZE,
                          payloadTwo, sizeof(payloadTwo)), 0);
}

void CfsBridgeTester ::testDataReturn() {
    U8 storage[16];
    Fw::Buffer buffer(storage, sizeof(storage));
    ComCfg::FrameContext context;
    this->clearHistory();
    this->invoke_to_dataReturnIn(0, buffer, context);
    // The returned buffer is handed back to the allocator; nothing else is produced
    ASSERT_from_bufferDeallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_bufferDeallocate->at(0).fwBuffer.getData(), storage);
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
                    this->fillRandom(payload, sizeof(payload));
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
            default: {  // Transmit a space packet out to the software bus
                this->fillRandom(payload, sizeof(payload));
                Fw::Buffer buffer(storage, sizeof(storage));
                buffer.setSize(this->makePacket(storage, TLM_MID_FOR_APID_1, payload, sizeof(payload)));
                ComCfg::FrameContext context;
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
