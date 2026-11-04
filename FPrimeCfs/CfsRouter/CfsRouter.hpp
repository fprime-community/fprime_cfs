// ======================================================================
// \title  CfsRouter.hpp
// \brief  hpp file for CfsRouter component implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsRouter_HPP
#define FPrimeCfs_CfsRouter_HPP

#include "FPrimeCfs/CfsRouter/CfsRouterComponentAc.hpp"
#include "FPrimeCfs/CfsRouter/CfsRouter_CfsRouteTableArrayAc.hpp"
#include "CfsRouterConfig/FppConstantsAc.hpp"
#include "Fw/DataStructures/ArrayMap.hpp"
#include "Os/Mutex.hpp"

namespace FPrimeCfs
{

//! Size in bytes of the cFS command secondary header ({U8 FunctionCode, U8 Checksum})
constexpr FwSizeType CFS_ROUTER_CMD_SEC_HDR_SIZE = 2;

//! cFE telemetry secondary header: 6-byte time plus the 4 spare bytes of CFE_MSG_TelemetryHeader_t
constexpr FwSizeType CFS_ROUTER_TLM_SEC_HDR_SIZE = 10;

class CfsRouter final : public CfsRouterComponentBase
{
public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CfsRouter object
    CfsRouter(const char *const compName //!< The component name
    );

    //! Destroy CfsRouter object
    ~CfsRouter();

    //! Compute the cFS command checksum residual over a deframed command (zero when valid)
    static U8 computeChecksumResidual(const Fw::Buffer &data, const ComCfg::FrameContext &context);

private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    void dataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context) override;

    //! Handler implementation for cmdResponseIn (no-op)
    void cmdResponseIn_handler(FwIndexType portNum, FwOpcodeType opCode, U32 cmdSeq,
                               const Fw::CmdResponse &response) override;

    //! Handler implementation for bufferReturnIn
    void bufferReturnIn_handler(FwIndexType portNum, Fw::Buffer &fwBuffer) override;

    //! Handler implementation for dataReturnIn
    void dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context) override;

    //! Handler implementation for fileBufferReturnIn
    void fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer &fwBuffer) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Look up the routing table entry for an APID (and function code for commands); nullptr if none
    const CfsRouteEntry *findRoute(ComCfg::Apid::T apid, bool isCommand, U8 functionCode) const;

    //! Return the incoming buffer to the sender with the context it was received with
    void returnData(Fw::Buffer &data, const ComCfg::FrameContext &context);

    //! Record of an outstanding pass-through transfer: the buffer originally received
    //! on dataIn and the context it was received with, keyed by the outgoing payload pointer
    struct PendingReturn
    {
        Fw::Buffer           original; //!< Buffer as received on dataIn
        ComCfg::FrameContext context;  //!< Context the buffer was received with
    };

    //! Track an outgoing pass-through buffer so the original buffer and context can be
    //! restored on return. On failure the buffer must not be routed.
    Fw::Success trackPending(const Fw::Buffer &outgoing, const Fw::Buffer &original,
                             const ComCfg::FrameContext &context);

    //! Restore the original buffer and context for a returned buffer and complete the
    //! return to the deframer; untracked buffers are returned as-is with the fallback context
    void restoreAndReturn(Fw::Buffer &returned, const ComCfg::FrameContext &fallbackContext);

    //! Route an F Prime command packet (copy)
    void routeFprimeCommand(const CfsRouteEntry &route, Fw::Buffer &data, const ComCfg::FrameContext &context);

    //! Route a cFS command packet (ownership transfer)
    void routeCfsCommand(const CfsRouteEntry &route, Fw::Buffer &data, const ComCfg::FrameContext &context);

    //! Route a cFS telemetry packet (ownership transfer)
    void routeCfsTelemetry(const CfsRouteEntry &route, Fw::Buffer &data, const ComCfg::FrameContext &context);

    //! Route a file packet (ownership transfer)
    void routeFile(Fw::Buffer &data, const ComCfg::FrameContext &context);

    //! Route a packet to the unknown output (ownership transfer)
    void routeUnknown(Fw::Buffer &data, const ComCfg::FrameContext &context);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The APID routing table, statically configured via CfsRouterCfg.fpp
    const CfsRouter_CfsRouteTable m_routes;

    //! Map of outstanding pass-through payload pointers to the original buffer and
    //! context, used to return the original buffer and context on dataReturnOut
    Fw::ArrayMap<const U8 *, PendingReturn, CFS_ROUTER_MAX_PENDING_BUFFERS> m_pending;

    //! Protects m_pending; held only around map operations, never across port invocations
    Os::Mutex m_pendingLock;
};

} // namespace FPrimeCfs

#endif
