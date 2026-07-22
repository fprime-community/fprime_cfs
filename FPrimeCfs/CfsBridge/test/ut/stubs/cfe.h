/**
 * @file
 *   Minimal cFE stub header for CfsBridge unit testing.
 *
 *   Provides only the types, constants, and function prototypes used by the
 *   CfsBridge component. The functions are implemented by CfeStubs.cpp which
 *   records calls and allows tests to inject return values and messages.
 */
#ifndef FPRIME_CFS_UT_STUB_CFE_H
#define FPRIME_CFS_UT_STUB_CFE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int32_t int32;

typedef int32 CFE_Status_t;

#define CFE_SUCCESS ((CFE_Status_t)0)
#define CFE_SB_NO_MESSAGE ((CFE_Status_t)0xca000005)
#define CFE_SB_PIPE_CR_ERR ((CFE_Status_t)0xca000003)
#define CFE_SB_MAX_MSGS_MET ((CFE_Status_t)0xca000004)
#define CFE_SB_BAD_ARGUMENT ((CFE_Status_t)0xca000001)
#define CFE_SB_PIPE_RD_ERR ((CFE_Status_t)0xca000006)

#define CFE_SB_POLL (0)

typedef uint32 CFE_SB_PipeId_t;
typedef uint32 CFE_SB_MsgId_Atom_t;
typedef size_t CFE_MSG_Size_t;

typedef struct {
    CFE_SB_MsgId_Atom_t Value;
} CFE_SB_MsgId_t;

#include "cfe_msg_hdr.h"

typedef union {
    CFE_MSG_Message_t Msg;
    uint8 Bytes[65536];
} CFE_SB_Buffer_t;

/* Message ID <-> value conversions */
static inline CFE_SB_MsgId_t CFE_SB_ValueToMsgId(CFE_SB_MsgId_Atom_t value) {
    CFE_SB_MsgId_t msgId;
    msgId.Value = value;
    return msgId;
}

static inline CFE_SB_MsgId_Atom_t CFE_SB_MsgIdToValue(CFE_SB_MsgId_t msgId) {
    return msgId.Value;
}

/* Software bus API (implemented by CfeStubs.cpp) */
CFE_Status_t CFE_SB_CreatePipe(CFE_SB_PipeId_t* pipeIdPtr, uint16 depth, const char* pipeName);
CFE_Status_t CFE_SB_Subscribe(CFE_SB_MsgId_t msgId, CFE_SB_PipeId_t pipeId);
CFE_Status_t CFE_SB_ReceiveBuffer(CFE_SB_Buffer_t** bufPtr, CFE_SB_PipeId_t pipeId, int32 timeOut);
CFE_Status_t CFE_SB_TransmitMsg(const CFE_MSG_Message_t* msgPtr, bool incrementSequenceCount);
void* CFE_SB_GetUserData(CFE_MSG_Message_t* msgPtr);
size_t CFE_SB_GetUserDataLength(const CFE_MSG_Message_t* msgPtr);

/* Message API (implemented by CfeStubs.cpp) */
CFE_Status_t CFE_MSG_Init(CFE_MSG_Message_t* msgPtr, CFE_SB_MsgId_t msgId, CFE_MSG_Size_t size);
CFE_Status_t CFE_MSG_GetMsgId(const CFE_MSG_Message_t* msgPtr, CFE_SB_MsgId_t* msgId);

#if defined(__cplusplus)
}
#endif

#endif
