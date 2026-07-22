// ======================================================================
// 	itle  PollingTimer.cpp
// \author mstarch
// \brief  cpp file for PollingTimer component implementation class
// ======================================================================

#include "FPrimeCfs/PollingTimer/PollingTimer.hpp"
#include "Os/RawTime.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PollingTimer ::PollingTimer(const char* const compName)
    : PollingTimerComponentBase(compName),
      m_interval(Clock::duration::zero()),
      m_nextCycleTime(),
      m_enabled(false) {}

PollingTimer ::~PollingTimer() {}

void PollingTimer ::startTimer(const Fw::TimeInterval& interval) {
    const auto configuredInterval = std::chrono::seconds(interval.getSeconds()) +
                                    std::chrono::microseconds(interval.getUSeconds());
    this->m_interval = std::chrono::duration_cast<Clock::duration>(configuredInterval);

    if (this->m_interval <= Clock::duration::zero()) {
        this->stop();
        return;
    }

    this->m_enabled = true;
    this->m_nextCycleTime = Clock::now() + this->m_interval;
}

void PollingTimer ::cycle() {
    if (this->m_enabled) {
        const Clock::time_point now = Clock::now();

        if (now >= this->m_nextCycleTime) {
            Os::RawTime timestamp;
            timestamp.now();
            this->CycleOut_out(0, timestamp);

            Clock::time_point nextCycleTime = this->m_nextCycleTime + this->m_interval;
            if (nextCycleTime <= now) {
                nextCycleTime = now + this->m_interval;
            }
            this->m_nextCycleTime = nextCycleTime;
        }
    }
}

void PollingTimer ::stop() {
    this->m_enabled = false;
    this->m_interval = Clock::duration::zero();
    this->m_nextCycleTime = Clock::time_point{};
}

}  // namespace Svc
