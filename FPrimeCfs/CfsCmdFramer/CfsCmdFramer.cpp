// ======================================================================
// \title  CfsCmdFramer.cpp
// \brief  cpp file for CfsCmdFramer component implementation class
// ======================================================================

#include "FPrimeCfs/CfsCmdFramer/CfsCmdFramer.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/SpacePacketHeaderSerializableAc.hpp"
#include <cstring>
#include <limits>

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CfsCmdFramer ::CfsCmdFramer(const char *const compName) : CfsCmdFramerComponentBase(compName) {}

CfsCmdFramer ::~CfsCmdFramer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CfsCmdFramer ::dataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    const FwSizeType wrappedSize = data.getSize() + CFS_CMD_FRAMER_SEC_HDR_SIZE;
    FW_ASSERT(data.getSize() <= std::numeric_limits<Fw::Buffer::SizeType>::max() - CFS_CMD_FRAMER_SEC_HDR_SIZE,
              static_cast<FwAssertArgType>(data.getSize()));

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

    // cFS command secondary header: function code (from the frame context), then checksum
    wrapped.getData()[0] = context.get_functionCode();
    wrapped.getData()[1] = this->computeChecksum(data, context);
    if (data.getSize() > 0)
    {
        (void)std::memcpy(&wrapped.getData()[CFS_CMD_FRAMER_SEC_HDR_SIZE], data.getData(), data.getSize());
    }
    wrapped.setSize(static_cast<Fw::Buffer::SizeType>(wrappedSize));

    ComCfg::FrameContext wrappedContext = context;
    wrappedContext.set_hasSecHdr(true);
    wrappedContext.set_pktType(ComCfg::SppPacketType::SPP_COMMAND);
    this->dataOut_out(0, wrapped, wrappedContext);
    this->dataReturnOut_out(0, data, context); // return ownership of the original data buffer
}

void CfsCmdFramer ::comStatusIn_handler(FwIndexType portNum, Fw::Success &condition)
{
    if (this->isConnected_comStatusOut_OutputPort(portNum))
    {
        this->comStatusOut_out(portNum, condition);
    }
}

void CfsCmdFramer ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &frameBuffer,
                                         const ComCfg::FrameContext &context)
{
    // dataReturnIn is the allocated buffer coming back from the dataOut port
    this->bufferDeallocate_out(0, frameBuffer);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

U8 CfsCmdFramer ::computeChecksum(const Fw::Buffer &data, const ComCfg::FrameContext &context) const
{
    // The cFS checksum convention (CFE_MSG): the XOR of every byte of the
    // complete packet, seeded with 0xFF, must equal zero. The primary header is
    // written downstream, so its expected contents are predicted here: PVN 0,
    // packet type 1 (command), secondary header flag set, the APID and sequence
    // count from the frame context, sequence flags 0b11 (unsegmented), and the
    // standard length field (total bytes after the primary header, minus one)
    const FwSizeType packetDataLength     = data.getSize() + CFS_CMD_FRAMER_SEC_HDR_SIZE - 1;
    const U16        packetIdentification = static_cast<U16>(
        (static_cast<U16>(context.get_apid()) & Svc::Ccsds::SpacePacketSubfields::ApidMask) |
        (1 << Svc::Ccsds::SpacePacketSubfields::SecHdrOffset) | (1 << Svc::Ccsds::SpacePacketSubfields::PktTypeOffset));
    const U16 packetSequenceControl =
        static_cast<U16>((0x3 << Svc::Ccsds::SpacePacketSubfields::SeqFlagsOffset) |
                         (context.get_sequenceCount() & Svc::Ccsds::SpacePacketSubfields::SeqCountMask));
    const U8 header[CFS_CMD_FRAMER_SPACE_PACKET_HEADER_SIZE] = {
        static_cast<U8>(packetIdentification >> 8),      static_cast<U8>(packetIdentification & 0xFF),
        static_cast<U8>(packetSequenceControl >> 8),     static_cast<U8>(packetSequenceControl & 0xFF),
        static_cast<U8>((packetDataLength >> 8) & 0xFF), static_cast<U8>(packetDataLength & 0xFF)};

    U8 checksum = 0xFF;
    for (FwSizeType i = 0; i < CFS_CMD_FRAMER_SPACE_PACKET_HEADER_SIZE; i++)
    {
        checksum ^= header[i];
    }
    checksum ^= context.get_functionCode();
    for (FwSizeType i = 0; i < data.getSize(); i++)
    {
        checksum ^= data.getData()[i];
    }
    return checksum;
}

} // namespace FPrimeCfs
