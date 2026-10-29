// ======================================================================
// \title  CfsBridgeTester.hpp
// \brief  hpp file for CfsBridge component test harness implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsBridgeTester_HPP
#define FPrimeCfs_CfsBridgeTester_HPP

#include "CfsBridgeGTestBase.hpp"
#include "FPrimeCfs/CfsBridge/CfsBridge.hpp"
#include "CfeStubs.hpp"

namespace FPrimeCfs {

class CfsBridgeTester final : public CfsBridgeGTestBase {
  public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 100;

    // Queue depth supplied to the component instance under test
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Construct object CfsBridgeTester
    CfsBridgeTester();

    //! Destroy object CfsBridgeTester
    ~CfsBridgeTester();

    // ----------------------------------------------------------------------
    // Tests: configure/subscribe
    // ----------------------------------------------------------------------

    //! Configure creates the software bus pipe with the supplied settings
    void testConfigure();

    //! Configure returns the software bus error on pipe creation failure
    void testConfigureFailure();

    //! Subscribe maps APIDs to the expected command/telemetry message ids
    void testSubscribe();

    //! Configure rejects pipe depths that would truncate in cFE's uint16 depth
    void testConfigureDepthTooLarge();

    //! SubscribeCfs maps APIDs to cFS command/telemetry message ids (secondary header flag set)
    void testSubscribeCfs();

    //! Subscribe returns the software bus error on subscription failure
    void testSubscribeFailure();

    //! Subscriptions allow as many queued messages per message ID as the configured pipe depth
    void testSubscribeMessageLimit();

    //! Subscriptions with an explicit message limit pass exactly that limit, including 0
    void testSubscribeExplicitMessageLimit(const U16 limit);

    // ----------------------------------------------------------------------
    // Tests: transmit (dataIn -> software bus)
    // ----------------------------------------------------------------------

    //! A single complete space packet is transmitted as one software bus message
    void testTransmitSinglePacket();

    //! Multiple concatenated space packets are each transmitted as their own message
    void testTransmitMultiplePackets();

    //! Buffer ownership is returned and com status emitted on transmit failure
    void testTransmitFailure();

    //! A packet whose length field exceeds the available data is dropped
    void testTransmitTruncated();

    //! Residual bytes too small to form a primary header are dropped
    void testTransmitResidual();

    //! With wrapping enabled, F Prime command packets are transmitted as valid cFS command packets
    void testTransmitWrappedCommand();

    //! With wrapping enabled, telemetry and secondary-header command packets pass through unmodified
    void testTransmitWrapPassthrough();

    //! With wrapping enabled, command packets too large to wrap are dropped
    void testTransmitWrapTooLarge();

    //! Test wrapping a command packet whose wrapped size exactly fills the wrap storage
    void testTransmitWrapExactBoundary();

    //! Test wrapping the first packet of a multi-packet buffer without disturbing the following packet
    void testTransmitWrapMultiplePackets();

    //! Test software bus transmit failure on the wrapped command path
    void testTransmitWrapFailure();

    // ----------------------------------------------------------------------
    // Tests: receive (software bus -> dataOut)
    // ----------------------------------------------------------------------

    //! Received software bus messages are sent whole out dataOut with a default context
    void testReceive();

    //! process() emits a single preroll com status once subscribed
    void testPreroll();

    //! Empty software bus polls produce no output
    void testReceiveNoMessage();

    //! Received messages are dropped when buffer allocation fails
    void testReceiveAllocationFailure();

    //! Software bus receive errors produce no output
    void testReceiveError();

    //! Messages whose size cannot be read are dropped
    void testReceiveGetSizeFailure();

    //! Flow control gates deframed messages on comStatusIn signals
    void testFlowControl();

    //! dataReturnIn accepts returned buffers without action
    void testDataReturn();

    //! schedIn drives one process() pass: drains the queue and polls the software bus
    void testSchedIn();

    //! Randomized sequence of uplink/downlink/flow-control operations
    void testRandomized();

  private:
    //! Allocate from the tester's static storage, recording the call in the port history
    Fw::Buffer from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size) override;

    //! Storage backing from_bufferAllocate_handler allocations
    U8 m_allocStorage[1024];

    //! When true, from_bufferAllocate_handler returns an empty (failed) allocation
    bool m_failAllocation = false;
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Configure and subscribe the component to the given apid
    void configureAndSubscribe(ComCfg::Apid::T apid, bool paused = false);

    //! Build a CCSDS space packet with the given stream identifier and payload; returns the packet size
    FwSizeType makePacket(U8* dest, U16 streamIdValue, const U8* payload, FwSizeType payloadSize);

    //! Fill a byte array with random data
    void fillRandom(U8* data, FwSizeType size);

    //! Send a buffer through dataIn (dispatching the queued message) and assert
    //! buffer return and com status emission
    void sendDataIn(Fw::Buffer& buffer, const ComCfg::FrameContext& context);

    //! Queue a software bus message and poll it through via process()
    void receiveMessage(ComCfg::Apid::T apid, const U8* payload, FwSizeType size);

    //! Assert that a transmit call matches the given expectations
    void assertTransmitted(U32 index,
                           CFE_SB_MsgId_Atom_t expectedMsgId,
                           const U8* expectedPayload,
                           FwSizeType expectedPayloadSize);

    void connectPorts();    //!< Connects all ports for the component under test (auto-generated)
    void initComponents();  //!< Initializes the component under test (auto-generated)

    //! The component under test
    CfsBridge component;
};

}  // namespace FPrimeCfs

#endif
