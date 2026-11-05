#ifndef COMCFSSUBTOPOLOGY_CONFIG_HPP
#define COMCFSSUBTOPOLOGY_CONFIG_HPP

#include "Fw/Types/MallocAllocator.hpp"
#include "config/ApidEnumAc.hpp"

namespace ComCfs {
namespace Allocation {
extern Fw::MemAllocator& memAllocator;
}
namespace BridgeConfig {
//! The APID whose cFS command messages the bridge subscribes to for uplink
//! (this application's own command message ID). Projects override this module
//! to set the APID and add any further subscriptions to the configure phase.
extern const ComCfg::Apid::T uplinkApid;
}  // namespace BridgeConfig
}  // namespace ComCfs

#endif
