# FPrimeCfs::CfsTlmFramer

A framer stage that wraps outgoing F Prime telemetry in a cFS telemetry secondary header (big-endian 4-byte
seconds, 2-byte subseconds, then the 4 spare bytes of `CFE_MSG_TelemetryHeader_t`) ahead of a downstream space packet framer (e.g. `Svc.Ccsds.SpacePacketFramer`). The
result, once the downstream framer prepends the CCSDS primary header, is a valid cFS telemetry packet
(`CFE_MSG_TelemetryHeader_t` layout).

## Usage

```
Svc.ComQueue.dataOut -> CfsTlmFramer.dataIn
CfsTlmFramer.dataOut -> Svc.Ccsds.SpacePacketFramer.dataIn
Svc.Ccsds.SpacePacketFramer.dataReturnOut -> CfsTlmFramer.dataReturnIn
CfsTlmFramer.dataReturnOut -> Svc.ComQueue.dataReturnIn
CfsTlmFramer.bufferAllocate / bufferDeallocate -> [buffer manager]
CfsTlmFramer.cfsTimeConvert -> [FPrimeCfs.CfsSystemTime or equivalent]
```

The framer sets `hasSecHdr = true` in the context it forwards downstream, so the space packet framer sets the
secondary header flag in the CCSDS primary header.

## Time extraction

The header time is extracted from the incoming packet's own time tag. F Prime downlink packets serialize a packet
descriptor, an id, and an `Fw.Time` time tag, in that order; only the id width differs by packet type:

- `FW_PACKET_LOG` (events): `FwEventIdType` id
- `FW_PACKET_TELEM` (telemetry channels): `FwChanIdType` id (the time of the first entry is used)
- `FW_PACKET_PACKETIZED_TLM`: `FwTlmPacketizeIdType` id

If the descriptor is not one of these, or the packet is too short, the current system time is used instead and a
`TimeExtractionFailed` warning event is emitted. The original packet bytes are always forwarded unchanged after
the secondary header.

## Time conversion

The extracted `Fw.Time` is converted to a cFS system time (`CfsTime`: seconds, 2^-32 subseconds) through the
`cfsTimeConvert` output port, allowing a project to supply a mission-accurate conversion (e.g. one backed by
`CFE_TIME_Micro2SubSecs`). When the port is unconnected, the conversion is computed directly as
`subseconds = usec * 2^32 / 10^6`. The 10-byte header stores the seconds and the most significant 16 bits of the
subseconds (matching `CFE_MSG_TelemetrySecondaryHeader_t`) followed by 4 zero bytes: cFE pads
`CFE_MSG_TelemetryHeader_t` to 16 bytes with `Spare[4]`, so a native cFS application's payload starts at
offset 16. `FPrimeCfs::CfsRouter` parses the same layout.

## Buffer ownership

- `dataIn`: a new buffer is allocated via `bufferAllocate` for the wrapped packet; the incoming buffer is returned
  to the sender on `dataReturnOut` immediately with its original context.
- `dataReturnIn`: the wrapped buffer coming back from downstream is deallocated via `bufferDeallocate`.
- If allocation fails (undersized buffer), the packet is dropped with an `AllocationFailed` warning event, and any
  non-empty allocation is deallocated.

`comStatusIn` is passed through to `comStatusOut` unchanged.
