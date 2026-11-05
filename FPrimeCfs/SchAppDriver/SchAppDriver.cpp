// ======================================================================
// \title  SchAppDriver.cpp
// \brief  cpp file for SchAppDriver component implementation class
// ======================================================================

#include "FPrimeCfs/SchAppDriver/SchAppDriver.hpp"
#include "Os/RawTime.hpp"

namespace FPrimeCfs {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SchAppDriver ::SchAppDriver(const char* const compName)
    : SchAppDriverComponentBase(compName), m_expectedFunctionCode(DEFAULT_EXPECTED_FUNCTION_CODE) {}

SchAppDriver ::~SchAppDriver() {}

void SchAppDriver ::configure(U8 expectedFunctionCode) {
    this->m_expectedFunctionCode = expectedFunctionCode;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SchAppDriver ::cfsCommandIn_handler(FwIndexType portNum,
                                         const ComCfg::Apid& apid,
                                         U8 functionCode,
                                         Fw::Buffer& data) {
    if (functionCode == this->m_expectedFunctionCode) {
        Os::RawTime timestamp;
        timestamp.now();
        this->CycleOut_out(0, timestamp);
    } else {
        this->log_WARNING_HI_UnexpectedMessage(functionCode, static_cast<U32>(data.getSize()));
    }
    this->bufferReturnOut_out(0, data);
}

}  // namespace FPrimeCfs
