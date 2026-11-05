// ======================================================================
// \title  CfsAppBridgeTester.cpp
// \brief  cpp file for CfsAppBridge component test harness implementation class
// ======================================================================

#include "CfsAppBridgeTester.hpp"
#include "STest/Random/Random.hpp"

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CfsAppBridgeTester ::CfsAppBridgeTester()
    : CfsAppBridgeGTestBase("CfsAppBridgeTester", CfsAppBridgeTester::MAX_HISTORY_SIZE), component("CfsAppBridge")
{
    this->initComponents();
    this->connectPorts();
}

CfsAppBridgeTester ::~CfsAppBridgeTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void CfsAppBridgeTester ::testCfsCommand()
{
    this->component.configure(ComCfg::Apid::FW_PACKET_COMMAND);

    U8 payload[16];
    for (U32 i = 0; i < sizeof(payload); ++i)
    {
        payload[i] = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));
    }
    Fw::Buffer data(payload, sizeof(payload));
    const U8   functionCode = static_cast<U8>(STest::Random::lowerUpper(0, 0x7F));

    this->invoke_to_cfsCommandIn(0, functionCode, data);

    ASSERT_from_dataOut_SIZE(1);
    const ComCfg::FrameContext &context = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(context.get_apid(), ComCfg::Apid::FW_PACKET_COMMAND);
    ASSERT_EQ(context.get_functionCode(), functionCode);

    // The data is copied; the copy matches and the original is returned
    Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(outBuffer.getSize(), sizeof(payload));
    ASSERT_NE(outBuffer.getData(), payload);
    for (U32 i = 0; i < sizeof(payload); ++i)
    {
        ASSERT_EQ(outBuffer.getData()[i], payload[i]);
    }
    ASSERT_from_bufferReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_bufferReturnOut->at(0).fwBuffer.getData(), payload);
}

void CfsAppBridgeTester ::testCfsData()
{
    U8 payload[24];
    for (U32 i = 0; i < sizeof(payload); ++i)
    {
        payload[i] = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));
    }
    Fw::Buffer           data(payload, sizeof(payload));
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_HAND);
    context.set_functionCode(0x42);

    this->invoke_to_cfsDataIn(0, data, context);

    // The source's context (APID, function code) is forwarded unchanged
    ASSERT_from_dataOut_SIZE(1);
    const ComCfg::FrameContext &outContext = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(outContext.get_apid(), ComCfg::Apid::FW_PACKET_HAND);
    ASSERT_EQ(outContext.get_functionCode(), 0x42);
    Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(outBuffer.getSize(), sizeof(payload));
    ASSERT_NE(outBuffer.getData(), payload);
    for (U32 i = 0; i < sizeof(payload); ++i)
    {
        ASSERT_EQ(outBuffer.getData()[i], payload[i]);
    }
    // The original is returned to the source with its context
    ASSERT_from_cfsDataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_cfsDataReturnOut->at(0).data.getData(), payload);
    ASSERT_EQ(this->fromPortHistory_cfsDataReturnOut->at(0).context.get_apid(), ComCfg::Apid::FW_PACKET_HAND);
    ASSERT_from_bufferReturnOut_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void CfsAppBridgeTester ::testComDescriptorMapping()
{
    const ComCfg::Apid::T expected[] = {ComCfg::Apid::FW_PACKET_TELEM, ComCfg::Apid::FW_PACKET_LOG,
                                        ComCfg::Apid::FW_PACKET_PACKETIZED_TLM};
    for (U32 i = 0; i < FW_NUM_ARRAY_ELEMENTS(expected); i++)
    {
        this->clearHistory();
        Fw::ComBuffer com;
        ASSERT_EQ(com.serializeFrom(static_cast<FwPacketDescriptorType>(expected[i])), Fw::FW_SERIALIZE_OK);
        const U32 value = STest::Random::lowerUpper(0, 0xFFFF);
        ASSERT_EQ(com.serializeFrom(value), Fw::FW_SERIALIZE_OK);

        this->invoke_to_comIn(0, com, 0);

        ASSERT_from_dataOut_SIZE(1);
        ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_apid(), expected[i]);

        // The complete com buffer (descriptor included) is forwarded unchanged
        Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(0).data;
        ASSERT_EQ(outBuffer.getSize(), com.getBuffLength());
        for (FwSizeType j = 0; j < com.getBuffLength(); j++)
        {
            ASSERT_EQ(outBuffer.getData()[j], com.getBuffAddr()[j]);
        }
        ASSERT_EVENTS_UnknownDescriptor_SIZE(0);
    }
}

void CfsAppBridgeTester ::testUnknownDescriptor()
{
    Fw::ComBuffer                com;
    const FwPacketDescriptorType descriptor = 0x1234;
    ASSERT_EQ(com.serializeFrom(descriptor), Fw::FW_SERIALIZE_OK);

    this->invoke_to_comIn(0, com, 0);

    ASSERT_EVENTS_UnknownDescriptor_SIZE(1);
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_apid(), ComCfg::Apid::FW_PACKET_UNKNOWN);
}

void CfsAppBridgeTester ::testAllocationFailure()
{
    U8         payload[16] = {};
    Fw::Buffer data(payload, sizeof(payload));

    this->m_useUndersizedAlloc = true;
    this->invoke_to_cfsCommandIn(0, 0, data);
    this->m_useUndersizedAlloc = false;

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
    // The undersized allocation is deallocated and the original buffer is still returned
    ASSERT_from_bufferDeallocate_SIZE(1);
    ASSERT_from_bufferReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_bufferReturnOut->at(0).fwBuffer.getData(), payload);
}

void CfsAppBridgeTester ::testDataReturn()
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

Fw::Buffer CfsAppBridgeTester ::from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size)
{
    const FwSizeType allocation = this->m_useUndersizedAlloc ? (size - 1) : size;
    FW_ASSERT(allocation <= sizeof(this->m_allocStorage), static_cast<FwAssertArgType>(allocation));
    return Fw::Buffer(this->m_allocStorage, allocation);
}

} // namespace FPrimeCfs
