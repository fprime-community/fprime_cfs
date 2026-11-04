// ======================================================================
// \title  CfsAppBridge.cpp
// \brief  cpp file for CfsAppBridge component implementation class
// ======================================================================

#include "FPrimeCfs/CfsAppBridge/CfsAppBridge.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include <cstring>

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CfsAppBridge ::CfsAppBridge(const char *const compName) : CfsAppBridgeComponentBase(compName) {}

CfsAppBridge ::~CfsAppBridge() {}

void CfsAppBridge ::configure(const ComCfg::Apid::T commandApid)
{
    this->m_commandApid = commandApid;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CfsAppBridge ::cfsCommandIn_handler(FwIndexType portNum, U8 functionCode, Fw::Buffer &data)
{
    ComCfg::FrameContext context;
    context.set_apid(this->m_commandApid);
    context.set_functionCode(functionCode);
    this->copyAndSend(data.getData(), data.getSize(), context);
    // The incoming buffer was copied; return it to its sender
    this->bufferReturnOut_out(0, data);
}

void CfsAppBridge ::cfsDataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    // The source owns the frame context (APID, function code); the framers below add the
    // packet type and the cFS headers
    this->copyAndSend(data.getData(), data.getSize(), context);
    // The incoming buffer was copied; return it to its sender
    this->cfsDataReturnOut_out(0, data, context);
}

void CfsAppBridge ::comIn_handler(FwIndexType portNum, Fw::ComBuffer &data, U32 context)
{
    ComCfg::FrameContext frameContext;
    frameContext.set_apid(this->mapDescriptorToApid(data));
    this->copyAndSend(data.getBuffAddr(), data.getSize(), frameContext);
}

void CfsAppBridge ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    // Buffers sent on dataOut were allocated by this component
    this->bufferDeallocate_out(0, data);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void CfsAppBridge ::copyAndSend(const U8 *const bytes, const FwSizeType size, const ComCfg::FrameContext &context)
{
    Fw::Buffer outgoing = this->bufferAllocate_out(0, static_cast<Fw::Buffer::SizeType>(size));
    if (outgoing.getSize() < size)
    {
        this->log_WARNING_HI_AllocationFailed(static_cast<U32>(size));
        if (outgoing.getSize() > 0)
        {
            this->bufferDeallocate_out(0, outgoing);
        }
        return;
    }
    if (size > 0)
    {
        (void)std::memcpy(outgoing.getData(), bytes, size);
    }
    outgoing.setSize(static_cast<Fw::Buffer::SizeType>(size));
    this->dataOut_out(0, outgoing, context);
}

ComCfg::Apid::T CfsAppBridge ::mapDescriptorToApid(Fw::ComBuffer &data)
{
    FwPacketDescriptorType descriptor = 0;
    data.resetDeser();
    const Fw::SerializeStatus status = data.deserializeTo(descriptor);
    if (status == Fw::FW_SERIALIZE_OK)
    {
        // F Prime packet descriptors coincide with the FW_PACKET_* APID values
        switch (static_cast<ComCfg::Apid::T>(descriptor))
        {
            case ComCfg::Apid::FW_PACKET_COMMAND:
            case ComCfg::Apid::FW_PACKET_TELEM:
            case ComCfg::Apid::FW_PACKET_LOG:
            case ComCfg::Apid::FW_PACKET_FILE:
            case ComCfg::Apid::FW_PACKET_PACKETIZED_TLM:
            case ComCfg::Apid::FW_PACKET_DP:
            case ComCfg::Apid::FW_PACKET_IDLE:
            case ComCfg::Apid::FW_PACKET_HAND:
                return static_cast<ComCfg::Apid::T>(descriptor);
            default:
                break;
        }
    }
    this->log_WARNING_LO_UnknownDescriptor(static_cast<U16>(descriptor));
    return ComCfg::Apid::FW_PACKET_UNKNOWN;
}

} // namespace FPrimeCfs
