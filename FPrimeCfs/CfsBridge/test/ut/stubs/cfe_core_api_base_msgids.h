/**
 * @file
 *   Minimal cFE base message ID stub header for CfsBridge unit testing.
 *   Values match the cFE defaults in default_cfe_core_api_base_msgid_values.h.
 */
#ifndef FPRIME_CFS_UT_STUB_CFE_CORE_API_BASE_MSGIDS_H
#define FPRIME_CFS_UT_STUB_CFE_CORE_API_BASE_MSGIDS_H

#define CFE_PLATFORM_CMD_MID_BASE 0x1800
#define CFE_PLATFORM_TLM_MID_BASE 0x0800

#define CFE_PLATFORM_CMD_TOPICID_TO_MIDV(topic) (CFE_PLATFORM_CMD_MID_BASE | (topic))
#define CFE_PLATFORM_TLM_TOPICID_TO_MIDV(topic) (CFE_PLATFORM_TLM_MID_BASE | (topic))

#endif
