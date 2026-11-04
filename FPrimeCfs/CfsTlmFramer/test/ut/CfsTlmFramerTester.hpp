// ======================================================================
// \title  CfsTlmFramerTester.hpp
// \brief  hpp file for CfsTlmFramer component test harness implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsTlmFramerTester_HPP
#define FPrimeCfs_CfsTlmFramerTester_HPP

#include "CfsTlmFramerGTestBase.hpp"
#include "FPrimeCfs/CfsTlmFramer/CfsTlmFramer.hpp"

namespace FPrimeCfs
{

class CfsTlmFramerTester final : public CfsTlmFramerGTestBase
{
public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Construct object CfsTlmFramerTester
    CfsTlmFramerTester();

    //! Destroy object CfsTlmFramerTester
    ~CfsTlmFramerTester();

    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! An event packet's time tag is extracted and placed in the secondary header
    void testLogPacketFraming();

    //! A telemetry channel packet's first time tag is extracted
    void testTlmPacketFraming();

    //! A packetized telemetry packet's time tag is extracted
    void testPacketizedTlmFraming();

    //! An unknown packet type falls back to the current time with a warning
    void testUnknownPacketFallback();

    //! An allocation that is too small drops the packet and returns the data
    void testAllocationFailure();
    void testRawPayloadCurrentTime();

    //! comStatusIn is passed through to comStatusOut
    void testComStatusPassthrough();

    //! dataReturnIn deallocates the wrapped buffer
    void testDataReturnPassthrough();

private:
    // ----------------------------------------------------------------------
    // Output port handler overrides
    // ----------------------------------------------------------------------

    //! Handler for bufferAllocate
    Fw::Buffer from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size) override;

    //! Handler for cfsTimeConvert
    CfsTime from_cfsTimeConvert_handler(FwIndexType portNum, const Fw::Time &time) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Serialize a packet (descriptor, id, time, payload byte) into m_packetStorage
    //! and run it through the framer, verifying the secondary header time
    void checkFraming(const FwPacketDescriptorType descriptor, const Fw::Time &time);

    void connectPorts();   //!< Connects all ports for the component under test (auto-generated)
    void initComponents(); //!< Initializes the component under test (auto-generated)

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    CfsTlmFramer component;

    //! Backing storage returned by the bufferAllocate handler
    U8 m_allocStorage[1024] = {};

    //! Storage for serialized test packets
    U8 m_packetStorage[128] = {};

    //! Whether to force an undersized allocation
    bool m_useUndersizedAlloc = false;
};

} // namespace FPrimeCfs

#endif
