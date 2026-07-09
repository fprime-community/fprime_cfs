/**
 * @file
 *
 * Compatibility definitions allowing F Prime cFS modules and apps to build
 * against older cFE versions (e.g. draco) as well as the latest cFS. Every
 * definition here is a no-op when the cFE version already provides it.
 */

#ifndef FPRIME_CFS_COMPATIBILITY_H
#define FPRIME_CFS_COMPATIBILITY_H

#include "cfe_version.h"
#include "cfe_core_api_base_msgids.h"

/*
 * Older cFE versions do not provide the topic ID to MID value mapping macros;
 * derive them from the platform MID base values instead.
 */
#ifndef CFE_PLATFORM_CMD_TOPICID_TO_MIDV
#define CFE_PLATFORM_CMD_TOPICID_TO_MIDV(topic) (CFE_PLATFORM_CMD_MID_BASE | (topic))
#endif
#ifndef CFE_PLATFORM_TLM_TOPICID_TO_MIDV
#define CFE_PLATFORM_TLM_TOPICID_TO_MIDV(topic) (CFE_PLATFORM_TLM_MID_BASE | (topic))
#endif

/*
 * Older cFE versions do not provide CFE_Config_GetVersionString; provide an
 * equivalent local implementation when building against them.
 */
#if CFE_MAJOR_VERSION < 7
#include <stdio.h>
static inline void CFE_Config_GetVersionString(char *Buf, size_t Size, const char *Component,
                                               const char *SrcVersion, const char *CodeName,
                                               const char *LastOffcRel)
{
    (void)LastOffcRel;
    snprintf(Buf, Size, "%s %s (%s)", Component, SrcVersion, CodeName);
}
#endif

#endif /* FPRIME_CFS_COMPATIBILITY_H */
