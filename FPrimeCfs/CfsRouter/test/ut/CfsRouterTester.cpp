// ======================================================================
// \title  CfsRouterTester.cpp
// \brief  cpp file for CfsRouter component test harness implementation class
// ======================================================================

#include "CfsRouterTester.hpp"
#include <STest/Pick/Pick.hpp>
#include <cstring>

namespace FPrimeCfs
{

// APIDs matching the static routing table configured in CfsRouterCfg.fpp
static const ComCfg::Apid::T FPRIME_CMD_APID = ComCfg::Apid::FW_PACKET_COMMAND;
static const ComCfg::Apid::T CFS_CMD_APID    = ComCfg::Apid::FW_PACKET_HAND;
static const ComCfg::Apid::T CFS_TLM_APID    = ComCfg::Apid::FW_PACKET_TELEM;
static const ComCfg::Apid::T FILE_APID       = ComCfg::Apid::FW_PACKET_FILE;
static const ComCfg::Apid::T UNKNOWN_APID    = ComCfg::Apid::FW_PACKET_LOG;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CfsRouterTester ::CfsRouterTester(bool connectOutputs)
    : CfsRouterGTestBase("CfsRouterTester", CfsRouterTester::MAX_HISTORY_SIZE), component("CfsRouter")
{
    this->initComponents();
    if (connectOutputs)
    {
        this->connectPorts();
    }
    else
    {
        // Connect only the input ports and the data return path, leaving all
        // route output ports disconnected
        this->connect_to_dataIn(0, this->component.get_dataIn_InputPort(0));
        this->connect_to_bufferReturnIn(0, this->component.get_bufferReturnIn_InputPort(0));
        this->component.set_dataReturnOut_OutputPort(0, this->get_from_dataReturnOut(0));
        this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
        this->component.set_logOut_OutputPort(0, this->get_from_logOut(0));
        this->component.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
    }
}

CfsRouterTester ::~CfsRouterTester() {}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void CfsRouterTester ::sendData(ComCfg::Apid::T apid, bool hasSecHdr, U8 *bytes, FwSizeType size)
{
    Fw::Buffer           buffer(bytes, size);
    ComCfg::FrameContext context;
    context.set_apid(apid);
    context.set_hasSecHdr(hasSecHdr);
    const bool isCommand = (apid == FPRIME_CMD_APID) || (apid == CFS_CMD_APID);
    context.set_pktType(isCommand ? ComCfg::SppPacketType::SPP_COMMAND : ComCfg::SppPacketType::SPP_TELEMETRY);
    if (isCommand && hasSecHdr && (size >= CFS_ROUTER_CMD_SEC_HDR_SIZE))
    {
        // Make the cFS command checksum valid (or deliberately invalid) over the reconstructed packet
        bytes[1]          = 0;
        const U8 residual = CfsRouter::computeChecksumResidual(buffer, context);
        bytes[1]          = this->m_corruptChecksum ? static_cast<U8>(residual ^ 0x5A) : residual;
    }
    this->invoke_to_dataIn(0, buffer, context);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void CfsRouterTester ::testRouteFprimeCommand()
{
    U8 bytes[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    // A command packet without the cFS command secondary header cannot be checksum-verified: not routed
    this->sendData(FPRIME_CMD_APID, false, bytes, sizeof(bytes));
    ASSERT_from_commandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(1);
    ASSERT_EVENTS_CommandMissingSecondaryHeader_SIZE(1);
    Fw::Buffer buffer = this->fromPortHistory_unknownDataOut->at(0).data;
    this->invoke_to_bufferReturnIn(0, buffer);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), FPRIME_CMD_APID);
}

void CfsRouterTester ::testRouteFprimeCommandSecHdr()
{
    U8 bytes[8] = {0x00, 0x55, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04}; // fn code 0, checksum, F Prime command
    this->sendData(FPRIME_CMD_APID, true, bytes, sizeof(bytes));
    // The 2-byte cFS command secondary header is excluded from the copy
    ASSERT_from_commandOut_SIZE(1);
    const Fw::ComBuffer &com = this->fromPortHistory_commandOut->at(0).data;
    ASSERT_EQ(com.getBuffLength(), sizeof(bytes) - CFS_ROUTER_CMD_SEC_HDR_SIZE);
    ASSERT_EQ(std::memcmp(com.getBuffAddr(), &bytes[CFS_ROUTER_CMD_SEC_HDR_SIZE],
                          sizeof(bytes) - CFS_ROUTER_CMD_SEC_HDR_SIZE),
              0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EVENTS_SIZE(0);
}

void CfsRouterTester ::testRouteFprimeCommandTooLarge()
{
    U8 bytes[FW_COM_BUFFER_MAX_SIZE + CFS_ROUTER_CMD_SEC_HDR_SIZE + 1] = {};
    this->sendData(FPRIME_CMD_APID, true, bytes, sizeof(bytes));
    ASSERT_from_commandOut_SIZE(0);
    ASSERT_EVENTS_SerializationError_SIZE(1);
    // Buffer still returned
    ASSERT_from_dataReturnOut_SIZE(1);
}

void CfsRouterTester ::testRouteCfsCommand()
{
    U8 bytes[6] = {0x2A, 0xFF, 0x11, 0x22, 0x33, 0x44}; // fn code 0x2A, checksum, payload
    this->sendData(CFS_CMD_APID, true, bytes, sizeof(bytes));
    ASSERT_from_cfsCommandOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_cfsCommandOut->at(0).functionCode, 0x2A);
    Fw::Buffer payload = this->fromPortHistory_cfsCommandOut->at(0).data;
    ASSERT_EQ(payload.getData(), &bytes[CFS_ROUTER_CMD_SEC_HDR_SIZE]);
    ASSERT_EQ(payload.getSize(), sizeof(bytes) - CFS_ROUTER_CMD_SEC_HDR_SIZE);
    // Ownership not yet returned
    ASSERT_from_dataReturnOut_SIZE(0);
    // Return the buffer; it is forwarded to dataReturnOut
    this->invoke_to_bufferReturnIn(0, payload);
    ASSERT_from_dataReturnOut_SIZE(1);
    // The original buffer is returned with the context it was received with
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), sizeof(bytes));
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), CFS_CMD_APID);
    ASSERT_EVENTS_SIZE(0);
}

void CfsRouterTester ::testRouteCfsTelemetry()
{
    // Big-endian time: seconds = 0x01020304, subseconds16 = 0xABCD; 4 spare bytes; then payload
    U8 bytes[13] = {0x01, 0x02, 0x03, 0x04, 0xAB, 0xCD, 0x00, 0x00, 0x00, 0x00, 0x77, 0x88, 0x99};
    this->sendData(CFS_TLM_APID, true, bytes, sizeof(bytes));
    ASSERT_from_cfsTelemetryOut_SIZE(1);
    const CfsTime &time = this->fromPortHistory_cfsTelemetryOut->at(0).sysTime;
    ASSERT_EQ(time.get_seconds(), 0x01020304u);
    ASSERT_EQ(time.get_subseconds(), 0xABCD0000u);
    Fw::Buffer payload = this->fromPortHistory_cfsTelemetryOut->at(0).data;
    ASSERT_EQ(payload.getData(), &bytes[CFS_ROUTER_TLM_SEC_HDR_SIZE]);
    ASSERT_EQ(payload.getSize(), sizeof(bytes) - CFS_ROUTER_TLM_SEC_HDR_SIZE);
    ASSERT_from_dataReturnOut_SIZE(0);
    this->invoke_to_bufferReturnIn(0, payload);
    ASSERT_from_dataReturnOut_SIZE(1);
    // The original buffer is returned with the context it was received with
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), sizeof(bytes));
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), CFS_TLM_APID);
    ASSERT_EVENTS_SIZE(0);
}

void CfsRouterTester ::testRouteFile()
{
    U8 bytes[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    this->sendData(FILE_APID, false, bytes, sizeof(bytes));
    ASSERT_from_fileOut_SIZE(1);
    Fw::Buffer buffer = this->fromPortHistory_fileOut->at(0).fwBuffer;
    ASSERT_EQ(buffer.getData(), bytes);
    ASSERT_EQ(buffer.getSize(), sizeof(bytes));
    // Ownership not yet returned
    ASSERT_from_dataReturnOut_SIZE(0);
    this->invoke_to_fileBufferReturnIn(0, buffer);
    ASSERT_from_dataReturnOut_SIZE(1);
    // The buffer is returned with the context it was received with
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), FILE_APID);
    ASSERT_EVENTS_SIZE(0);
}

void CfsRouterTester ::testRouteUnknown()
{
    U8 bytes[4] = {0x01, 0x02, 0x03, 0x04};
    this->sendData(UNKNOWN_APID, false, bytes, sizeof(bytes));
    ASSERT_from_unknownDataOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_unknownDataOut->at(0).context.get_apid(), UNKNOWN_APID);
    Fw::Buffer buffer = this->fromPortHistory_unknownDataOut->at(0).data;
    ASSERT_EQ(buffer.getData(), bytes);
    ASSERT_from_dataReturnOut_SIZE(0);
    this->invoke_to_bufferReturnIn(0, buffer);
    ASSERT_from_dataReturnOut_SIZE(1);
    // The buffer is returned with the context it was received with
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), UNKNOWN_APID);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_NoRoute_SIZE(1);
    ASSERT_EVENTS_NoRoute(0, static_cast<U16>(UNKNOWN_APID), 0);
}

void CfsRouterTester ::testBadChecksum()
{
    U8 bytes[6]             = {0x2A, 0x00, 0x11, 0x22, 0x33, 0x44};
    this->m_corruptChecksum = true;
    this->sendData(CFS_CMD_APID, true, bytes, sizeof(bytes));
    this->m_corruptChecksum = false;
    // Dropped before routing: nothing forwarded, buffer returned, warning raised
    ASSERT_from_cfsCommandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_BadChecksum_SIZE(1);
    ASSERT_EQ(this->eventHistory_BadChecksum->at(0).apid, static_cast<U16>(CFS_CMD_APID));
    ASSERT_EQ(this->eventHistory_BadChecksum->at(0).functionCode, 0x2A);
    ASSERT_EQ(this->eventHistory_BadChecksum->at(0).checksum, 0x5A);
}

void CfsRouterTester ::testNoRouteFunctionCode()
{
    // Valid command on a routed APID, but a function code with no route
    U8 bytes[6] = {0x2B, 0x00, 0x11, 0x22, 0x33, 0x44};
    this->sendData(CFS_CMD_APID, true, bytes, sizeof(bytes));
    ASSERT_from_cfsCommandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(1);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_NoRoute(0, static_cast<U16>(CFS_CMD_APID), 0x2B);
    Fw::Buffer buffer = this->fromPortHistory_unknownDataOut->at(0).data;
    this->invoke_to_bufferReturnIn(0, buffer);
    ASSERT_from_dataReturnOut_SIZE(1);
    // A telemetry packet on the same APID value is not a command and does not match the command route
    U8                   tlmBytes[8] = {};
    ComCfg::FrameContext context;
    context.set_apid(CFS_CMD_APID);
    context.set_hasSecHdr(true);
    context.set_pktType(ComCfg::SppPacketType::SPP_TELEMETRY);
    Fw::Buffer tlm(tlmBytes, sizeof(tlmBytes));
    this->invoke_to_dataIn(0, tlm, context);
    ASSERT_from_cfsCommandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(2);
    ASSERT_EVENTS_NoRoute_SIZE(2);
}

void CfsRouterTester ::testDataReturnIn()
{
    U8 bytes[4] = {0x01, 0x02, 0x03, 0x04};
    this->sendData(UNKNOWN_APID, false, bytes, sizeof(bytes));
    ASSERT_from_unknownDataOut_SIZE(1);
    Fw::Buffer buffer = this->fromPortHistory_unknownDataOut->at(0).data;
    ASSERT_from_dataReturnOut_SIZE(0);
    // Return the buffer with a different context; the original context is restored
    ComCfg::FrameContext otherContext;
    otherContext.set_apid(FPRIME_CMD_APID);
    this->invoke_to_dataReturnIn(0, buffer, otherContext);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), UNKNOWN_APID);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_NoRoute_SIZE(1);
}

void CfsRouterTester ::testDataReturnInUntracked()
{
    // A buffer that was never tracked is returned as-is with the supplied context
    U8                   bytes[4] = {0x0A, 0x0B, 0x0C, 0x0D};
    Fw::Buffer           buffer(bytes, sizeof(bytes));
    ComCfg::FrameContext context;
    context.set_apid(FPRIME_CMD_APID);
    this->invoke_to_dataReturnIn(0, buffer, context);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), bytes);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context.get_apid(), FPRIME_CMD_APID);
    ASSERT_EVENTS_SIZE(0);
}

void CfsRouterTester ::testMissingSecondaryHeader()
{
    U8 bytes[6] = {0x2A, 0xFF, 0x11, 0x22, 0x33, 0x44};
    // cFS command APID but no secondary header flag
    this->sendData(CFS_CMD_APID, false, bytes, sizeof(bytes));
    ASSERT_from_cfsCommandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(1);
    ASSERT_EVENTS_CommandMissingSecondaryHeader_SIZE(1);
    ASSERT_EVENTS_MissingSecondaryHeader_SIZE(0);
    // Return the first buffer so the pending map does not accumulate
    Fw::Buffer first = this->fromPortHistory_unknownDataOut->at(0).data;
    this->invoke_to_bufferReturnIn(0, first);
    // cFS telemetry APID but no secondary header flag
    this->sendData(CFS_TLM_APID, false, bytes, sizeof(bytes));
    ASSERT_from_cfsTelemetryOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(2);
    ASSERT_EVENTS_CommandMissingSecondaryHeader_SIZE(1);
    ASSERT_EVENTS_MissingSecondaryHeader_SIZE(1);
}

void CfsRouterTester ::testShortSecondaryHeader()
{
    U8 bytes[1] = {0x2A};
    this->sendData(CFS_CMD_APID, true, bytes, sizeof(bytes));
    ASSERT_from_cfsCommandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(1);
    ASSERT_EVENTS_CommandMissingSecondaryHeader_SIZE(1);
    Fw::Buffer first = this->fromPortHistory_unknownDataOut->at(0).data;
    this->invoke_to_bufferReturnIn(0, first);
    U8 tlmBytes[CFS_ROUTER_TLM_SEC_HDR_SIZE - 1] = {};
    this->sendData(CFS_TLM_APID, true, tlmBytes, sizeof(tlmBytes));
    ASSERT_from_cfsTelemetryOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(2);
    ASSERT_EVENTS_CommandMissingSecondaryHeader_SIZE(1);
    ASSERT_EVENTS_MissingSecondaryHeader_SIZE(1);
}

void CfsRouterTester ::testDisconnectedOutputs()
{
    // Constructed with connectOutputs == false: all route outputs disconnected
    U8 bytes[8] = {0x2A, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    this->sendData(FPRIME_CMD_APID, false, bytes, sizeof(bytes));
    ASSERT_from_dataReturnOut_SIZE(1);
    this->sendData(CFS_CMD_APID, true, bytes, sizeof(bytes));
    ASSERT_from_dataReturnOut_SIZE(2);
    this->sendData(CFS_TLM_APID, true, bytes, sizeof(bytes));
    ASSERT_from_dataReturnOut_SIZE(3);
    this->sendData(UNKNOWN_APID, false, bytes, sizeof(bytes));
    ASSERT_from_dataReturnOut_SIZE(4);
    this->sendData(FILE_APID, false, bytes, sizeof(bytes));
    ASSERT_from_dataReturnOut_SIZE(5);
}

void CfsRouterTester ::testCommandResponseNoop()
{
    this->invoke_to_cmdResponseIn(0, 0x123, 7, Fw::CmdResponse::OK);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_commandOut_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void CfsRouterTester ::testRandomized()
{
    U32 returned = 0;
    for (U32 i = 0; i < 1000; i++)
    {
        U8 bytes[32];
        for (FwSizeType j = 0; j < sizeof(bytes); j++)
        {
            bytes[j] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
        }
        const FwSizeType size =
            static_cast<FwSizeType>(STest::Pick::lowerUpper(CFS_ROUTER_TLM_SEC_HDR_SIZE, sizeof(bytes)));
        const U32                    pick      = STest::Pick::lowerUpper(0, 4);
        static const ComCfg::Apid::T APIDS[5]  = {FPRIME_CMD_APID, CFS_CMD_APID, CFS_TLM_APID, FILE_APID, UNKNOWN_APID};
        const bool                   hasSecHdr = (STest::Pick::lowerUpper(0, 1) == 1);
        this->sendData(APIDS[pick], hasSecHdr, bytes, size);
        // Return any buffer handed out on a pass-through route
        if (this->fromPortHistory_cfsCommandOut->size() > 0)
        {
            Fw::Buffer buffer = this->fromPortHistory_cfsCommandOut->at(0).data;
            this->invoke_to_bufferReturnIn(0, buffer);
        }
        else if (this->fromPortHistory_cfsTelemetryOut->size() > 0)
        {
            Fw::Buffer buffer = this->fromPortHistory_cfsTelemetryOut->at(0).data;
            this->invoke_to_bufferReturnIn(0, buffer);
        }
        else if (this->fromPortHistory_unknownDataOut->size() > 0)
        {
            Fw::Buffer buffer = this->fromPortHistory_unknownDataOut->at(0).data;
            this->invoke_to_bufferReturnIn(0, buffer);
        }
        else if (this->fromPortHistory_fileOut->size() > 0)
        {
            Fw::Buffer buffer = this->fromPortHistory_fileOut->at(0).fwBuffer;
            this->invoke_to_fileBufferReturnIn(0, buffer);
        }
        // Every message results in exactly one buffer return
        ASSERT_from_dataReturnOut_SIZE(1);
        returned++;
        this->clearHistory();
    }
    ASSERT_EQ(returned, 1000u);
}

void CfsRouterTester ::testLiteralChecksumVector()
{
    // Hand-computed per CFE_MSG: XOR of the complete packet seeded with 0xFF is zero.
    // Primary header 18 FE C0 00 00 05 (command, sec hdr, APID 0x0FE, seq 0, length 5),
    // secondary header {fc 0x2A, checksum 0xB2}, payload 11 22 33 44
    U8                   bytes[6] = {0x2A, 0xB2, 0x11, 0x22, 0x33, 0x44};
    Fw::Buffer           buffer(bytes, sizeof(bytes));
    ComCfg::FrameContext context;
    context.set_apid(CFS_CMD_APID);
    context.set_hasSecHdr(true);
    context.set_sequenceCount(0);
    context.set_pktType(ComCfg::SppPacketType::SPP_COMMAND);
    this->invoke_to_dataIn(0, buffer, context);
    ASSERT_EVENTS_BadChecksum_SIZE(0);
    ASSERT_from_cfsCommandOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_cfsCommandOut->at(0).functionCode, 0x2A);
    // One flipped payload bit must be caught
    Fw::Buffer routed = this->fromPortHistory_cfsCommandOut->at(0).data;
    this->invoke_to_bufferReturnIn(0, routed);
    bytes[5] ^= 0x01;
    this->invoke_to_dataIn(0, buffer, context);
    ASSERT_EVENTS_BadChecksum_SIZE(1);
    ASSERT_from_cfsCommandOut_SIZE(1);
}

} // namespace FPrimeCfs
