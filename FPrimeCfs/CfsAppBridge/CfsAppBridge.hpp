// ======================================================================
// \title  CfsAppBridge.hpp
// \brief  hpp file for CfsAppBridge component implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsAppBridge_HPP
#define FPrimeCfs_CfsAppBridge_HPP

#include "FPrimeCfs/CfsAppBridge/CfsAppBridgeComponentAc.hpp"

namespace FPrimeCfs
{

class CfsAppBridge final : public CfsAppBridgeComponentBase
{
public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CfsAppBridge object
    CfsAppBridge(const char *const compName //!< The component name
    );

    //! Destroy CfsAppBridge object
    ~CfsAppBridge();

private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for cfsCommandIn
    void cfsCommandIn_handler(FwIndexType portNum, const ComCfg::Apid &apid, U8 functionCode,
                              Fw::Buffer &data) override;

    //! Handler implementation for comIn
    void cfsDataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context) override;
    void comIn_handler(FwIndexType portNum, Fw::ComBuffer &data, U32 context) override;

    //! Handler implementation for dataReturnIn
    void dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Copy the given bytes into an allocated buffer and send it on dataOut
    //! with the given context. Emits AllocationFailed and drops the data when
    //! the allocation is undersized.
    void copyAndSend(const U8 *const bytes, const FwSizeType size, const ComCfg::FrameContext &context);

    //! Map a com buffer's leading packet descriptor to an APID. Returns the
    //! unknown APID (with a warning event) when the descriptor cannot be read
    //! or is not a known packet type.
    ComCfg::Apid::T mapDescriptorToApid(Fw::ComBuffer &data);
};

} // namespace FPrimeCfs

#endif
