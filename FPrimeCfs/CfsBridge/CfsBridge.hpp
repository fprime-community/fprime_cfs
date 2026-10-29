// ======================================================================
// \title  CfsBridge.hpp
// \author mstarch
// \brief  hpp file for CfsBridge component implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsBridge_HPP
#define FPrimeCfs_CfsBridge_HPP

#include "FPrimeCfs/CfsBridge/CfsBridgeComponentAc.hpp"

// Some cFE versions (e.g. draco) use compound literals in inline functions,
// which is a GCC extension when compiled as C++
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
extern "C" {
    #include "cfe_sb.h"   // for CFE_SB_MsgId_t
}
#pragma GCC diagnostic pop

namespace FPrimeCfs
{

//! Size in bytes of the CCSDS space packet primary header
constexpr FwSizeType CFS_BRIDGE_SPACE_PACKET_HEADER_SIZE = 6;
//! Mask of the 11 bit APID field within the space packet stream identifier
constexpr U32 CFS_BRIDGE_SPACE_PACKET_APID_MASK = 0x07FF;
//! Mask of the packet type bit (1 = command) within the space packet stream identifier
constexpr U32 CFS_BRIDGE_SPACE_PACKET_TYPE_MASK = 0x1000;
//! Mask of the secondary header flag within the space packet stream identifier
constexpr U32 CFS_BRIDGE_SPACE_PACKET_SEC_HDR_MASK = 0x0800;
//! Byte offset of the big-endian 16-bit packet data length field within the space packet primary header
constexpr FwSizeType CFS_BRIDGE_SPACE_PACKET_LENGTH_OFFSET = 4;
//! Size in bytes of the cFS command secondary header: {U8 FunctionCode, U8 Checksum}
constexpr FwSizeType CFS_BRIDGE_CMD_SEC_HDR_SIZE = 2;
//! Maximum total size in bytes of a wrapped cFS command packet (primary header + secondary header + payload).
//! Sized to bound the component's internal wrap storage: large enough for the largest F Prime uplink command
//! packet, and well under the software bus limit CFE_MISSION_SB_MAX_SB_MSG_SIZE (default 32768).
constexpr FwSizeType CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE = 2048;

class CfsBridge final : public CfsBridgeComponentBase
{
  public:
    //! Type of cFS message to subscribe to
    enum class CfsMessageType {
        COMMAND,   //!< cFS command message: packet type bit and secondary header flag set
        TELEMETRY  //!< cFS telemetry message: secondary header flag set, packet type bit clear
    };

    enum ConfigurationState {
        UNCONFIGURED, //!< The component is not configured and cannot operate
        CONFIGURED,    //!< The component is configured and can operate
        SUBSCRIBED      //!< The component is subscribed to at least one message and can operate
    };

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CfsBridge object
    CfsBridge(const char *const compName //!< The component name
    );

    //! Destroy CfsBridge object
    ~CfsBridge();

    //! Configure the cFS bridge component
    //!
    //! This method configures the cFS bridge component with the specific pipe depth and name. When `paused` is true,
    //! flow control is enabled: deframed messages are held until a comStatusIn success signal is received, and one
    //! message is sent per received signal. When `wrapFprimeCommands` is true, F Prime command space packets received
    //! on `dataIn` (packet type command, no secondary header) are transmitted as valid cFS command packets: the
    //! secondary header flag is set, and a cFS command secondary header (function code
    //! `ComCfg.FprimeCommandFunctionCode` and a valid cFS XOR checksum) is inserted ahead of the payload.
    CFE_Status_t configure(const FwSizeType pipeDepth,
                           const char* pipeName = "CFS_BRIDGE_PIPE",
                           bool paused = false,
                           bool wrapFprimeCommands = false);

    //! Subscribe to a cFS message with the supplied F Prime apid
    //!
    //! Subscribe to the message-bus for messages with the given APID.  If messages are available, they will be
    //! processed in the components processQueue() function and sent out the dataOut port. Up to the configured pipe
    //! depth of messages of this message ID may be queued.
    CFE_Status_t subscribe(const ComCfg::Apid::T apid);

    //! Subscribe to a cFS message with the supplied F Prime apid and per-message-ID limit
    //!
    //! As subscribe(apid), but at most `msgLimit` messages of this message ID may be queued in the pipe at once. A
    //! `msgLimit` of 0 is passed through to cFE, which then rejects every message of this ID with a "Msg Limit Err"
    //! event; do not subscribe (or unsubscribe) to turn a message ID off.
    CFE_Status_t subscribe(const ComCfg::Apid::T apid, const U16 msgLimit);

    //! Subscribe to a cFS command or telemetry message with the supplied F Prime apid
    //!
    //! Subscribe to the message-bus for cFS messages with the given APID. The message type selects how the message
    //! ID is formed: COMMAND sets both the packet type bit and the secondary header flag (e.g. scheduler (SCH)
    //! wakeup messages), while TELEMETRY sets only the secondary header flag (e.g. housekeeping telemetry).
    //! Received messages will be processed in the component's process() function and sent out the dataOut port.
    //! Up to the configured pipe depth of messages of this message ID may be queued.
    CFE_Status_t subscribeCfs(const ComCfg::Apid::T apid, const CfsMessageType type);

    //! Subscribe to a cFS command or telemetry message with the supplied F Prime apid and per-message-ID limit
    //!
    //! As subscribeCfs(apid, type), but at most `msgLimit` messages of this message ID may be queued in the pipe at
    //! once. A `msgLimit` of 0 is passed through to cFE, which then rejects every message of this ID with a
    //! "Msg Limit Err" event; do not subscribe (or unsubscribe) to turn a message ID off.
    CFE_Status_t subscribeCfs(const ComCfg::Apid::T apid, const CfsMessageType type, const U16 msgLimit);

    //! Process messages in the component's queue and the cFS software bus
    //!
    //! Use this method to drain the component's message queue and process a single message. This allows the actual
    //! work of the handler to be performed on a designated thread. It will also poll the cFS software bus for one message.
    //!
    //! Callers should continually call process() until it returns MSG_DISPATCH_EXIT indicating that the program
    //! is exiting.
    Fw::QueuedComponentBase::MsgDispatchStatus process();

private:
    //! Helper to get cFS message ID or default
    CFE_SB_MsgId_t getCfsMessageId(const ComCfg::Apid::T apid);

    //! Helper to subscribe the input pipe to a message ID, allowing up to msgLimit queued messages for it
    CFE_Status_t subscribeMessageId(const CFE_SB_MsgId_t msgId, const U16 msgLimit);

    //! Helper to poll the cFS software bus for a message
    void poll();

    //! Helper to wrap an F Prime command space packet as a cFS command packet and transmit it
    //!
    //! Copies the packet into internal storage, sets the secondary header flag in the primary header, inserts the
    //! 2-byte cFS command secondary header (function code and checksum) ahead of the payload, adjusts the length
    //! field, and computes the checksum per CFE_MSG conventions (XOR of all packet bytes with 0xFF equals zero).
    CFE_Status_t transmitWrappedCommand(const U8* packet, const FwSizeType size);



    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Port to receive data to frame in a cFS message and send to the cFS software bus. The data will be in the F
    //! Prime application layer message buffer.
    void dataIn_handler(FwIndexType portNum, //!< The port number
                        Fw::Buffer &data, const ComCfg::FrameContext &context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port to return deframed cFS messages data and context back to the cFS bridge component once F Prime has
    //! finished thus completing the data ownership transfer back to the cFS bridge component.
    void dataReturnIn_handler(FwIndexType portNum, //!< The port number
                              Fw::Buffer &data, const ComCfg::FrameContext &context) override;

    //! Handler implementation for schedIn
    //!
    //! Drives the bridge from a rate group: drains the message queue and polls the software bus once
    void schedIn_handler(FwIndexType portNum, //!< The port number
                         U32 context) override;

    //! Handler implementation for comStatusIn
    //!
    //! Port to receive com status signals from the cFS bridge component.
    void comStatusIn_handler(FwIndexType portNum, //!< The port number
                             Fw::Success &status) override;

    CFE_SB_PipeId_t m_inputPipe = {};  //!< Software bus pipe for receiving subscribed messages
    U16 m_pipeDepth = 0;  //!< Depth of the input pipe, also the default per-message-ID limit of each subscription

    ConfigurationState m_configurationState = UNCONFIGURED;  //!< Tracks the configuration state of the component to ensure proper ordering of operations
    bool m_prerolled = false;  //!< Initial comStatusOut signal has been sent to enable downstream data flow
    bool m_paused = true;  //!< Awaiting a comStatusIn success before sending the next deframed message
    bool m_flowControlled = false;  //!< When true, deframed messages are gated by comStatusIn signals
    bool m_wrapFprimeCommands = false;  //!< When true, transmit F Prime command packets as cFS command packets
    alignas(CFE_MSG_Message_t) U8 m_wrapStorage[CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE] = {};  //!< Storage for wrapped cFS command packets
};

} // namespace FPrimeCfs

#endif
