// ======================================================================
// \title  CfeStubs.hpp
// \brief  Test control interface for the cFE stub layer
//
// Tests use this interface to inject return statuses, queue messages for
// receipt, and inspect calls the component under test made into cFE.
// ======================================================================
#ifndef FPRIME_CFS_UT_CFE_STUBS_HPP
#define FPRIME_CFS_UT_CFE_STUBS_HPP

#include "cfe.h"

namespace CfeStub {

//! Maximum recorded/queued entries in the stub
static const unsigned int STUB_MAX_ENTRIES = 32;
//! Maximum payload bytes captured per transmitted message
static const unsigned int STUB_MAX_PAYLOAD = 4096;

//! Record of a CFE_SB_CreatePipe call
struct CreatePipeCall {
    uint16 depth;
    char name[64];
};

//! Record of a CFE_SB_Subscribe call
struct SubscribeCall {
    CFE_SB_MsgId_Atom_t msgIdValue;
    CFE_SB_PipeId_t pipeId;
};

//! Record of a CFE_SB_TransmitMsg call
struct TransmitCall {
    CFE_SB_MsgId_Atom_t msgIdValue;
    size_t totalSize;
    size_t payloadSize;
    bool incrementSequenceCount;
    uint8 payload[STUB_MAX_PAYLOAD];
};

//! Shared state of the cFE stub layer
struct State {
    // Injectable return statuses
    CFE_Status_t createPipeStatus;
    CFE_Status_t subscribeStatus;
    CFE_Status_t transmitStatus;
    CFE_Status_t msgInitStatus;
    CFE_Status_t getMsgIdStatus;
    CFE_Status_t receiveStatus;  //!< Returned by CFE_SB_ReceiveBuffer when not CFE_SUCCESS

    // Call records
    unsigned int createPipeCount;
    CreatePipeCall createPipeCalls[STUB_MAX_ENTRIES];
    unsigned int subscribeCount;
    SubscribeCall subscribeCalls[STUB_MAX_ENTRIES];
    unsigned int transmitCount;
    TransmitCall transmitCalls[STUB_MAX_ENTRIES];
    unsigned int receiveCount;

    // Queue of buffers to hand out on CFE_SB_ReceiveBuffer
    unsigned int pendingCount;
    unsigned int pendingHead;
    CFE_SB_Buffer_t pendingBuffers[STUB_MAX_ENTRIES];
};

//! Get the stub state singleton
State& state();

//! Reset the stub state to defaults (all statuses CFE_SUCCESS, no records)
void reset();

//! Queue a message for receipt: builds a stub message with the given message
//! id value and payload placed after the appropriate (command/telemetry) header
void queueMessage(CFE_SB_MsgId_Atom_t msgIdValue, const uint8* payload, size_t payloadSize);

}  // namespace CfeStub

#endif
