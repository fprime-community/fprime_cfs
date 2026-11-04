// ======================================================================
// \title  CfsRouter.cpp
// \brief  cpp file for CfsRouter component implementation class
// ======================================================================

#include "FPrimeCfs/CfsRouter/CfsRouter.hpp"
#include "Fw/FPrimeBasicTypes.hpp"

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CfsRouter ::CfsRouter(const char *const compName) : CfsRouterComponentBase(compName) {}

CfsRouter ::~CfsRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CfsRouter ::dataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    const ComCfg::Apid::T apid         = context.get_apid();
    const bool            isCommand    = (context.get_pktType() == ComCfg::SppPacketType::SPP_COMMAND);
    U8                    functionCode = 0;
    if (isCommand)
    {
        // Command packets carry the cFS command secondary header (function code, checksum);
        // the checksum is verified before any routing decision is made
        if ((!context.get_hasSecHdr()) || (data.getSize() < CFS_ROUTER_CMD_SEC_HDR_SIZE))
        {
            this->log_WARNING_HI_MissingSecondaryHeader(static_cast<U16>(apid), static_cast<U32>(data.getSize()));
            this->routeUnknown(data, context);
            return;
        }
        functionCode      = data.getData()[0];
        const U8 residual = CfsRouter::computeChecksumResidual(data, context);
        if (residual != 0)
        {
            // Failed checksum: drop the command and return the buffer to the sender
            this->log_WARNING_HI_BadChecksum(static_cast<U16>(apid), functionCode, residual);
            this->returnData(data, context);
            return;
        }
    }
    const CfsRouteEntry *route = this->findRoute(apid, isCommand, functionCode);
    if (route == nullptr)
    {
        this->log_WARNING_LO_NoRoute(static_cast<U16>(apid), functionCode);
        this->routeUnknown(data, context);
        return;
    }
    switch (route->get_routeType())
    {
        case CfsRouteType::FPRIME_COMMAND:
            this->routeFprimeCommand(*route, data, context);
            break;
        case CfsRouteType::CFS_COMMAND:
            this->routeCfsCommand(*route, data, context);
            break;
        case CfsRouteType::CFS_TELEMETRY:
            this->routeCfsTelemetry(*route, data, context);
            break;
        case CfsRouteType::FILE:
            this->routeFile(data, context);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(route->get_routeType()));
            break;
    }
}

void CfsRouter ::cmdResponseIn_handler(FwIndexType portNum, FwOpcodeType opCode, U32 cmdSeq,
                                       const Fw::CmdResponse &response)
{
    // Nothing to do
}

void CfsRouter ::bufferReturnIn_handler(FwIndexType portNum, Fw::Buffer &fwBuffer)
{
    // Ownership of a buffer sent on cfsCommandOut, cfsTelemetryOut, or unknownDataOut
    // has been returned; complete the transfer back to the deframer with the original
    // buffer and the context the data was received with
    const ComCfg::FrameContext defaultContext;
    this->restoreAndReturn(fwBuffer, defaultContext);
}

void CfsRouter ::fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer &fwBuffer)
{
    this->bufferReturnIn_handler(portNum, fwBuffer);
}

void CfsRouter ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    // The buffer is returned with the context it was originally received with,
    // restored from the pending-buffer map; the supplied context is the fallback
    // when the buffer is not tracked
    this->restoreAndReturn(data, context);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

const CfsRouteEntry *CfsRouter ::findRoute(ComCfg::Apid::T apid, bool isCommand, U8 functionCode) const
{
    // Command routes match on (APID, function code); telemetry routes match on APID for
    // telemetry packets only; file routes match on APID regardless of packet type
    for (FwSizeType i = 0; i < CfsRouter_CfsRouteTable::SIZE; i++)
    {
        const CfsRouteEntry &entry = this->m_routes[i];
        if (entry.get_apid() != apid)
        {
            continue;
        }
        switch (entry.get_routeType())
        {
            case CfsRouteType::FPRIME_COMMAND:
            case CfsRouteType::CFS_COMMAND:
                if (isCommand && (entry.get_functionCode() == functionCode))
                {
                    return &entry;
                }
                break;
            case CfsRouteType::CFS_TELEMETRY:
                if (!isCommand)
                {
                    return &entry;
                }
                break;
            case CfsRouteType::FILE:
                return &entry;
            default:
                FW_ASSERT(0, static_cast<FwAssertArgType>(entry.get_routeType()));
                break;
        }
    }
    return nullptr;
}

U8 CfsRouter ::computeChecksumResidual(const Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    // Per CFE_MSG conventions: XOR of every space packet byte with 0xFF equals zero when valid.
    // The primary header was consumed upstream; reconstruct it (cFS command packet) from the context.
    const U16        apid          = static_cast<U16>(context.get_apid());
    const U16        sequenceCount = context.get_sequenceCount();
    const FwSizeType lengthToken   = data.getSize() - 1;
    U8               residual      = 0xFF;
    residual ^= static_cast<U8>(0x18 | ((apid >> 8) & 0x07));
    residual ^= static_cast<U8>(apid & 0xFF);
    residual ^= static_cast<U8>(0xC0 | ((sequenceCount >> 8) & 0x3F));
    residual ^= static_cast<U8>(sequenceCount & 0xFF);
    residual ^= static_cast<U8>((lengthToken >> 8) & 0xFF);
    residual ^= static_cast<U8>(lengthToken & 0xFF);
    const U8 *const bytes = data.getData();
    for (FwSizeType i = 0; i < data.getSize(); i++)
    {
        residual ^= bytes[i];
    }
    return residual;
}

void CfsRouter ::returnData(Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    this->dataReturnOut_out(0, data, context);
}

Fw::Success CfsRouter ::trackPending(const Fw::Buffer &outgoing, const Fw::Buffer &original,
                                     const ComCfg::FrameContext &context)
{
    PendingReturn pending;
    pending.original = original;
    pending.context  = context;
    this->m_pendingLock.lock();
    const Fw::Success status = this->m_pending.insert(outgoing.getData(), pending);
    this->m_pendingLock.unlock();
    if (status != Fw::Success::SUCCESS)
    {
        this->log_WARNING_HI_TooManyPendingBuffers(static_cast<U16>(context.get_apid()));
    }
    return status;
}

void CfsRouter ::restoreAndReturn(Fw::Buffer &returned, const ComCfg::FrameContext &fallbackContext)
{
    PendingReturn pending;
    pending.original = returned;
    pending.context  = fallbackContext;
    this->m_pendingLock.lock();
    (void)this->m_pending.remove(returned.getData(), pending);
    this->m_pendingLock.unlock();
    this->returnData(pending.original, pending.context);
}

void CfsRouter ::routeFprimeCommand(const CfsRouteEntry &route, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    // A cFS-framed F Prime command carries the cFS command secondary header ahead of
    // the F Prime command data; exclude it from the copy when present
    const FwSizeType  offset = context.get_hasSecHdr() ? CFS_ROUTER_CMD_SEC_HDR_SIZE : 0;
    const FwIndexType index  = route.get_portIndex();
    if ((data.getSize() >= offset) && this->isConnected_commandOut_OutputPort(index))
    {
        Fw::ComBuffer             com;
        const Fw::SerializeStatus status = com.setBuff(data.getData() + offset, data.getSize() - offset);
        if (status == Fw::FW_SERIALIZE_OK)
        {
            this->commandOut_out(index, com, 0);
        }
        else
        {
            this->log_WARNING_HI_SerializationError(static_cast<U32>(status));
        }
    }
    else if (data.getSize() < offset)
    {
        this->log_WARNING_HI_MissingSecondaryHeader(static_cast<U16>(context.get_apid()),
                                                    static_cast<U32>(data.getSize()));
    }
    // The F Prime command route copies: ownership returns to the sender immediately
    this->returnData(data, context);
}

void CfsRouter ::routeCfsCommand(const CfsRouteEntry &route, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    if ((!context.get_hasSecHdr()) || (data.getSize() < CFS_ROUTER_CMD_SEC_HDR_SIZE))
    {
        this->log_WARNING_HI_MissingSecondaryHeader(static_cast<U16>(context.get_apid()),
                                                    static_cast<U32>(data.getSize()));
        this->routeUnknown(data, context);
        return;
    }
    const FwIndexType index = route.get_portIndex();
    if (!this->isConnected_cfsCommandOut_OutputPort(index))
    {
        this->returnData(data, context);
        return;
    }
    const U8 functionCode = data.getData()[0];
    // Payload is the data after the secondary header; ownership transfers to the receiver
    // and returns via bufferReturnIn
    Fw::Buffer payload(data.getData() + CFS_ROUTER_CMD_SEC_HDR_SIZE, data.getSize() - CFS_ROUTER_CMD_SEC_HDR_SIZE);
    const Fw::Success trackStatus = this->trackPending(payload, data, context);
    if (trackStatus == Fw::Success::SUCCESS)
    {
        this->cfsCommandOut_out(index, functionCode, payload);
    }
    else
    {
        this->returnData(data, context);
    }
}

void CfsRouter ::routeCfsTelemetry(const CfsRouteEntry &route, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    if ((!context.get_hasSecHdr()) || (data.getSize() < CFS_ROUTER_TLM_SEC_HDR_SIZE))
    {
        this->log_WARNING_HI_MissingSecondaryHeader(static_cast<U16>(context.get_apid()),
                                                    static_cast<U32>(data.getSize()));
        this->routeUnknown(data, context);
        return;
    }
    const FwIndexType index = route.get_portIndex();
    if (!this->isConnected_cfsTelemetryOut_OutputPort(index))
    {
        this->returnData(data, context);
        return;
    }
    // The cFS telemetry secondary header is big-endian: 4-byte seconds, 2-byte subseconds.
    // The 16-bit subseconds field holds the most significant 16 bits of the 2^-32 value.
    const U8 *const bytes   = data.getData();
    const U32       seconds = (static_cast<U32>(bytes[0]) << 24) | (static_cast<U32>(bytes[1]) << 16) |
                        (static_cast<U32>(bytes[2]) << 8) | static_cast<U32>(bytes[3]);
    const U32     subseconds = ((static_cast<U32>(bytes[4]) << 8) | static_cast<U32>(bytes[5])) << 16;
    const CfsTime time(seconds, subseconds);
    // Payload is the data after the secondary header; ownership transfers to the receiver
    // and returns via bufferReturnIn
    Fw::Buffer payload(data.getData() + CFS_ROUTER_TLM_SEC_HDR_SIZE, data.getSize() - CFS_ROUTER_TLM_SEC_HDR_SIZE);
    const Fw::Success trackStatus = this->trackPending(payload, data, context);
    if (trackStatus == Fw::Success::SUCCESS)
    {
        this->cfsTelemetryOut_out(index, time, payload);
    }
    else
    {
        this->returnData(data, context);
    }
}

void CfsRouter ::routeFile(Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    if (!this->isConnected_fileOut_OutputPort(0))
    {
        this->returnData(data, context);
        return;
    }
    const Fw::Success trackStatus = this->trackPending(data, data, context);
    if (trackStatus == Fw::Success::SUCCESS)
    {
        // Ownership transfers to the receiver and returns via fileBufferReturnIn
        this->fileOut_out(0, data);
    }
    else
    {
        this->returnData(data, context);
    }
}

void CfsRouter ::routeUnknown(Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    if (!this->isConnected_unknownDataOut_OutputPort(0))
    {
        this->returnData(data, context);
        return;
    }
    const Fw::Success trackStatus = this->trackPending(data, data, context);
    if (trackStatus == Fw::Success::SUCCESS)
    {
        // Ownership transfers to the receiver and returns via bufferReturnIn
        this->unknownDataOut_out(0, data, context);
    }
    else
    {
        this->returnData(data, context);
    }
}

} // namespace FPrimeCfs
