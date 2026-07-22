// ======================================================================
// \title  CfeStubs.cpp
// \brief  Implementation of the cFE stub layer for CfsBridge unit testing
// ======================================================================
#include "CfeStubs.hpp"

#include <cstring>

namespace CfeStub {

static State s_state;

State& state() {
    return s_state;
}

void reset() {
    (void)std::memset(&s_state, 0, sizeof(s_state));
    s_state.createPipeStatus = CFE_SUCCESS;
    s_state.subscribeStatus = CFE_SUCCESS;
    s_state.transmitStatus = CFE_SUCCESS;
    s_state.msgInitStatus = CFE_SUCCESS;
    s_state.getMsgIdStatus = CFE_SUCCESS;
    s_state.receiveStatus = CFE_SUCCESS;
}

//! Command messages carry the 0x1000 type bit in the default cFE MsgId V1 scheme
static bool isCommandMsgId(CFE_SB_MsgId_Atom_t msgIdValue) {
    return (msgIdValue & 0x1000) != 0;
}

static size_t headerSize(CFE_SB_MsgId_Atom_t msgIdValue) {
    return isCommandMsgId(msgIdValue) ? sizeof(CFE_MSG_CommandHeader_t) : sizeof(CFE_MSG_TelemetryHeader_t);
}

void queueMessage(CFE_SB_MsgId_Atom_t msgIdValue, const uint8* payload, size_t payloadSize) {
    if (s_state.pendingCount >= STUB_MAX_ENTRIES) {
        return;
    }
    unsigned int slot = (s_state.pendingHead + s_state.pendingCount) % STUB_MAX_ENTRIES;
    CFE_SB_Buffer_t& buffer = s_state.pendingBuffers[slot];
    (void)std::memset(&buffer, 0, sizeof(buffer));
    size_t header = headerSize(msgIdValue);
    buffer.Msg.MsgId = msgIdValue;
    buffer.Msg.Size = static_cast<uint32>(header + payloadSize);
    if ((payload != nullptr) && (payloadSize > 0) && (header + payloadSize <= sizeof(buffer.Bytes))) {
        (void)std::memcpy(&buffer.Bytes[header], payload, payloadSize);
    }
    s_state.pendingCount++;
}

}  // namespace CfeStub

// ----------------------------------------------------------------------
// C API implementations
// ----------------------------------------------------------------------

extern "C" {

CFE_Status_t CFE_SB_CreatePipe(CFE_SB_PipeId_t* pipeIdPtr, uint16 depth, const char* pipeName) {
    CfeStub::State& s = CfeStub::s_state;
    if (s.createPipeCount < CfeStub::STUB_MAX_ENTRIES) {
        CfeStub::CreatePipeCall& call = s.createPipeCalls[s.createPipeCount];
        call.depth = depth;
        (void)std::strncpy(call.name, (pipeName != nullptr) ? pipeName : "", sizeof(call.name) - 1);
        call.name[sizeof(call.name) - 1] = '\0';
    }
    s.createPipeCount++;
    if (s.createPipeStatus == CFE_SUCCESS && pipeIdPtr != nullptr) {
        *pipeIdPtr = s.createPipeCount;
    }
    return s.createPipeStatus;
}

CFE_Status_t CFE_SB_Subscribe(CFE_SB_MsgId_t msgId, CFE_SB_PipeId_t pipeId) {
    CfeStub::State& s = CfeStub::s_state;
    if (s.subscribeCount < CfeStub::STUB_MAX_ENTRIES) {
        CfeStub::SubscribeCall& call = s.subscribeCalls[s.subscribeCount];
        call.msgIdValue = msgId.Value;
        call.pipeId = pipeId;
    }
    s.subscribeCount++;
    return s.subscribeStatus;
}

CFE_Status_t CFE_SB_ReceiveBuffer(CFE_SB_Buffer_t** bufPtr, CFE_SB_PipeId_t pipeId, int32 timeOut) {
    (void)pipeId;
    (void)timeOut;
    CfeStub::State& s = CfeStub::s_state;
    s.receiveCount++;
    if (s.receiveStatus != CFE_SUCCESS) {
        return s.receiveStatus;
    }
    if (s.pendingCount == 0) {
        return CFE_SB_NO_MESSAGE;
    }
    if (bufPtr != nullptr) {
        *bufPtr = &s.pendingBuffers[s.pendingHead];
    }
    s.pendingHead = (s.pendingHead + 1) % CfeStub::STUB_MAX_ENTRIES;
    s.pendingCount--;
    return CFE_SUCCESS;
}

CFE_Status_t CFE_SB_TransmitMsg(const CFE_MSG_Message_t* msgPtr, bool incrementSequenceCount) {
    CfeStub::State& s = CfeStub::s_state;
    if ((msgPtr != nullptr) && (s.transmitCount < CfeStub::STUB_MAX_ENTRIES)) {
        CfeStub::TransmitCall& call = s.transmitCalls[s.transmitCount];
        size_t header = CfeStub::headerSize(msgPtr->MsgId);
        call.msgIdValue = msgPtr->MsgId;
        call.totalSize = msgPtr->Size;
        call.payloadSize = (msgPtr->Size > header) ? (msgPtr->Size - header) : 0;
        call.incrementSequenceCount = incrementSequenceCount;
        size_t copySize = (call.payloadSize <= CfeStub::STUB_MAX_PAYLOAD) ? call.payloadSize : CfeStub::STUB_MAX_PAYLOAD;
        (void)std::memcpy(call.payload, reinterpret_cast<const uint8*>(msgPtr) + header, copySize);
    }
    s.transmitCount++;
    return s.transmitStatus;
}

void* CFE_SB_GetUserData(CFE_MSG_Message_t* msgPtr) {
    return reinterpret_cast<uint8*>(msgPtr) + CfeStub::headerSize(msgPtr->MsgId);
}

size_t CFE_SB_GetUserDataLength(const CFE_MSG_Message_t* msgPtr) {
    size_t header = CfeStub::headerSize(msgPtr->MsgId);
    return (msgPtr->Size > header) ? (msgPtr->Size - header) : 0;
}

CFE_Status_t CFE_MSG_Init(CFE_MSG_Message_t* msgPtr, CFE_SB_MsgId_t msgId, CFE_MSG_Size_t size) {
    CfeStub::State& s = CfeStub::s_state;
    if (s.msgInitStatus == CFE_SUCCESS && msgPtr != nullptr) {
        msgPtr->MsgId = msgId.Value;
        msgPtr->Size = static_cast<uint32>(size);
    }
    return s.msgInitStatus;
}

CFE_Status_t CFE_MSG_GetMsgId(const CFE_MSG_Message_t* msgPtr, CFE_SB_MsgId_t* msgId) {
    CfeStub::State& s = CfeStub::s_state;
    if (s.getMsgIdStatus == CFE_SUCCESS && msgPtr != nullptr && msgId != nullptr) {
        msgId->Value = msgPtr->MsgId;
    }
    return s.getMsgIdStatus;
}

}  // extern "C"
