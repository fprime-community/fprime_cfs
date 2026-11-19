// ======================================================================
// \title  CfsChecksum.cpp
// \brief  cFS (CFE_MSG) command checksum helpers shared by the framing and routing stages
// ======================================================================
#include "FPrimeCfs/Types/CfsChecksum.hpp"
#include "Fw/Types/Assert.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"

namespace FPrimeCfs
{

constexpr U8 CfsChecksum::SEED;

U8 CfsChecksum ::xorBytes(U8 seed, const U8 *bytes, FwSizeType size)
{
    FW_ASSERT((bytes != nullptr) || (size == 0), static_cast<FwAssertArgType>(size));
    for (FwSizeType i = 0; i < size; i++)
    {
        seed ^= bytes[i];
    }
    return seed;
}

U8 CfsChecksum ::compute(const ComCfg::FrameContext &context, const U8 *bytes, FwSizeType size)
{
    FW_ASSERT(size >= 1, static_cast<FwAssertArgType>(size));
    // CCSDS space packet primary header of a cFS command: PVN 0, packet type 1 (command),
    // secondary header flag set, 11-bit APID; sequence flags and count; length token
    const U16 packetIdentification = static_cast<U16>(
        (static_cast<U16>(context.get_apid()) & Svc::Ccsds::SpacePacketSubfields::ApidMask) |
        (1 << Svc::Ccsds::SpacePacketSubfields::SecHdrOffset) | (1 << Svc::Ccsds::SpacePacketSubfields::PktTypeOffset));
    const U16 packetSequenceControl = static_cast<U16>(
        ((static_cast<U16>(context.get_sequenceFlags()) << Svc::Ccsds::SpacePacketSubfields::SeqFlagsOffset) &
         Svc::Ccsds::SpacePacketSubfields::SeqFlagsMask) |
        (context.get_sequenceCount() & Svc::Ccsds::SpacePacketSubfields::SeqCountMask));
    const FwSizeType lengthToken = size - 1;
    const U8         header[6]   = {static_cast<U8>(packetIdentification >> 8),
                                    static_cast<U8>(packetIdentification & 0xFF),
                                    static_cast<U8>(packetSequenceControl >> 8),
                                    static_cast<U8>(packetSequenceControl & 0xFF),
                                    static_cast<U8>((lengthToken >> 8) & 0xFF),
                                    static_cast<U8>(lengthToken & 0xFF)};
    return CfsChecksum::xorBytes(CfsChecksum::xorBytes(CfsChecksum::SEED, header, sizeof(header)), bytes, size);
}

} // namespace FPrimeCfs
