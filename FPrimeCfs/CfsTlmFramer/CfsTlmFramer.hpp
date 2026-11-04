// ======================================================================
// \title  CfsTlmFramer.hpp
// \brief  hpp file for CfsTlmFramer component implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsTlmFramer_HPP
#define FPrimeCfs_CfsTlmFramer_HPP

#include "FPrimeCfs/CfsTlmFramer/CfsTlmFramerComponentAc.hpp"

namespace FPrimeCfs
{

//! Size in bytes of the cFS telemetry secondary header (4-byte seconds, 2-byte subseconds)
//! cFE telemetry secondary header: 6-byte time plus the 4 spare bytes of CFE_MSG_TelemetryHeader_t
constexpr FwSizeType CFS_TLM_FRAMER_TIME_SIZE    = 6;
constexpr FwSizeType CFS_TLM_FRAMER_SEC_HDR_SIZE = 10;

class CfsTlmFramer final : public CfsTlmFramerComponentBase
{
public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CfsTlmFramer object
    CfsTlmFramer(const char *const compName //!< The component name
    );

    //! Destroy CfsTlmFramer object
    ~CfsTlmFramer();

private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    void dataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context) override;

    //! Handler implementation for dataReturnIn
    void dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &frameBuffer,
                              const ComCfg::FrameContext &context) override;

    //! Handler implementation for comStatusIn
    void comStatusIn_handler(FwIndexType portNum, Fw::Success &condition) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Extract the time tag from an incoming F Prime packet (event, telemetry
    //! channel, or packetized telemetry). Falls back to the current system time
    //! (with a warning event) when a time tag cannot be extracted.
    Fw::Time extractTime(Fw::Buffer &data);

    //! Whether the context describes an F Prime packet (one that carries its own time tag)
    static bool isFprimePacket(const ComCfg::FrameContext &context);

    //! Convert an F Prime time to cFS system time via the cfsTimeConvert port,
    //! or directly from seconds and microseconds when the port is unconnected
    CfsTime convertTime(const Fw::Time &time);
};

} // namespace FPrimeCfs

#endif
