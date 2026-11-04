// ======================================================================
// \title  CfsRouterTester.hpp
// \brief  hpp file for CfsRouter component test harness implementation class
// ======================================================================

#ifndef FPrimeCfs_CfsRouterTester_HPP
#define FPrimeCfs_CfsRouterTester_HPP

#include "CfsRouterGTestBase.hpp"
#include "FPrimeCfs/CfsRouter/CfsRouter.hpp"

namespace FPrimeCfs
{

class CfsRouterTester final : public CfsRouterGTestBase
{
public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Construct object CfsRouterTester. When connectOutputs is false, no
    //! route output ports are connected (dataReturnOut is always connected).
    explicit CfsRouterTester(bool connectOutputs = true);

    //! Destroy object CfsRouterTester
    ~CfsRouterTester();

    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! F Prime commands are copied to the configured command port and the buffer returned
    void testRouteFprimeCommand();
    void testBadChecksum();
    void testNoRouteFunctionCode();

    //! The cFS command secondary header is excluded from copied F Prime command data
    void testRouteFprimeCommandSecHdr();

    //! Oversized F Prime command data emits a serialization error and returns the buffer
    void testRouteFprimeCommandTooLarge();

    //! cFS commands route with function code and payload; ownership returns via bufferReturnIn
    void testRouteCfsCommand();

    //! cFS telemetry routes with parsed time and payload; ownership returns via bufferReturnIn
    void testRouteCfsTelemetry();

    //! File packets route to the file output; ownership returns via fileBufferReturnIn
    void testRouteFile();

    //! Unconfigured APIDs route to the unknown output with their context
    void testRouteUnknown();

    //! Buffers returned via dataReturnIn are forwarded with their original context
    void testDataReturnIn();

    //! Untracked buffers returned via dataReturnIn are forwarded with the supplied context
    void testDataReturnInUntracked();

    //! cFS routes without a secondary header go to unknown with a warning event
    void testMissingSecondaryHeader();

    //! cFS routes whose data is too small for the secondary header go to unknown with a warning
    void testShortSecondaryHeader();

    //! Disconnected route outputs return the buffer rather than assert or leak
    void testDisconnectedOutputs();

    //! cmdResponseIn is accepted as a no-op
    void testCommandResponseNoop();

    //! Randomized routing across all categories returns every buffer
    void testRandomized();

private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Invoke dataIn with the given apid/secondary-header flag over the supplied bytes
    void sendData(ComCfg::Apid::T apid, bool hasSecHdr, U8 *bytes, FwSizeType size);
    bool m_corruptChecksum = false;

    void connectPorts();   //!< Connects all ports for the component under test (auto-generated)
    void initComponents(); //!< Initializes the component under test (auto-generated)

    //! The component under test
    CfsRouter component;
};

} // namespace FPrimeCfs

#endif
