# FPrimeCfs::CfsBridge

Bridge F Prime communication constructs to and from the cFS Software Bus (SB). `CfsBridge` acts as the
boundary between an F Prime communication stack and cFS: outgoing data is one or more complete CCSDS
space packets (e.g. from an `Svc::Ccsds::SpacePacketFramer`, possibly concatenated by an
`Svc::ComAggregator`), each transmitted on the software bus as its own message, and incoming cFS
messages are emitted whole (as CCSDS space packets) for deframing by a downstream deframer (e.g.
`Svc::Ccsds::SpacePacketDeframer`). In F Prime terms it fills the role of a communication driver at
the space packet layer.

`CfsBridge` is a **queued** component: incoming F Prime port calls on `dataIn` are queued, and the
hosting cFS application drives the component by calling `process()` from its run loop, or by connecting
the `schedIn` port to a rate group. Each `process()` call (or `schedIn` invocation) drains the F Prime
message queue and then polls the software bus pipe for at most one message.

## Usage Examples

A hosting cFS application configures and runs the bridge as follows:

```cpp
FPrimeCfs::CfsBridge bridge("bridge");
bridge.init(QUEUE_DEPTH, INSTANCE_ID);

// Create the SB pipe and subscribe to the APIDs to receive
// Optionally pass wrapFprimeCommands=true to transmit F Prime command packets as valid cFS
// command packets (secondary header flag + function code + checksum)
CFE_Status_t status = bridge.configure(PIPE_DEPTH, "MY_PIPE", /*paused=*/true, /*wrapFprimeCommands=*/false);
status = bridge.subscribe(ComCfg::Apid::FW_PACKET_COMMAND);

// Optionally subscribe to native cFS messages (secondary header flag set). COMMAND sets the
// packet type bit (e.g. scheduler (SCH) wakeup messages); TELEMETRY leaves it clear (e.g.
// housekeeping telemetry).
status = bridge.subscribeCfs(ComCfg::Apid::CFS_SCH_TICK, FPrimeCfs::CfsBridge::CfsMessageType::COMMAND);

// In the application's run loop:
while (CFE_ES_RunLoop(&runStatus)) {
    if (bridge.process() == Fw::QueuedComponentBase::MSG_DISPATCH_EXIT) {
        break;
    }
}
```

When `paused` is true, flow control is enabled: received messages are held until a `comStatusIn`
success signal is received, and one message is released per success signal. This paces uplink data to
match the downstream pipeline (e.g. a deframer/router stack).

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| output | `dataOut` | `Svc.ComDataWithContext` | Complete received SB message (a CCSDS space packet including headers) with a default context |
| sync input | `dataReturnIn` | `Svc.ComDataWithContext` | Return of ownership for buffers sent on `dataOut` (deallocated via `bufferDeallocate` when `bufferAllocate` is connected; no-op otherwise) |
| sync input | `comStatusIn` | `Fw.SuccessCondition` | Downstream com status; a SUCCESS unpauses one received message when flow control is enabled |
| sync input | `schedIn` | `Svc.Sched` | Rate-group tick; each invocation performs one `process()` call (drain queue, poll SB once) |
| output | `bufferAllocate` | `Fw.BufferGet` | Allocates a buffer to copy each received SB message into; when unconnected, received messages are emitted zero-copy and must be consumed synchronously |
| output | `bufferDeallocate` | `Fw.BufferSend` | Returns allocated `dataOut` buffers to the allocator once returned on `dataReturnIn` |
| async input | `dataIn` | `Svc.ComDataWithContext` | One or more complete CCSDS space packets, each transmitted on the SB as its own message |
| output | `dataReturnOut` | `Svc.ComDataWithContext` | Return of ownership for buffers received on `dataIn` |
| output | `comStatusOut` | `Fw.SuccessCondition` | Com status emitted after each transmission attempt, and once as a preroll |

## Requirements

| Name | Description | Validation |
|---|---|---|
| FPRIMECFS-CFSBRIDGE-001 | `CfsBridge` shall create a software bus pipe with the configured depth and name when `configure()` is called, and shall report the software bus status to the caller | Unit test |
| FPRIMECFS-CFSBRIDGE-002 | `CfsBridge` shall subscribe to the software bus message ID derived from the supplied APID when `subscribe()` is called, mirroring the space packet stream identifier: the APID in the low 11 bits with the packet type bit set for `FW_PACKET_COMMAND` and clear otherwise | Unit test |
| FPRIMECFS-CFSBRIDGE-003 | `CfsBridge` shall transmit each complete CCSDS space packet contained in a buffer received on `dataIn` as its own software bus message | Unit test |
| FPRIMECFS-CFSBRIDGE-004 | `CfsBridge` shall drop `dataIn` data that cannot be transmitted (truncated packets, residual bytes, or transmission failure) and log an error, without asserting | Unit test |
| FPRIMECFS-CFSBRIDGE-005 | `CfsBridge` shall poll the software bus pipe during `process()` and emit each received message whole (headers included) on `dataOut` with a default frame context | Unit test |
| FPRIMECFS-CFSBRIDGE-006 | `CfsBridge` shall drop received software bus messages whose size cannot be read, logging an error, without asserting | Unit test |
| FPRIMECFS-CFSBRIDGE-007 | When configured with flow control enabled, `CfsBridge` shall hold received messages while paused and shall release exactly one message per `comStatusIn` SUCCESS signal | Unit test |
| FPRIMECFS-CFSBRIDGE-008 | `CfsBridge` shall emit a single `comStatusOut` SUCCESS (preroll) on the first `process()` call after subscription to open the downstream communication pipeline | Unit test |
| FPRIMECFS-CFSBRIDGE-009 | `CfsBridge` shall return ownership of every buffer received on `dataIn` via `dataReturnOut` and shall emit a `comStatusOut` SUCCESS after every transmission attempt, regardless of outcome | Unit test |
| FPRIMECFS-CFSBRIDGE-010 | `CfsBridge` shall subscribe to native cFS messages when `subscribeCfs()` is called, forming the message ID with the secondary header flag set and the packet type bit set for `CfsMessageType::COMMAND` and clear for `CfsMessageType::TELEMETRY` | Unit test |
| FPRIMECFS-CFSBRIDGE-011 | `CfsBridge` shall reject a `configure()` pipe depth exceeding the cFE `uint16` pipe depth range with `CFE_SB_BAD_ARGUMENT` rather than silently truncating | Unit test |
| FPRIMECFS-CFSBRIDGE-012 | When configured with F Prime command wrapping enabled, `CfsBridge` shall transmit each F Prime command space packet (packet type command, no secondary header) as a valid cFS command packet: secondary header flag set, length field adjusted, and a 2-byte cFS command secondary header (function code `ComCfg.FprimeCommandFunctionCode` and a valid cFS XOR checksum) inserted between the primary header and the payload; telemetry packets and packets already carrying a secondary header shall be transmitted unmodified, and packets too large to wrap shall be dropped with a logged error | Unit test |
| FPRIMECFS-CFSBRIDGE-013 | `CfsBridge` shall perform one `process()` call (drain the message queue and poll the software bus pipe once) for each invocation of `schedIn`, allowing rate-group driven operation | Unit test |
| FPRIMECFS-CFSBRIDGE-014 | When `bufferAllocate` is connected, `CfsBridge` shall copy each received software bus message into an allocated buffer before emitting it on `dataOut`, shall deallocate the buffer when returned on `dataReturnIn`, and shall drop the message with a logged error when allocation fails | Unit test |
| FPRIMECFS-CFSBRIDGE-015 | `CfsBridge` shall subscribe with a per-message-ID message limit equal to the `msgLimit` argument when one is given to `subscribe()` or `subscribeCfs()` (including 0), and otherwise equal to the configured pipe depth | Unit test |

## Design

### Transmitting (F Prime → cFS)

`dataIn` is an async input: invocations are queued and dispatched from `process()`. The buffer
contains one or more complete CCSDS space packets. The handler walks the buffer using each packet's
primary header length field and transmits each packet in place with `CFE_SB_TransmitMsg()` (the
software bus derives the message ID from the packet's stream identifier and copies the data).
Truncated packets and residual bytes are dropped with a logged error. Ownership of the incoming
buffer is always returned via `dataReturnOut` and `comStatusOut` always reports SUCCESS, since the
software bus does not support retry semantics.

When `configure(..., wrapFprimeCommands=true)` is used, each F Prime command space packet (stream
identifier with the packet type bit set and the secondary header flag clear) is wrapped as a valid
cFS command packet before transmission: the packet is copied into internal storage, the secondary
header flag is set in the primary header, the 16-bit length field is increased by two, and a 2-byte
cFS command secondary header `{U8 FunctionCode, U8 Checksum}` is inserted between the primary header
and the F Prime command payload. The function code is the fixed
`ComCfg.FprimeCommandFunctionCode` used for all F Prime passthrough commands, and the checksum
is computed per `CFE_MSG` conventions (the XOR of every packet byte with 0xFF equals zero). Telemetry
packets and packets that already carry a secondary header are transmitted unmodified. The internal
wrap storage is bounded by `CFS_BRIDGE_MAX_WRAPPED_PACKET_SIZE` (2048 bytes, well under the software
bus limit `CFE_MISSION_SB_MAX_SB_MSG_SIZE`); a command packet whose wrapped size would exceed this
bound is dropped with a logged error. Note that wrapping applies to every command-type packet
without a secondary header regardless of APID, so the uplink path should only carry F Prime command
APIDs intended for passthrough. Downstream, an `FPrimeCfs::CfsRouter` excludes the secondary header
before delivering the F Prime command bytes to the command dispatcher.

### Receiving (cFS → F Prime)

`process()` polls the pipe with `CFE_SB_ReceiveBuffer(..., CFE_SB_POLL)` (at most one message per
call). When `bufferAllocate` is connected, the message is copied into a buffer obtained from the
allocator before being emitted on `dataOut`: SB buffers remain valid only until the next
`CFE_SB_ReceiveBuffer()` call on the pipe, and the copy lets downstream consumers hold the buffer
past the current invocation (e.g. deferred-return routes such as file uplink). The copy is returned
to the allocator via `bufferDeallocate` when it comes back on `dataReturnIn`; a failed allocation
drops the message with a logged error.

When `bufferAllocate` is not connected, the message pointer and its length (from
`CFE_MSG_GetSize()`) are wrapped in an `Fw::Buffer` that aliases the SB buffer — no copy is
performed, which is safe only when every downstream consumer of `dataOut` operates synchronously
within `process()`. In this mode `dataReturnIn` is a no-op.

The message is a CCSDS space packet: it is emitted whole with a default frame context so that a
downstream deframer (e.g. `Svc::Ccsds::SpacePacketDeframer`) can validate the primary header and
derive the APID and secondary-header presence. Since software bus messages are external input, a
message whose size cannot be read is dropped with a logged error rather than asserted upon.

### Subscriptions

`subscribe()` and `subscribeCfs()` use `CFE_SB_SubscribeEx()` with the default quality of service.
The per-message-ID message limit (the most messages of that ID queued in the pipe at once) is the
`msgLimit` argument of the `subscribe(apid, msgLimit)` / `subscribeCfs(apid, type, msgLimit)`
overloads, or the configured pipe depth when it is not given. `CFE_SB_Subscribe()` would limit each
message ID to `CFE_PLATFORM_SB_DEFAULT_MSG_LIMIT` (4 by default) queued messages, so a burst of more
messages of one ID (e.g. the events produced by one command) than that limit would be dropped by the
software bus ("Msg Limit Err") while the pipe still had room.

A `msgLimit` of 0 is passed to cFE unchanged: cFE then rejects every message of that ID and emits a
"Msg Limit Err" event for each one. To stop receiving a message ID, do not subscribe to it (or
unsubscribe) rather than subscribing with a limit of 0.

### Flow control

`configure(..., paused=true)` enables flow control. While paused, `poll()` does not read from the
pipe (messages back up in the SB pipe, whose depth bounds the backlog). A `comStatusIn` SUCCESS
clears the pause; after each message is emitted on `dataOut` the component re-pauses, yielding
one-message-per-status pacing.

## Unit Testing

Unit tests run standalone (no cFS build) against a stub cFE layer in `test/ut/stubs/` that records
pipe/subscribe/transmit calls and allows queuing of inbound messages and injection of error statuses.
The suite covers all requirements, including a randomized scenario interleaving uplink, downlink, and
flow-control operations against a shadow model.

To run:

```bash
fprime-util generate --ut
fprime-util build --ut -j"$(nproc)"
fprime-util check --coverage
```

Coverage: 100% lines, 100% functions.

## Change Log

| Date | Description |
|---|---|
| 2026-07-22 | Initial SDD with requirements and unit tests |
| 2026-07-22 | Emit received messages whole (space packets) for downstream deframing instead of deframing internally |
| 2026-07-22 | Transmit complete space packets from `dataIn` as-is (one SB message per packet) instead of framing payloads; message IDs mirror space packet stream identifiers |
| 2026-07-29 | Add `subscribeCfs()` and `CfsMessageType` for subscribing to native cFS command/telemetry messages (secondary header flag set) |
| 2026-07-30 | Remove development-time subscription-success and message-received log output; operational error logging retained |
| 2026-07-30 | Reject `configure()` pipe depths that would truncate in cFE's `uint16` pipe depth |
| 2026-07-30 | Optional F Prime command wrapping: transmit F Prime command packets as valid cFS command packets (secondary header flag, function code, checksum) |
| 2026-08-04 | Add `schedIn` port for rate-group driven operation (one `process()` per tick); bound the `dataIn` packet loop by the maximum packet count |
| 2026-08-04 | Add `bufferAllocate`/`bufferDeallocate` ports: received SB messages are copied into allocated buffers so downstream consumers may hold them past the polling cycle |
| 2026-08-05 | Source the wrapped-command function code from the `ComCfg.FprimeCommandFunctionCode` dictionary constant instead of a local constant |
| 2026-10-29 | Subscribe with a per-message-ID limit equal to the pipe depth (`CFE_SB_SubscribeEx`) instead of the cFE default limit of 4 |
| 2026-10-29 | Add `subscribe()` / `subscribeCfs()` overloads taking an explicit per-message-ID `msgLimit`; the existing overloads keep the pipe-depth limit |
