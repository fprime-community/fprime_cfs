// ======================================================================
// \title  CfsCmdFramer.hpp
// \brief  hpp file for CfsCmdFramer component implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsCmdFramer_HPP
#define FPrimeCfs_CfsCmdFramer_HPP

#include "FPrimeCfs/CfsCmdFramer/CfsCmdFramerComponentAc.hpp"

namespace FPrimeCfs {

//! Size in bytes of the cFS command secondary header ({U8 FunctionCode, U8 Checksum})
constexpr FwSizeType CFS_CMD_FRAMER_SEC_HDR_SIZE = 2;


class CfsCmdFramer final : public CfsCmdFramerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CfsCmdFramer object
    CfsCmdFramer(const char* const compName  //!< The component name
    );

    //! Destroy CfsCmdFramer object
    ~CfsCmdFramer();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    void dataIn_handler(FwIndexType portNum,
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    void dataReturnIn_handler(FwIndexType portNum,
                              Fw::Buffer& frameBuffer,
                              const ComCfg::FrameContext& context) override;

    //! Handler implementation for comStatusIn
    void comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Compute the cFS command checksum over the predicted final space packet:
    //! the primary header the downstream space packet framer is expected to
};

}  // namespace FPrimeCfs

#endif
