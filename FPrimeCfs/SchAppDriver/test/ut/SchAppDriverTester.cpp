// ======================================================================
// \title  SchAppDriverTester.cpp
// \author mstarch
// \brief  cpp file for SchAppDriver component test harness implementation class
// ======================================================================

#include "SchAppDriverTester.hpp"
#include "STest/Pick/Pick.hpp"

namespace FPrimeCfs {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SchAppDriverTester ::SchAppDriverTester()
    : SchAppDriverGTestBase("SchAppDriverTester", SchAppDriverTester::MAX_HISTORY_SIZE),
      component("SchAppDriver"),
      m_expectedFunctionCode(static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF))),
      m_messageData() {
    this->initComponents();
    this->connectPorts();
    this->component.configure(this->m_expectedFunctionCode);
}

SchAppDriverTester ::~SchAppDriverTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SchAppDriverTester ::testSingleTick() {
    Fw::Buffer buffer;
    this->sendSchMessage(buffer, this->m_expectedFunctionCode);
    this->assertSingleTickAndReturn(buffer);
}

void SchAppDriverTester ::testMultipleTicks() {
    const U32 iterations = STest::Pick::lowerUpper(2, 25);
    for (U32 i = 0; i < iterations; i++) {
        Fw::Buffer buffer;
        this->sendSchMessage(buffer, this->m_expectedFunctionCode);
        this->assertSingleTickAndReturn(buffer);
    }
}

void SchAppDriverTester ::testUnexpectedFunctionCode() {
    U8 badFunctionCode = this->m_expectedFunctionCode;
    while (badFunctionCode == this->m_expectedFunctionCode) {
        badFunctionCode = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    Fw::Buffer buffer;
    this->sendSchMessage(buffer, badFunctionCode);
    ASSERT_from_CycleOut_SIZE(0);
    ASSERT_EVENTS_UnexpectedMessage_SIZE(1);
    ASSERT_EVENTS_UnexpectedMessage(0, badFunctionCode, static_cast<U32>(buffer.getSize()));
    this->assertBufferReturned(buffer);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void SchAppDriverTester ::sendSchMessage(Fw::Buffer& buffer, U8 functionCode) {
    this->clearHistory();
    const FwSizeType size = STest::Pick::lowerUpper(0, MAX_MESSAGE_SIZE);
    for (FwSizeType i = 0; i < size; i++) {
        this->m_messageData[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    buffer.set(this->m_messageData, size);
    this->invoke_to_cfsCommandIn(0, ComCfg::Apid::FW_PACKET_COMMAND, functionCode, buffer);
}

void SchAppDriverTester ::assertSingleTickAndReturn(const Fw::Buffer& buffer) {
    ASSERT_from_CycleOut_SIZE(1);
    ASSERT_EVENTS_UnexpectedMessage_SIZE(0);
    this->assertBufferReturned(buffer);
}

void SchAppDriverTester ::assertBufferReturned(const Fw::Buffer& buffer) {
    ASSERT_from_bufferReturnOut_SIZE(1);
    const Fw::Buffer& returned = this->fromPortHistory_bufferReturnOut->at(0).fwBuffer;
    ASSERT_EQ(returned.getData(), buffer.getData());
    ASSERT_EQ(returned.getSize(), buffer.getSize());
}

}  // namespace FPrimeCfs
