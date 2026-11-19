// ======================================================================
// \title  CfsCmdFramer.cpp
// \brief  cpp file for CfsCmdFramer component implementation class
// ======================================================================

#include "FPrimeCfs/CfsCmdFramer/CfsCmdFramer.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "FPrimeCfs/Types/CfsChecksum.hpp"
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

    // cFS command secondary header: function code (from the frame context), then the checksum
    // computed over the predicted primary header and the whole packet with the checksum byte zero
    wrapped.getData()[0] = context.get_functionCode();
    wrapped.getData()[1] = 0;
    if (data.getSize() > 0)
    {
        (void)std::memcpy(&wrapped.getData()[CFS_CMD_FRAMER_SEC_HDR_SIZE], data.getData(), data.getSize());
    }
    wrapped.getData()[1] = CfsChecksum::compute(context, wrapped.getData(), wrappedSize);
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

} // namespace FPrimeCfs
