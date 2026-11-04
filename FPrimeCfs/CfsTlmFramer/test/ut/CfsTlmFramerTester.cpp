// ======================================================================
// \title  CfsTlmFramerTester.cpp
// \brief  cpp file for CfsTlmFramer component test harness implementation class
// ======================================================================

#include "CfsTlmFramerTester.hpp"
#include "Fw/Types/Serializable.hpp"
#include "STest/Random/Random.hpp"

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CfsTlmFramerTester ::CfsTlmFramerTester()
    : CfsTlmFramerGTestBase("CfsTlmFramerTester", CfsTlmFramerTester::MAX_HISTORY_SIZE), component("CfsTlmFramer")
{
    this->initComponents();
    this->connectPorts();
}

CfsTlmFramerTester ::~CfsTlmFramerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void CfsTlmFramerTester ::checkFraming(const FwPacketDescriptorType descriptor, const Fw::Time &time)
{
    Fw::ExternalSerializeBuffer packet(this->m_packetStorage, sizeof(this->m_packetStorage));
    ASSERT_EQ(packet.serializeFrom(descriptor), Fw::FW_SERIALIZE_OK);
    switch (descriptor)
    {
        case static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_LOG):
        {
            const FwEventIdType id = 0x123;
            ASSERT_EQ(packet.serializeFrom(id), Fw::FW_SERIALIZE_OK);
            break;
        }
        case static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_TELEM):
        {
            const FwChanIdType id = 0x456;
            ASSERT_EQ(packet.serializeFrom(id), Fw::FW_SERIALIZE_OK);
            break;
        }
        case static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_PACKETIZED_TLM):
        {
            const FwTlmPacketizeIdType id = 0x78;
            ASSERT_EQ(packet.serializeFrom(id), Fw::FW_SERIALIZE_OK);
            break;
        }
        default:
            FAIL() << "Unsupported descriptor in test helper";
            break;
    }
    ASSERT_EQ(packet.serializeFrom(time), Fw::FW_SERIALIZE_OK);
    const U8 payloadByte = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));
    ASSERT_EQ(packet.serializeFrom(payloadByte), Fw::FW_SERIALIZE_OK);

    Fw::Buffer           data(this->m_packetStorage, packet.getSize());
    ComCfg::FrameContext context;
    // As set upstream by CfsAppBridge: the APID identifies the F Prime packet kind
    context.set_apid(static_cast<ComCfg::Apid::T>(descriptor));
    this->invoke_to_dataIn(0, data, context);

    ASSERT_from_dataOut_SIZE(1);
    Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(outBuffer.getSize(), data.getSize() + CFS_TLM_FRAMER_SEC_HDR_SIZE);
    ASSERT_TRUE(this->fromPortHistory_dataOut->at(0).context.get_hasSecHdr());

    // Expected secondary header: big-endian seconds and top 16 bits of subseconds
    const U32       seconds      = time.getSeconds();
    const U32       subseconds   = static_cast<U32>((static_cast<U64>(time.getUSeconds()) << 32) / 1000000ULL);
    const U16       subseconds16 = static_cast<U16>(subseconds >> 16);
    const U8 *const header       = outBuffer.getData();
    ASSERT_EQ(header[0], static_cast<U8>(seconds >> 24));
    ASSERT_EQ(header[1], static_cast<U8>((seconds >> 16) & 0xFF));
    ASSERT_EQ(header[2], static_cast<U8>((seconds >> 8) & 0xFF));
    ASSERT_EQ(header[3], static_cast<U8>(seconds & 0xFF));
    ASSERT_EQ(header[4], static_cast<U8>(subseconds16 >> 8));
    ASSERT_EQ(header[5], static_cast<U8>(subseconds16 & 0xFF));

    // The original packet follows the secondary header unchanged
    for (FwSizeType i = 0; i < data.getSize(); i++)
    {
        ASSERT_EQ(header[CFS_TLM_FRAMER_SEC_HDR_SIZE + i], this->m_packetStorage[i]);
    }

    // Original buffer returned; no time extraction warnings
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), this->m_packetStorage);
    ASSERT_EVENTS_TimeExtractionFailed_SIZE(0);
}

void CfsTlmFramerTester ::testLogPacketFraming()
{
    const Fw::Time time(TimeBase::TB_WORKSTATION_TIME, 1234567, 250000);
    this->checkFraming(static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_LOG), time);
}

void CfsTlmFramerTester ::testTlmPacketFraming()
{
    const Fw::Time time(TimeBase::TB_WORKSTATION_TIME, 42, 999999);
    this->checkFraming(static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_TELEM), time);
}

void CfsTlmFramerTester ::testPacketizedTlmFraming()
{
    const Fw::Time time(TimeBase::TB_WORKSTATION_TIME, 0xDEADBEEF, 500000);
    this->checkFraming(static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_PACKETIZED_TLM), time);
}

void CfsTlmFramerTester ::testUnknownPacketFallback()
{
    // A packet with an unknown descriptor: time extraction fails and the
    // current system time (test harness time) is used
    Fw::ExternalSerializeBuffer  packet(this->m_packetStorage, sizeof(this->m_packetStorage));
    const FwPacketDescriptorType descriptor = static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_UNKNOWN);
    ASSERT_EQ(packet.serializeFrom(descriptor), Fw::FW_SERIALIZE_OK);

    const U32 seconds = 777;
    this->setTestTime(Fw::Time(TimeBase::TB_WORKSTATION_TIME, seconds, 0));

    Fw::Buffer           data(this->m_packetStorage, packet.getSize());
    ComCfg::FrameContext context;
    // An F Prime telemetry APID whose body does not carry the expected packet layout
    context.set_apid(ComCfg::Apid::FW_PACKET_TELEM);
    this->invoke_to_dataIn(0, data, context);

    ASSERT_EVENTS_TimeExtractionFailed_SIZE(1);
    ASSERT_from_dataOut_SIZE(1);
    Fw::Buffer      outBuffer = this->fromPortHistory_dataOut->at(0).data;
    const U8 *const header    = outBuffer.getData();
    ASSERT_EQ(header[0], static_cast<U8>(seconds >> 24));
    ASSERT_EQ(header[3], static_cast<U8>(seconds & 0xFF));
}

void CfsTlmFramerTester ::testRawPayloadCurrentTime()
{
    // A raw cFS payload (non-F Prime APID set by the source) carries no time tag of its own:
    // it is stamped with the current time and no extraction warning is raised
    for (U32 i = 0; i < 8; i++)
    {
        this->m_packetStorage[i] = static_cast<U8>(0xA0 + i);
    }
    const U32 seconds = 4242;
    this->setTestTime(Fw::Time(TimeBase::TB_WORKSTATION_TIME, seconds, 0));
    Fw::Buffer           data(this->m_packetStorage, 8);
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_HAND);
    this->invoke_to_dataIn(0, data, context);
    ASSERT_EVENTS_TimeExtractionFailed_SIZE(0);
    ASSERT_from_dataOut_SIZE(1);
    Fw::Buffer      outBuffer = this->fromPortHistory_dataOut->at(0).data;
    const U8 *const header    = outBuffer.getData();
    ASSERT_EQ(outBuffer.getSize(), 8 + CFS_TLM_FRAMER_SEC_HDR_SIZE);
    ASSERT_EQ(header[0], static_cast<U8>(seconds >> 24));
    ASSERT_EQ(header[3], static_cast<U8>(seconds & 0xFF));
    for (U32 i = 0; i < 8; i++)
    {
        ASSERT_EQ(header[CFS_TLM_FRAMER_SEC_HDR_SIZE + i], static_cast<U8>(0xA0 + i));
    }
    const ComCfg::FrameContext &outContext = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(outContext.get_apid(), ComCfg::Apid::FW_PACKET_HAND);
    ASSERT_EQ(outContext.get_pktType(), ComCfg::SppPacketType::SPP_TELEMETRY);
    ASSERT_TRUE(outContext.get_hasSecHdr());
}

void CfsTlmFramerTester ::testAllocationFailure()
{
    U8                   payload[16] = {};
    Fw::Buffer           data(payload, sizeof(payload));
    ComCfg::FrameContext context;

    this->m_useUndersizedAlloc = true;
    this->invoke_to_dataIn(0, data, context);
    this->m_useUndersizedAlloc = false;

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), payload);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    ASSERT_from_bufferDeallocate_SIZE(1);
}

void CfsTlmFramerTester ::testComStatusPassthrough()
{
    Fw::Success status = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, status);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(0).condition, status);
    this->clearHistory();
    status = Fw::Success::FAILURE;
    this->invoke_to_comStatusIn(0, status);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(0).condition, status);
}

void CfsTlmFramerTester ::testDataReturnPassthrough()
{
    U8                   data[8] = {};
    Fw::Buffer           buffer(data, sizeof(data));
    ComCfg::FrameContext context;
    this->invoke_to_dataReturnIn(0, buffer, context);
    ASSERT_from_bufferDeallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_bufferDeallocate->at(0).fwBuffer.getData(), data);
}

// ----------------------------------------------------------------------
// Output port handler overrides
// ----------------------------------------------------------------------

Fw::Buffer CfsTlmFramerTester ::from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size)
{
    const FwSizeType allocation = this->m_useUndersizedAlloc ? (size - 1) : size;
    FW_ASSERT(allocation <= sizeof(this->m_allocStorage), static_cast<FwAssertArgType>(allocation));
    return Fw::Buffer(this->m_allocStorage, allocation);
}

CfsTime CfsTlmFramerTester ::from_cfsTimeConvert_handler(FwIndexType portNum, const Fw::Time &time)
{
    // Mirror the CfsSystemTime conversion: microseconds to 2^-32 second units
    const U32 subseconds = static_cast<U32>((static_cast<U64>(time.getUSeconds()) << 32) / 1000000ULL);
    return CfsTime(time.getSeconds(), subseconds);
}

} // namespace FPrimeCfs
