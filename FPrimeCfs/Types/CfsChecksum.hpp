// ======================================================================
// \title  CfsChecksum.hpp
// \brief  cFS (CFE_MSG) command checksum helpers shared by the framing and routing stages
// ======================================================================
#ifndef FPrimeCfs_CfsChecksum_HPP
#define FPrimeCfs_CfsChecksum_HPP

#include "Fw/FPrimeBasicTypes.hpp"
#include "config/FrameContextSerializableAc.hpp"

namespace FPrimeCfs
{

//! cFS command checksum per CFE_MSG: the XOR of every byte of the complete space packet, seeded
//! with 0xFF, is zero for a valid command
class CfsChecksum final
{
  public:
    //! XOR seed of the cFS command checksum
    static constexpr U8 SEED = 0xFF;

    //! XOR `bytes` into `seed`
    static U8 xorBytes(U8 seed, const U8 *bytes, FwSizeType size);

    //! Checksum of a command packet whose primary header is predicted from the frame context
    //! (command type, secondary header present, APID, sequence flags and count from the context,
    //! length token `size - 1`) over `bytes`: the secondary header followed by the payload.
    //! With the checksum byte zeroed this is the value to write; with it present this is the
    //! residual, which is zero when the packet is valid.
    static U8 compute(const ComCfg::FrameContext &context, const U8 *bytes, FwSizeType size);
};

} // namespace FPrimeCfs

#endif
