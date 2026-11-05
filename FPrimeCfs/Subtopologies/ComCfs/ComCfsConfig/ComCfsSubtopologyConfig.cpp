#include "ComCfsSubtopologyConfig.hpp"

namespace ComCfs {
namespace Allocation {
// This instance can be changed to use a different allocator in the ComCfs Subtopology
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
namespace BridgeConfig {
// Placeholder default; projects override this configuration module to supply
// this application's own command message ID APID for uplink subscription
const ComCfg::Apid::T uplinkApid = ComCfg::Apid::FW_PACKET_COMMAND;
}  // namespace BridgeConfig
}  // namespace ComCfs
