# FPrimeCfs::CfsBridge

Bridge F Prime communication constructs to and from the cFS Software Bus (SB). `CfsBridge` acts as the
boundary between an F Prime communication stack and cFS: it *frames* outgoing F Prime data into cFS
messages and transmits them on the software bus, and it *deframes* incoming cFS messages into F Prime
buffers annotated with their APID. In F Prime terms it fills the combined role of a framer, a deframer,
and a communication driver.

`CfsBridge` is a **queued** component: incoming F Prime port calls on `dataIn` are queued, and the
hosting cFS application drives the component by calling `process()` from its run loop. Each `process()`
call drains the F Prime message queue and then polls the software bus pipe for at most one message.

## Usage Examples

A hosting cFS application configures and runs the bridge as follows:

```cpp
FPrimeCfs::CfsBridge bridge("bridge");
bridge.init(QUEUE_DEPTH, INSTANCE_ID);

// Create the SB pipe and subscribe to the APIDs to receive
CFE_Status_t status = bridge.configure(PIPE_DEPTH, "MY_PIPE", /*paused=*/true);
status = bridge.subscribe(ComCfg::Apid::FW_PACKET_COMMAND);

// In the application's run loop:
while (CFE_ES_RunLoop(&runStatus)) {
    if (bridge.process() == Fw::QueuedComponentBase::MSG_DISPATCH_EXIT) {
        break;
    }
}
```

When `paused` is true, flow control is enabled: deframed messages are held until a `comStatusIn`
success signal is received, and one message is released per success signal. This paces uplink data to
match the downstream pipeline (e.g. an `Svc::FprimeRouter`/`Svc::ComQueue` stack).

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| output | `dataOut` | `Svc.ComDataWithContext` | Deframed data (payload of a received SB message) with the APID in the context |
| sync input | `dataReturnIn` | `Svc.ComDataWithContext` | Return of ownership for buffers sent on `dataOut` (no-op: SB owns its buffers) |
| sync input | `comStatusIn` | `Fw.SuccessCondition` | Downstream com status; a SUCCESS unpauses one deframed message when flow control is enabled |
| async input | `dataIn` | `Svc.ComDataWithContext` | F Prime data to frame into a cFS message and transmit on the SB |
| output | `dataReturnOut` | `Svc.ComDataWithContext` | Return of ownership for buffers received on `dataIn` |
| output | `comStatusOut` | `Fw.SuccessCondition` | Com status emitted after each transmission attempt, and once as a preroll |

## Requirements

| Name | Description | Validation |
|---|---|---|
| FPRIMECFS-CFSBRIDGE-001 | `CfsBridge` shall create a software bus pipe with the configured depth and name when `configure()` is called, and shall report the software bus status to the caller | Unit test |
| FPRIMECFS-CFSBRIDGE-002 | `CfsBridge` shall subscribe to the software bus message ID derived from the supplied APID when `subscribe()` is called, mapping `FW_PACKET_COMMAND` into the platform command message ID space and all other APIDs into the platform telemetry message ID space | Unit test |
| FPRIMECFS-CFSBRIDGE-003 | `CfsBridge` shall frame each buffer received on `dataIn` into a cFS message (command header for `FW_PACKET_COMMAND`, telemetry header otherwise) and transmit it on the software bus | Unit test |
| FPRIMECFS-CFSBRIDGE-004 | `CfsBridge` shall drop `dataIn` data that cannot be transmitted (oversize data, message initialization failure, or transmission failure) and log an error, without asserting | Unit test |
| FPRIMECFS-CFSBRIDGE-005 | `CfsBridge` shall poll the software bus pipe during `process()` and emit each received message's payload on `dataOut` with the message's APID set in the frame context | Unit test |
| FPRIMECFS-CFSBRIDGE-006 | `CfsBridge` shall drop received software bus messages whose message ID cannot be read or does not map to a valid APID, logging an error, without asserting | Unit test |
| FPRIMECFS-CFSBRIDGE-007 | When configured with flow control enabled, `CfsBridge` shall hold received messages while paused and shall release exactly one message per `comStatusIn` SUCCESS signal | Unit test |
| FPRIMECFS-CFSBRIDGE-008 | `CfsBridge` shall emit a single `comStatusOut` SUCCESS (preroll) on the first `process()` call after subscription to open the downstream communication pipeline | Unit test |
| FPRIMECFS-CFSBRIDGE-009 | `CfsBridge` shall return ownership of every buffer received on `dataIn` via `dataReturnOut` and shall emit a `comStatusOut` SUCCESS after every transmission attempt, regardless of outcome | Unit test |

## Design

### Framing (F Prime → cFS)

`dataIn` is an async input: invocations are queued and dispatched from `process()`. The handler builds
a cFS message in a local, statically-sized union of command and telemetry headers plus payload space,
initializes it with `CFE_MSG_Init()` using the message ID derived from the context APID, copies the
payload behind the header, and transmits with `CFE_SB_TransmitMsg()`. Ownership of the incoming buffer
is always returned via `dataReturnOut` and `comStatusOut` always reports SUCCESS, since the software
bus does not support retry semantics.

### Deframing (cFS → F Prime)

`process()` polls the pipe with `CFE_SB_ReceiveBuffer(..., CFE_SB_POLL)` (at most one message per
call). The payload pointer and length are obtained from `CFE_SB_GetUserData()`/
`CFE_SB_GetUserDataLength()` and wrapped in an `Fw::Buffer` that aliases the SB buffer — no copy is
performed, which is safe because SB buffers remain valid until the next `CFE_SB_ReceiveBuffer()` call
on the pipe and the downstream consumers of `dataOut` operate synchronously within `process()`.
`dataReturnIn` is therefore a no-op.

The APID is recovered by masking the message ID value (there is no inverse of the
`CFE_PLATFORM_*_TOPICID_TO_MIDV` macros). Since software bus messages are external input, a message ID
that fails this round-trip mapping is dropped with a logged error rather than asserted upon.

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
