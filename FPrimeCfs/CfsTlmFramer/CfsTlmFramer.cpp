// ======================================================================
// \title  CfsTlmFramer.cpp
// \brief  cpp file for CfsTlmFramer component implementation class
// ======================================================================

#include "FPrimeCfs/CfsTlmFramer/CfsTlmFramer.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include <cstring>
#include <limits>

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CfsTlmFramer ::CfsTlmFramer(const char *const compName) : CfsTlmFramerComponentBase(compName) {}

CfsTlmFramer ::~CfsTlmFramer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CfsTlmFramer ::dataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    const FwSizeType wrappedSize = data.getSize() + CFS_TLM_FRAMER_SEC_HDR_SIZE;
    FW_ASSERT(data.getSize() <= std::numeric_limits<Fw::Buffer::SizeType>::max() - CFS_TLM_FRAMER_SEC_HDR_SIZE,
              static_cast<FwAssertArgType>(data.getSize()));

    // F Prime packets carry their own timestamp; raw cFS payloads are tagged with the current time
    const Fw::Time time    = CfsTlmFramer::isFprimePacket(context) ? this->extractTime(data) : this->getTime();
    const CfsTime  sysTime = this->convertTime(time);

    Fw::Buffer wrapped = this->bufferAllocate_out(0, static_cast<Fw::Buffer::SizeType>(wrappedSize));
    if (wrapped.getSize() < wrappedSize)
    {
        this->log_WARNING_HI_AllocationFailed(static_cast<U32>(wrappedSize));
        if (wrapped.getSize() > 0)
        {
            this->bufferDeallocate_out(0, wrapped);
        }
        this->dataReturnOut_out(0, data, context);
        return;
    }

    // cFS telemetry secondary header, mirroring CFE_MSG_TelemetrySecondaryHeader_t:
    // big-endian 4-byte seconds followed by the most significant 16 bits of the
    // 2^-32 subseconds value
    U8 *const header       = wrapped.getData();
    const U32 seconds      = sysTime.get_seconds();
    const U16 subseconds16 = static_cast<U16>(sysTime.get_subseconds() >> 16);
    header[0]              = static_cast<U8>(seconds >> 24);
    header[1]              = static_cast<U8>((seconds >> 16) & 0xFF);
    header[2]              = static_cast<U8>((seconds >> 8) & 0xFF);
    header[3]              = static_cast<U8>(seconds & 0xFF);
    header[4]              = static_cast<U8>(subseconds16 >> 8);
    header[5]              = static_cast<U8>(subseconds16 & 0xFF);
    for (FwSizeType i = CFS_TLM_FRAMER_TIME_SIZE; i < CFS_TLM_FRAMER_SEC_HDR_SIZE; i++)
    {
        header[i] = 0; // CFE_MSG_TelemetryHeader_t spare bytes
    }
    if (data.getSize() > 0)
    {
        (void)std::memcpy(&header[CFS_TLM_FRAMER_SEC_HDR_SIZE], data.getData(), data.getSize());
    }
    wrapped.setSize(static_cast<Fw::Buffer::SizeType>(wrappedSize));

    ComCfg::FrameContext wrappedContext = context;
    wrappedContext.set_hasSecHdr(true);
    wrappedContext.set_pktType(ComCfg::SppPacketType::SPP_TELEMETRY);
    this->dataOut_out(0, wrapped, wrappedContext);
    this->dataReturnOut_out(0, data, context); // return ownership of the original data buffer
}

void CfsTlmFramer ::comStatusIn_handler(FwIndexType portNum, Fw::Success &condition)
{
    if (this->isConnected_comStatusOut_OutputPort(portNum))
    {
        this->comStatusOut_out(portNum, condition);
    }
}

void CfsTlmFramer ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &frameBuffer,
                                         const ComCfg::FrameContext &context)
{
    // dataReturnIn is the allocated buffer coming back from the dataOut port
    this->bufferDeallocate_out(0, frameBuffer);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

bool CfsTlmFramer ::isFprimePacket(const ComCfg::FrameContext &context)
{
    switch (context.get_apid())
    {
        case ComCfg::Apid::FW_PACKET_LOG:
        case ComCfg::Apid::FW_PACKET_TELEM:
        case ComCfg::Apid::FW_PACKET_PACKETIZED_TLM:
            return true;
        default:
            return false;
    }
}

Fw::Time CfsTlmFramer ::extractTime(Fw::Buffer &data)
{
    // F Prime downlink packets carry a serialized time tag after the packet
    // descriptor and an id whose width depends on the packet type:
    //   FW_PACKET_LOG:            descriptor, FwEventIdType id, Fw::Time, arguments
    //   FW_PACKET_TELEM:          descriptor, then per entry: FwChanIdType id, Fw::Time, value
    //   FW_PACKET_PACKETIZED_TLM: descriptor, FwTlmPacketizeIdType id, Fw::Time, data
    // For telemetry channel packets the time of the first entry is used.
    auto                   deserializer = data.getDeserializer();
    FwPacketDescriptorType descriptor   = static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_UNKNOWN);
    Fw::SerializeStatus    status       = deserializer.deserializeTo(descriptor);
    if (status == Fw::FW_SERIALIZE_OK)
    {
        switch (descriptor)
        {
            case static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_LOG):
            {
                FwEventIdType id = 0;
                status           = deserializer.deserializeTo(id);
                break;
            }
            case static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_TELEM):
            {
                FwChanIdType id = 0;
                status          = deserializer.deserializeTo(id);
                break;
            }
            case static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_PACKETIZED_TLM):
            {
                FwTlmPacketizeIdType id = 0;
                status                  = deserializer.deserializeTo(id);
                break;
            }
            default:
                status = Fw::FW_DESERIALIZE_TYPE_MISMATCH;
                break;
        }
    }
    Fw::Time time;
    if (status == Fw::FW_SERIALIZE_OK)
    {
        status = deserializer.deserializeTo(time);
    }
    if (status != Fw::FW_SERIALIZE_OK)
    {
        this->log_WARNING_LO_TimeExtractionFailed(static_cast<U16>(descriptor));
        time = this->getTime();
    }
    return time;
}

CfsTime CfsTlmFramer ::convertTime(const Fw::Time &time)
{
    if (this->isConnected_cfsTimeConvert_OutputPort(0))
    {
        return this->cfsTimeConvert_out(0, time);
    }
    // Direct conversion: microseconds to 2^-32 second units
    const U32 subseconds = static_cast<U32>((static_cast<U64>(time.getUSeconds()) << 32) / 1000000ULL);
    return CfsTime(time.getSeconds(), subseconds);
}

} // namespace FPrimeCfs
