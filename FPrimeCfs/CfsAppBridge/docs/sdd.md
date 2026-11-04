# FPrimeCfs::CfsAppBridge

An app-layer bridge placed at the head of a framing chain, in place of `Svc.ComQueue`. It accepts either a cFS
command (function code plus payload buffer) or an F Prime com buffer, and emits the data with a populated
`ComCfg.FrameContext` on a single buffer + context output feeding the framer chain (e.g. `CfsCmdFramer` or
`CfsTlmFramer` ahead of `Svc.Ccsds.SpacePacketFramer`).

## Usage

Command sending instance:

```
[command source].cfsCommandOut -> CfsAppBridge.cfsCommandIn
CfsAppBridge.dataOut -> CfsCmdFramer.dataIn
CfsCmdFramer.dataReturnOut -> CfsAppBridge.dataReturnIn
CfsAppBridge.bufferReturnOut -> [command source's buffer return input]
```

Telemetry instance (packetizer, events, telemetry channels):

```
[packetizer / eventManager / tlmChan].PktSend/comOut -> CfsAppBridge.comIn[i]
CfsAppBridge.dataOut -> CfsTlmFramer.dataIn
CfsTlmFramer.dataReturnOut -> CfsAppBridge.dataReturnIn
```

Both instances connect `bufferAllocate` / `bufferDeallocate` to a buffer manager.

## APID selection

- `cfsCommandIn`: the APID is configured once via `configure(commandApid)`; the function code from the port call
  is set in the context's `functionCode` field (added by this library's `ComCfg.fpp` configuration override) for
  `CfsCmdFramer` to place in the cFS command secondary header.
- `comIn`: the leading `FwPacketDescriptorType` packet descriptor is read from the com buffer and mapped to the
  corresponding `ComCfg.Apid` value (descriptors coincide with the `FW_PACKET_*` APID values). An unreadable or
  unknown descriptor forwards the data with `FW_PACKET_UNKNOWN` and an `UnknownDescriptor` warning event.

## Buffer ownership

Both inputs copy the incoming data into a buffer allocated via `bufferAllocate`:

- `cfsCommandIn` buffers are returned to the sender on `bufferReturnOut` before the handler returns.
- `cfsDataIn` (`Svc.ComDataWithContext`) accepts a raw payload together with a source-built `ComCfg::FrameContext`:
  the source sets `apid` (and `functionCode` for commands) and the bridge forwards the context unchanged to the
  framer stage, so one bridge instance can serve many destination message ids. The payload is copied into the
  bridge's buffer and the source buffer is returned on `cfsDataReturnOut` before the handler returns (also when the
  allocation fails). The `ComCfs` subtopology exposes this port on both bridges as `cfsCommandDataIn` /
  `cfsCommandDataReturnOut` and `cfsTelemetryDataIn` / `cfsTelemetryDataReturnOut`.
- `comIn` receives `Fw::ComBuffer` by value; no return is needed.
- Buffers coming back on `dataReturnIn` are deallocated via `bufferDeallocate`.
- If allocation fails (undersized buffer), the data is dropped with an `AllocationFailed` warning event and any
  non-empty allocation is deallocated.

Unlike `Svc.ComQueue`, this component does not queue or pace transmission on communication status; data is
forwarded as it arrives.
