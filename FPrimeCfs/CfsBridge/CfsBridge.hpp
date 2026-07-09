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
};
#pragma GCC diagnostic pop

namespace FPrimeCfs
{

class CfsBridge final : public CfsBridgeComponentBase
{
  public:
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
    //! This method configures the cFS bridge component with the specific pipe depth. It allows for starting in a paused
    //! state pending on the receipt of a port call to unpuase.
    CFE_Status_t configure(const FwSizeType pipeDepth, const char* pipeName = "CFS_BRIDGE_PIPE", bool paused = false);

    //! Subscribe to a cFS message with the supplied F Prime apid
    //!
    //! Subscribe to the message-bus for messages with the given APID.  If messages are available, they will be processed
    //! in the components processQueue() function and sent out the dataOut port.
    CFE_Status_t subscribe(const ComCfg::Apid::T apid);

    //! Process messages in the component's queue and the cFS software bus
    //!
    //! Use this method to drain the component's message queue and process a single message. This allows the actual
    //! work of the handler to be performed on a designated thread. It will also poll the cFS softwar bus for one message.
    //!
    //! Callers should continually call process() until it returns MSG_DISPATCH_EXIT indicating that the program
    //! is exiting.
    Fw::QueuedComponentBase::MsgDispatchStatus process();

private:
    //! Helper to get cFS message ID or default
    CFE_SB_MsgId_t getCfsMessageId(const ComCfg::Apid::T apid);

    //! Helper to poll the cFS software bus for a message
    void poll();



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

    //! Handler implementation for comStatusIn
    //!
    //! Port to receive com status signals from the cFS bridge component.
    void comStatusIn_handler(FwIndexType portNum, //!< The port number
                             Fw::Success &status) override;
    CFE_SB_PipeId_t inputPipe;

    ConfigurationState m_configurationState = UNCONFIGURED;  //!< Tracks the configuration state of the component to ensure proper ordering of operations
    U16 m_defaultCommandMessageId = 1;  // TODO: fix these defaults
    U16 m_defaultTelemetryMessageId = 1; // TODO: fix these defaults
    bool m_source = true;
    bool m_prerolled = false;
    bool m_paused = true;
    bool m_flow = false;
};

} // namespace FPrimeCfs

#endif
