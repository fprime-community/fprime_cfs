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
    s_state.getSizeStatus = CFE_SUCCESS;
    s_state.receiveStatus = CFE_SUCCESS;
}

//! Read the big-endian stream identifier from a CCSDS primary header
static CFE_SB_MsgId_Atom_t streamId(const CFE_MSG_Message_t* msgPtr) {
    return static_cast<CFE_SB_MsgId_Atom_t>((msgPtr->Pri.StreamId[0] << 8) | msgPtr->Pri.StreamId[1]);
}

//! Compute the total packet size from the CCSDS primary header length field
static size_t packetSize(const CFE_MSG_Message_t* msgPtr) {
    return static_cast<size_t>(((msgPtr->Pri.Length[0] << 8) | msgPtr->Pri.Length[1]) + 1 +
                               sizeof(CCSDS_PrimaryHeader_t));
}

void queueMessage(CFE_SB_MsgId_Atom_t msgIdValue, const uint8* payload, size_t payloadSize) {
    if (s_state.pendingCount >= STUB_MAX_ENTRIES) {
        return;
    }
    unsigned int slot = (s_state.pendingHead + s_state.pendingCount) % STUB_MAX_ENTRIES;
    CFE_SB_Buffer_t& buffer = s_state.pendingBuffers[slot];
    (void)std::memset(&buffer, 0, sizeof(buffer));
    buffer.Msg.Pri.StreamId[0] = static_cast<uint8>((msgIdValue >> 8) & 0xFF);
    buffer.Msg.Pri.StreamId[1] = static_cast<uint8>(msgIdValue & 0xFF);
    buffer.Msg.Pri.Sequence[0] = 0xC0;  // Sequence flags: unsegmented user data
    buffer.Msg.Pri.Sequence[1] = 0x00;
    size_t lengthToken = (payloadSize > 0) ? (payloadSize - 1) : 0;
    buffer.Msg.Pri.Length[0] = static_cast<uint8>((lengthToken >> 8) & 0xFF);
    buffer.Msg.Pri.Length[1] = static_cast<uint8>(lengthToken & 0xFF);
    size_t header = sizeof(CCSDS_PrimaryHeader_t);
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

CFE_Status_t CFE_SB_SubscribeEx(CFE_SB_MsgId_t msgId, CFE_SB_PipeId_t pipeId, CFE_SB_Qos_t quality, uint16 msgLim) {
    CfeStub::State& s = CfeStub::s_state;
    if (s.subscribeCount < CfeStub::STUB_MAX_ENTRIES) {
        CfeStub::SubscribeCall& call = s.subscribeCalls[s.subscribeCount];
        call.msgIdValue = msgId.Value;
        call.pipeId = pipeId;
        call.quality = quality;
        call.msgLim = msgLim;
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
        size_t header = sizeof(CCSDS_PrimaryHeader_t);
        size_t total = CfeStub::packetSize(msgPtr);
        call.msgIdValue = CfeStub::streamId(msgPtr);
        call.totalSize = total;
        call.payloadSize = (total > header) ? (total - header) : 0;
        call.incrementSequenceCount = incrementSequenceCount;
        size_t copySize = (call.payloadSize <= CfeStub::STUB_MAX_PAYLOAD) ? call.payloadSize : CfeStub::STUB_MAX_PAYLOAD;
        (void)std::memcpy(call.payload, reinterpret_cast<const uint8*>(msgPtr) + header, copySize);
    }
    s.transmitCount++;
    return s.transmitStatus;
}

CFE_Status_t CFE_MSG_GetSize(const CFE_MSG_Message_t* msgPtr, CFE_MSG_Size_t* size) {
    CfeStub::State& s = CfeStub::s_state;
    if (s.getSizeStatus == CFE_SUCCESS && msgPtr != nullptr && size != nullptr) {
        *size = CfeStub::packetSize(msgPtr);
    }
    return s.getSizeStatus;
}

}  // extern "C"
