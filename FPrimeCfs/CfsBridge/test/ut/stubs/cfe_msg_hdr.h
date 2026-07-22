/**
 * @file
 *   Minimal cFE message header stub for CfsBridge unit testing.
 *
 *   The layout is not CCSDS binary-accurate; it only needs to be internally
 *   consistent so that header sizes, message IDs, and payload offsets behave
 *   like the real cFE message API from the perspective of the code under test.
 */
#ifndef FPRIME_CFS_UT_STUB_CFE_MSG_HDR_H
#define FPRIME_CFS_UT_STUB_CFE_MSG_HDR_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    uint32_t MsgId;
    uint32_t Size;
} CFE_MSG_Message_t;

typedef struct {
    CFE_MSG_Message_t Msg;
} CFE_MSG_CommandHeader_t;

typedef struct {
    CFE_MSG_Message_t Msg;
    uint32_t Timestamp[2];
} CFE_MSG_TelemetryHeader_t;

#if defined(__cplusplus)
}
#endif

#endif
