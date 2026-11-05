// ======================================================================
// \title  CfsAppBridgeTester.hpp
// \brief  hpp file for CfsAppBridge component test harness implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsAppBridgeTester_HPP
#define FPrimeCfs_CfsAppBridgeTester_HPP

#include "CfsAppBridgeGTestBase.hpp"
#include "FPrimeCfs/CfsAppBridge/CfsAppBridge.hpp"

namespace FPrimeCfs
{

class CfsAppBridgeTester final : public CfsAppBridgeGTestBase
{
public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Construct object CfsAppBridgeTester
    CfsAppBridgeTester();

    //! Destroy object CfsAppBridgeTester
    ~CfsAppBridgeTester();

    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! A cFS command is forwarded with the configured APID and function code,
    //! and the incoming buffer is returned
    void testCfsCommand();

    //! A com buffer's descriptor selects the APID
    void testComDescriptorMapping();
    void testCfsData();

    //! An unknown descriptor maps to the unknown APID with a warning
    void testUnknownDescriptor();

    //! An allocation that is too small drops the data
    void testAllocationFailure();

    //! dataReturnIn deallocates the outgoing buffer
    void testDataReturn();

private:
    // ----------------------------------------------------------------------
    // Output port handler overrides
    // ----------------------------------------------------------------------

    //! Handler for bufferAllocate
    Fw::Buffer from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size) override;

    void connectPorts();   //!< Connects all ports for the component under test (auto-generated)
    void initComponents(); //!< Initializes the component under test (auto-generated)

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    CfsAppBridge component;

    //! Backing storage returned by the bufferAllocate handler
    U8 m_allocStorage[1024] = {};

    //! Whether to force an undersized allocation
    bool m_useUndersizedAlloc = false;
};

} // namespace FPrimeCfs

#endif
