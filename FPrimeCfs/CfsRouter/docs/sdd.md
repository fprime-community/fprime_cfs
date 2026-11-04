# FPrimeCfs::CfsRouter

Routes packets deframed by an `Svc.Ccsds.SpacePacketDeframer` to the rest of the system. `CfsRouter` replaces
`Svc.FprimeRouter` in a cFS-hosted topology: F Prime commands are copied and forwarded to a command dispatcher as
`Svc.FprimeRouter` does, while cFS commands and telemetry are forwarded on custom ports carrying the fields of their
cFS secondary headers (function code, or time) together with the payload. Packets with no configured route are
forwarded to an unknown output.

## Usage

```
CfsBridge.dataOut -> Svc.Ccsds.SpacePacketDeframer.dataIn
Svc.Ccsds.SpacePacketDeframer.dataOut -> CfsRouter.dataIn
CfsRouter.dataReturnOut -> Svc.Ccsds.SpacePacketDeframer.dataReturnIn
CfsRouter.commandOut[i] -> [command dispatcher].seqCmdBuff
CfsRouter.cfsCommandOut[j] / cfsTelemetryOut[k] / unknownDataOut -> [cFS-aware consumers]
[consumers' buffer return] -> CfsRouter.bufferReturnIn
```

Routing is selected by a static routing table keyed on APID and, for command packets, the cFS command function code, configured in `config/CfsRouterConfig/CfsRouterCfg.fpp`.
Projects override this file (via `register_fprime_config` `CONFIGURATION_OVERRIDES`) to supply their own table,
table size, port-array sizes, and pending-buffer limit:

```
constant CFS_ROUTER_ROUTE_TABLE_SIZE = 3
constant CFS_ROUTER_ROUTE_TABLE = [
    { apid = ComCfg.Apid.FW_PACKET_COMMAND, functionCode = ComCfg.FprimeCommandFunctionCode, routeType = CfsRouteType.FPRIME_COMMAND, portIndex = 0 },
    { apid = ComCfg.Apid.SOME_CFS_CMD_APID, functionCode = 0, routeType = CfsRouteType.CFS_COMMAND,    portIndex = 0 },
    { apid = ComCfg.Apid.SOME_CFS_TLM_APID, functionCode = 0, routeType = CfsRouteType.CFS_TELEMETRY,  portIndex = 0 },
]
```

The route entry type (`CfsRouteEntry`), route type enumeration (`CfsRouteType`), and time type (`CfsTime`) are
defined in the `Types` module (`Types/CfsRouterTypes.fpp`); the `CfsCommand` and `CfsTelemetry` port types are
defined in the common `FPrimeCfs/Ports` module.

Note: any cFS APID to be routed must also be present in the project's `ComCfg.Apid` enumeration, since the
`Svc.Ccsds.SpacePacketDeframer` maps APIDs not in the enumeration to `INVALID_UNINITIALIZED` (which then takes the
unknown route).

## Secondary headers

- cFS command secondary header (2 bytes): `{U8 FunctionCode, U8 Checksum}`. The function code is forwarded on
  `cfsCommandOut`; the payload is the data after the secondary header.
- cFS telemetry secondary header (10 bytes, big-endian): 4-byte seconds, 2-byte subseconds (the most
  significant 16 bits of the 2^-32 subseconds value), then the 4 spare bytes cFE adds to
  `CFE_MSG_TelemetryHeader_t` (payload at offset 16 of the packet). The time is forwarded on `cfsTelemetryOut` as a `CfsTime`
  (`seconds: U32`, `subseconds: U32`); the payload is the data after the secondary header.
- F Prime commands arrive framed as cFS commands: the 2-byte command secondary header precedes the F Prime
  command data and is excluded from the copied data (a command without it is not routed; see below).

## Checksum verification

Command packets (frame context `pktType == SPP_COMMAND`) must carry the 2-byte cFS command secondary header
(function code, checksum). Per `CFE_MSG` conventions the XOR of every byte of the space packet (primary header,
secondary header, payload) with `0xFF` is zero for a valid command. The primary header was consumed by the deframer,
so it is reconstructed from the frame context (APID, sequence count, length) for the computation. Commands failing
the check are dropped with `BadChecksum` before any route lookup.

## Buffer ownership

- F Prime command route: the packet data is copied into an `Fw::ComBuffer`; the incoming buffer is returned to the
  sender on `dataReturnOut` immediately. This is the only route that copies.
- cFS command, cFS telemetry, and unknown routes: ownership of the (payload) buffer transfers to the receiver, and
  returns on `bufferReturnIn`, which forwards it to `dataReturnOut`. Receivers that return data together with a
  context (such as a downstream router's `dataReturnOut`) may return buffers on `dataReturnIn`
  instead; the supplied context is ignored and the original context is restored.
- Buffers are always returned on `dataReturnOut` with the context they were received with. For pass-through routes
  the router records the buffer-to-context association in a map (capacity `CFS_ROUTER_MAX_PENDING_BUFFERS`); if the
  map is full, the packet is returned unrouted with a warning event.
- If the target output port for a computed route is not connected, the buffer is returned immediately rather than
  asserting or leaking.
- File route: APIDs configured as `FILE` route the whole packet data to `fileOut` (`Fw.BufferSend`, matching
  the `Svc.Router` interface). Ownership transfers to the receiver and returns on `fileBufferReturnIn`, which
  behaves identically to `bufferReturnIn`.

## Events

| Event | Severity | When |
| --- | --- | --- |
| `CommandMissingSecondaryHeader` | warning high | A command packet has no (or a truncated) cFS command secondary header; forwarded on `unknownDataOut` |
| `BadChecksum` | warning high | A command packet failed cFS checksum verification; dropped (buffer returned) before any routing |
| `NoRoute` | warning low | No routing table entry matched (APID, or APID and function code for commands); forwarded on `unknownDataOut` |
| `MissingSecondaryHeader` | warning high | A telemetry packet routed as `CFS_TELEMETRY` has no (or a truncated) cFS telemetry secondary header; forwarded on `unknownDataOut` |
| `SerializationError` | warning high | Copying an F Prime command into a com buffer failed |

## Requirements

| Requirement | Description | Verification Method |
|---|---|---|
| FPRIMECFS-CFSROUTER-001 | CfsRouter shall select a route for each message received on `dataIn` using a statically configured (APID, function code) to (route type, port index) routing table, matching command packets on APID and function code and telemetry packets on APID. | Unit test |
| FPRIMECFS-CFSROUTER-001a | CfsRouter shall verify the cFS command checksum of every command packet before routing it, and shall drop (returning the buffer to the sender) and emit a warning event for any command whose checksum is invalid. | Unit test |
| FPRIMECFS-CFSROUTER-002 | CfsRouter shall route messages whose APID is configured as `FPRIME_COMMAND` to the configured F Prime command output port by copying the packet data into an `Fw::ComBuffer`. | Unit test |
| FPRIMECFS-CFSROUTER-003 | CfsRouter shall route messages whose APID is configured as `CFS_COMMAND` to the configured cFS command output port, providing the function code (`U8`) parsed from the cFS command secondary header and the payload buffer. | Unit test |
| FPRIMECFS-CFSROUTER-004 | CfsRouter shall route messages whose APID is configured as `CFS_TELEMETRY` to the configured cFS telemetry output port, providing the time parsed from the 10-byte cFS telemetry secondary header (6-byte big-endian time plus the 4 spare bytes of `CFE_MSG_TelemetryHeader_t`) and the payload buffer. | Unit test |
| FPRIMECFS-CFSROUTER-005 | CfsRouter shall route any message without a matching route (APID, or APID and function code for commands) to the unknown data output port with its context and emit the low-severity `NoRoute` warning event. | Unit test |
| FPRIMECFS-CFSROUTER-006 | CfsRouter shall route command packets (any APID) and messages configured as `CFS_TELEMETRY` whose context indicates no secondary header, or whose data is too small to contain the secondary header, to the unknown data output port and emit a warning event (`CommandMissingSecondaryHeader` / `MissingSecondaryHeader`). | Unit test |
| FPRIMECFS-CFSROUTER-007 | For `FPRIME_COMMAND` routing, CfsRouter shall exclude the (already verified) 2-byte cFS command secondary header from the data copied into the com buffer. | Unit test |
| FPRIMECFS-CFSROUTER-008 | For `FPRIME_COMMAND` routing, CfsRouter shall return ownership of the received buffer to the sender immediately after copying. | Unit test |
| FPRIMECFS-CFSROUTER-009 | For `CFS_COMMAND`, `CFS_TELEMETRY`, and unknown routing, CfsRouter shall transfer buffer ownership to the receiver and return ownership to the sender only after the buffer is returned on `bufferReturnIn`. | Unit test |
| FPRIMECFS-CFSROUTER-010 | CfsRouter shall complete every buffer ownership transfer by eventually returning the received buffer to the sender on `dataReturnOut`. | Unit test |
| FPRIMECFS-CFSROUTER-011 | CfsRouter shall emit a warning-severity event when packet data cannot be copied into a command buffer. | Unit test |
| FPRIMECFS-CFSROUTER-012 | When the target output port for a computed route is not connected, CfsRouter shall return the received buffer to the sender rather than assert or leak. | Unit test |
| FPRIMECFS-CFSROUTER-013 | CfsRouter shall accept command responses on `cmdResponseIn` as a no-op. | Unit test |
| FPRIMECFS-CFSROUTER-014 | CfsRouter shall return every buffer on `dataReturnOut` with the context it was received with. | Unit test |
| FPRIMECFS-CFSROUTER-015 | CfsRouter shall route messages whose APID is configured as `FILE` to the file output port, transferring buffer ownership to the receiver and returning ownership to the sender only after the buffer is returned on `fileBufferReturnIn`. | Unit test |
| FPRIMECFS-CFSROUTER-016 | When a buffer is returned on `dataReturnIn` or `bufferReturnIn`, CfsRouter shall restore and return the original buffer (pointer and size) it was tracked with; untracked buffers shall be forwarded as-is with the supplied (or default) context. | Unit test |
