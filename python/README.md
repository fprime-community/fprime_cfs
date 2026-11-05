# fprime-cfs

Ground tooling for cFS systems built with `fprime_cfs`: F Prime GDS plugins (`cfs-lab` link,
`cfs` framing) that connect `fprime-gds` through CI_LAB/TO_LAB, and tools for running the
[cFS GroundSystem](https://github.com/nasa/cFS-GroundSystem) as the GDS for an F Prime
deployment.

> **Note**: the CI_LAB/TO_LAB path used by this tooling carries commands and telemetry
> over plain UDP with no authentication or integrity protection. It is intended for
> isolated lab and bench networks only, not as an operational uplink.

## Installation

```bash
pip install ./python  # from the fprime_cfs checkout root
```

## F Prime GDS plugins

Installing this package registers two [fprime-gds](https://github.com/nasa/fprime-gds)
plugins that connect the F Prime GDS (and `fprime-comm-bridge`, hence COSMOS or YAMCS) to
a cFS system through the standard lab applications:

- `cfs-lab` communication plugin: sends commands as UDP datagrams to CI_LAB (default port
  `1234`) and receives telemetry from TO_LAB (default port `2234`). It sends the TO_LAB
  enable-output command when the link opens and repeats it while no telemetry arrives: every
  second until the first telemetry packet, then after every `--to-lab-enable-interval`
  seconds (default 30, 0 never), so cFS may be started or restarted after the GDS (expect
  up to one interval of silence after a restart). Options: `--cfs-address`, `--ci-lab-port`,
  `--to-lab-port`, `--telemetry-destination` (IPv4 dotted quad of the GDS host as seen from
  cFS; TO_LAB takes no hostnames),
  `--to-lab-message-id`, `--no-enable-telemetry`, `--to-lab-enable-interval`. TO_LAB forwards
  to one destination at a time, so a running `fprime-gds` re-targets it away from any other
  ground system (e.g. the cFS GroundSystem on another host) after each quiet interval; use
  `--no-enable-telemetry` when another tool owns the TO_LAB destination.
- `cfs` framing plugin: wraps each F Prime command packet as a cFS command packet (CCSDS
  primary header on the F Prime command APID, cFS command secondary header with the
  `ComCfg.FprimeCommandFunctionCode` dictionary constant and checksum) and strips the
  CCSDS primary header and the 10-byte cFS telemetry secondary header (the time and spare
  bytes that complete the 16-byte `CFE_MSG_TelemetryHeader_t`) from each telemetry packet
  TO_LAB forwards, the ground-side inverse of
  `FPrimeCfs.CfsCmdFramer`/`FPrimeCfs.CfsTlmFramer`.

```bash
fprime-gds --communication-selection cfs-lab --framing-selection cfs \
    --dictionary <deployment>/dict/*TopologyDictionary.json
```

TO_LAB forwards only the message ids in its subscription table; subscribe the F Prime
downlink messages (telemetry `0x0801`, events `0x0802`, files `0x0803`, data products
`0x0805`, packetized telemetry `0x0820`) there for the full F Prime downlink.

## Tools

### `fprime-cfs`

Runs the full cFS GroundSystem GDS: generates the GroundSystem configuration from the
F Prime dictionary, optionally launches the cFS application, and launches the
GroundSystem GUI. The GroundSystem talks directly to the cFS software bus over UDP
through the standard lab applications: commands go to CI_LAB (default port 1234) and
telemetry arrives from TO_LAB (default port 2234). On startup the runner sends the
TO_LAB enable-output command (suppress with `--no-enable-telemetry`; the destination IP
may be changed with `--telemetry-destination`).

The F Prime GDS connects through the same CI_LAB/TO_LAB path with the plugins above; it
needs no cFS application of its own.

```bash
fprime-cfs --dictionary <deployment>/dict/*TopologyDictionary.json \
    --ground-system-dir <cfs-checkout>/tools/cFS-GroundSystem
```

By default the cFS-GroundSystem checkout is copied to a temporary directory (removed on
exit) and the configuration is generated there, leaving the checkout pristine; pass
`--no-copy` to generate the configuration in place instead.

The cFS application itself may be launched by the runner by passing `--app
<path-to-core-binary>` (with `--deployment` pointing at its directory), mirroring
`fprime-gds`; pass `--no-app`/`-n` to manage the application yourself. Extra application
arguments may be given with `--application-arguments`.

The GroundSystem GUI has its own dependencies (PyQt5, pyzmq); install them from the
cFS-GroundSystem checkout before running.

Telemetry decoding assumes the default `Fw.Time` serialization (U16 time base, U8 time
context, U32 seconds, U32 microseconds); deployments overriding the time base/context
storage types are not supported.

### `fprime-cfs-config`

Generates the cFS GroundSystem configuration only:

- `Subsystems/tlmGUI/telemetry-pages.txt` and `Subsystems/tlmGUI/fprime-tlm.txt`: a
  telemetry page for the single fixed telemetry packet defined by the dictionary. Exactly
  one packet must be defined; the tool errors otherwise. Multi-byte items use an explicit
  big-endian (`>`) struct prefix as F Prime serializes values big-endian.
- `Subsystems/cmdGui/command-pages.txt`, `CommandFiles/`, and `ParameterFiles/`: a command
  page exposing every F Prime command. All commands share the single cFS command function
  code published in the dictionary as `ComCfg.FprimeCommandFunctionCode`. The F Prime
  framing descriptor and the command opcode are prepended as the first two parameters of
  every command; their fixed required values are given in each parameter description.
  Commands with arguments that MiniCmdUtil cannot represent (e.g. strings) are skipped
  with a warning. A second `Telemetry Output` command page (the GroundSystem's stock
  TO_LAB command definitions) and a `quick-buttons.txt` with an `Enable Tlm` quick button
  are also written so telemetry output can be managed from the GUI.

Limitations inherited from the GroundSystem GUI:

- The tlmGUI telemetry page displays at most 40 rows; the tool errors if the packet
  defines more channels/fields than fit.
- MiniCmdUtil has no float or boolean entry: float arguments must be entered as their
  raw IEEE-754 bit pattern and booleans as 0 (false) / 255 (true), as noted in each
  generated parameter description.

```bash
fprime-cfs-config --dictionary <dictionary.json> --ground-system <cFS-GroundSystem dir>
```

