// ======================================================================
// \title  CfsBridge.cpp
// \author mstarch
// \brief  cpp file for CfsBridge component implementation class
// ======================================================================

#include "FPrimeCfs/CfsBridge/CfsBridge.hpp"
#include "FPrimeCfs/CfsBridge/cfs_bridge_msgstruct.h"
#include "Fw/Logger/Logger.hpp"
#include "config/TransmissionTypeEnumAc.hpp"
#include <cstring>
#include <limits>

// Some cFE versions (e.g. draco) use compound literals in inline functions,
// which is a GCC extension when compiled as C++
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
extern "C" {
    #include "cfe.h"
    #include "cfe_config.h"
    #include "cfs_bridge_msgstruct.h"
    #include "cfe_core_api_base_msgids.h"
    #include "cfe_sb.h"   // for CFE_SB_TransmitMsg
}
#pragma GCC diagnostic pop

/*
 * Older cFE versions (e.g. draco) do not provide the topic ID to MID value
 * mapping macros; derive them from the platform MID base values instead.
 */
#ifndef CFE_PLATFORM_CMD_TOPICID_TO_MIDV
#define CFE_PLATFORM_CMD_TOPICID_TO_MIDV(topic) (CFE_PLATFORM_CMD_MID_BASE | (topic))
#endif
#ifndef CFE_PLATFORM_TLM_TOPICID_TO_MIDV
#define CFE_PLATFORM_TLM_TOPICID_TO_MIDV(topic) (CFE_PLATFORM_TLM_MID_BASE | (topic))
#endif

namespace FPrimeCfs
{

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CfsBridge ::CfsBridge(const char *const compName) : CfsBridgeComponentBase(compName) {}

CfsBridge ::~CfsBridge() {}

CFE_Status_t CfsBridge ::configure(const FwSizeType pipeDepth, const char* pipeName, const bool paused) {
    CFE_Status_t status = CFE_SB_CreatePipe(&this->inputPipe, pipeDepth, pipeName);
    if (status == CFE_SUCCESS) {
        this->m_configurationState = CONFIGURED;
    }
    this->m_flow = paused;
    return status;
}

Fw::QueuedComponentBase::MsgDispatchStatus CfsBridge ::process() {
    // First drain the component's message queue. This ensures messages have been returned etc
    Fw::QueuedComponentBase::MsgDispatchStatus status = this->dispatchCurrentMessages();
    // Next, poll as long as we are not in an exit condition
    if (status != Fw::QueuedComponentBase::MSG_DISPATCH_EXIT && this->m_configurationState == SUBSCRIBED) {
        // Preroll the com pipeline to allow downlink
        if (not this->m_prerolled and this->isConnected_comStatusOut_OutputPort(0)) {
            Fw::Success status = Fw::Success::SUCCESS;
            this->comStatusOut_out(0, status);
            this->m_prerolled = true;
        }

        this->poll();
    }
    return status;
}

CFE_Status_t CfsBridge ::subscribe(const ComCfg::Apid::T apid) {
    FW_ASSERT(this->m_configurationState != UNCONFIGURED);
    CFE_SB_MsgId_t msgId = this->getCfsMessageId(apid);
    CFE_Status_t status = CFE_SB_Subscribe(msgId, this->inputPipe);
    if (status == CFE_SUCCESS) {
        this->m_configurationState = SUBSCRIBED;
        Fw::Logger::log("[INFO] Successfully subscribed to message ID: 0x%08x\n", msgId.Value);
    }
    return status;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

CFE_SB_MsgId_t CfsBridge ::getCfsMessageId(const ComCfg::Apid::T apid) {
    // First convert the topic (APID) to a message ID value
    U32 message_id = 0;
    switch (apid) {
        // Uplink entities are "commands"
        case ComCfg::Apid::FW_PACKET_COMMAND:
            message_id = CFE_PLATFORM_CMD_TOPICID_TO_MIDV(apid);
            break;
        // Everything else is "telemetry"
        default:
            message_id = CFE_PLATFORM_TLM_TOPICID_TO_MIDV(apid);
            break;
    }
    // Then conver the message ID value to a CFE_SB_MsgId_t
    return CFE_SB_ValueToMsgId(message_id);
}

void CfsBridge ::poll() {
    if (this->m_flow and this->m_paused) {
        return;
    }

    CFE_SB_Buffer_t* buffer = nullptr;
    CFE_Status_t status = CFE_SB_ReceiveBuffer(&buffer, this->inputPipe, CFE_SB_POLL);
    if (status == CFE_SUCCESS) {
        Fw::Logger::log("[DEBUG] Received message!\n");
        CFE_MSG_Message_t* received_message = &buffer->Msg;


        U8* payload = static_cast<U8*>(CFE_SB_GetUserData(received_message));
        FwSizeType payload_length = CFE_SB_GetUserDataLength(received_message);

        Fw::Buffer fwBuffer(payload, payload_length);
        ComCfg::FrameContext context;
        CFE_SB_MsgId_t message_id;
        status = CFE_MSG_GetMsgId(received_message, &message_id);
        if (status != CFE_SUCCESS) {
            Fw::Logger::log("[ERROR] Failed to get message ID from received message: 0x%08x\n", status);
            return;
        }
        // Convert out the message ID and then the topic (APID) from the message
        // WARNING: this is not "allowed" by cFS but because there is no inverse macro for CFE_PLATFORM_CMD_TOPICID_TO_MIDV
        //     we have not a lot of choice here.
        CFE_SB_MsgId_Atom_t message_id_value = CFE_SB_MsgIdToValue(message_id);
        ComCfg::Apid apid = static_cast<ComCfg::Apid::T>(message_id_value & 0x7FF);
        FW_ASSERT(CFE_SB_MsgIdToValue(message_id) == CFE_SB_MsgIdToValue(this->getCfsMessageId(apid.e)));
        if (apid.isValid() and (not this->m_flow or not this->m_paused)) {
            context.set_apid(apid);
            this->m_paused = true;
            // Send the message out of this port
            this->dataOut_out(0, fwBuffer, context);
        } else if (not apid.isValid()) {
            Fw::Logger::log("[ERROR] Received message with invalid APID: 0x%08x\n", message_id_value);
            return;
        } else if (this->m_flow and this->m_paused) {
            Fw::Logger::log("[INFO] Received message with APID: 0x%08x but currently paused. Message will not be sent out.\n", apid.e);
        }
    } else if (status != CFE_SB_NO_MESSAGE) {
        Fw::Logger::log("[ERROR] Error receiving message from cFS pipe: 0x%08x\n", status);
    }
}

void CfsBridge ::dataIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    FPRIME_FprimeMessage_t message;
    ComCfg::Apid::T apid = context.get_apid();
    size_t header_size = 0;
    CFE_MSG_Message_t* message_pointer = nullptr;

    Fw::Success comStatus = Fw::Success::SUCCESS; // Always return success as SB doesn't need retries

    switch (apid) {
        // Uplink entities are "commands"
        case ComCfg::Apid::FW_PACKET_COMMAND:
            header_size = sizeof(CFE_MSG_CommandHeader_t);
            message_pointer = reinterpret_cast<CFE_MSG_Message_t*>(&message.command);
            break;
        // Everything else is "telemetry"
        default:
            header_size = sizeof(CFE_MSG_TelemetryHeader_t);
            message_pointer = reinterpret_cast<CFE_MSG_Message_t*>(&message.telemetry);
            break;
    }

    // First, check for overflows before attempting to creat a cFS message that is too-large
    if (std::numeric_limits<CFE_MSG_Size_t>::max() - header_size < data.getSize()) {
        this->dataReturnOut_out(0, data, context);
        if (this->isConnected_comStatusOut_OutputPort(0)) {
            this->comStatusOut_out(0, comStatus);
        }
        return;
    }
    CFE_MSG_Size_t message_size = header_size + data.getSize();
    CFE_SB_MsgId_t message_id = this->getCfsMessageId(apid);

    // Initialize the cFS message with the appropriate header and size
    CFE_Status_t status = CFE_MSG_Init(message_pointer,  message_id, message_size);
    if (status != CFE_SUCCESS)
    {
        this->dataReturnOut_out(0, data, context);
        if (this->isConnected_comStatusOut_OutputPort(0)) {
            this->comStatusOut_out(0, comStatus);
        }
        return;
    }
    // Copy the message into the buffer for transmission
    std::memcpy(reinterpret_cast<U8*>(message_pointer) + header_size, data.getData(), data.getSize());

    status = CFE_SB_TransmitMsg(reinterpret_cast<CFE_MSG_Message_t*>(&message), this->m_source);
    if (status != CFE_SUCCESS)
    {
        this->dataReturnOut_out(0, data, context);
        if (this->isConnected_comStatusOut_OutputPort(0)) {
            this->comStatusOut_out(0, comStatus);
        }
        return;
    }
    this->dataReturnOut_out(0, data, context);
    if (this->isConnected_comStatusOut_OutputPort(0)) {
        this->comStatusOut_out(0, comStatus);
    }
    return;
}

void CfsBridge ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer &data, const ComCfg::FrameContext &context)
{
    // cFS does not return messages explicitly
}

void CfsBridge ::comStatusIn_handler(FwIndexType portNum, Fw::Success &status)
{
    this->m_paused = (status == Fw::Success::SUCCESS) ? false : true;
}

} // namespace FPrimeCfs
