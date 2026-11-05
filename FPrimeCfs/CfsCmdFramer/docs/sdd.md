# FPrimeCfs::CfsCmdFramer

A framer stage that wraps outgoing data in a cFS command secondary header (`{U8 FunctionCode, U8 Checksum}`) ahead
of a downstream space packet framer (e.g. `Svc.Ccsds.SpacePacketFramer`). The result, once the downstream framer
prepends the CCSDS primary header, is a valid cFS command packet (`CFE_MSG_CommandHeader_t` layout).

## Usage

```
[data source].dataOut -> CfsCmdFramer.dataIn
CfsCmdFramer.dataOut -> Svc.Ccsds.SpacePacketFramer.dataIn
Svc.Ccsds.SpacePacketFramer.dataReturnOut -> CfsCmdFramer.dataReturnIn
CfsCmdFramer.dataReturnOut -> [data source's return input]
CfsCmdFramer.bufferAllocate / bufferDeallocate -> [buffer manager]
```

The function code is read from the `functionCode` field of the `ComCfg.FrameContext` passed on `dataIn`. This
field is added to `ComCfg.FrameContext` by this library's configuration override
(`config/FPrimeCfsConfig/ComCfg.fpp`); the component sending data into the framer chain sets it per packet.
Projects that override `ComCfg.fpp` themselves must carry the `functionCode` field forward.

The framer sets `hasSecHdr = true` in the context it forwards downstream, so the space packet framer sets the
secondary header flag in the CCSDS primary header.

## Checksum

The checksum byte follows the cFS `CFE_MSG` convention: the XOR of every byte of the complete packet, seeded with
`0xFF`, must equal zero. Since the CCSDS primary header is written downstream, `FPrimeCfs::CfsChecksum`
(`FPrimeCfs/Types`, shared with `CfsRouter`) predicts its contents from the frame context: PVN 0, packet type 1
(command), secondary header flag set, the APID, sequence flags and sequence count from the context, and the standard
length field. The downstream `Svc.Ccsds.SpacePacketFramer` writes exactly these values from the same context fields.

Note: the sequence count in the context is assigned by the downstream `Svc.Ccsds.SpacePacketFramer` after this
stage runs, so the checksum written here is only a placeholder for the final header. `FPrimeCfs::CfsBridge`
transmits native cFS command packets with `CFE_SB_TransmitMsg(msg, true)`, and cFE then stamps the sequence count
and recomputes the checksum over the final header (`CFE_MSG_OriginationAction`). The framer also sets `pktType` to
`SPP_COMMAND` in the forwarded frame context so the space packet framer builds a command packet.

## Buffer ownership

- `dataIn`: a new buffer is allocated via `bufferAllocate` for the wrapped packet; the incoming buffer is returned
  to the sender on `dataReturnOut` immediately with its original context.
- `dataReturnIn`: the wrapped buffer coming back from downstream is deallocated via `bufferDeallocate`.
- If allocation fails (undersized buffer), the packet is dropped with an `AllocationFailed` warning event, and any
  non-empty allocation is deallocated.

`comStatusIn` is passed through to `comStatusOut` unchanged.
