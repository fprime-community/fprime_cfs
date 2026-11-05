// ======================================================================
// \title  SchAppDriver.hpp
// \brief  hpp file for SchAppDriver component implementation class
// ======================================================================

#ifndef FPrimeCfs_SchAppDriver_HPP
#define FPrimeCfs_SchAppDriver_HPP

#include "FPrimeCfs/SchAppDriver/SchAppDriverComponentAc.hpp"

namespace FPrimeCfs {

class SchAppDriver final : public SchAppDriverComponentBase {
  public:
    //! The default expected function code of scheduler messages (cFS scheduler
    //! wakeup messages carry function code 0)
    static const U8 DEFAULT_EXPECTED_FUNCTION_CODE = 0;

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SchAppDriver object
    SchAppDriver(const char* const compName  //!< The component name
    );

    //! Destroy SchAppDriver object
    ~SchAppDriver();

    //! Configure the expected function code of scheduler messages
    void configure(U8 expectedFunctionCode  //!< The function code that valid scheduler messages carry
    );

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for cfsCommandIn
    //!
    //! Port receiving routed cFS scheduler command messages. Emits one tick
    //! on CycleOut for messages with the expected function code, and returns
    //! the buffer via bufferReturnOut.
    void cfsCommandIn_handler(FwIndexType portNum,       //!< The port number
                              const ComCfg::Apid& apid,  //!< The APID of the message
                              U8 functionCode,      //!< Function code from the cFS command secondary header
                              Fw::Buffer& data      //!< Command payload
                              ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The function code that valid scheduler messages carry
    U8 m_expectedFunctionCode;
};

}  // namespace FPrimeCfs

#endif
