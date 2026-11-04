# fprime_cfs

This library supports the use of the F Prime framework as a mechanism for building cFS applications. This library
contains the necessary helper components to expedite the development of cFS applications using the F Prime framework.

> [!WARNING]
> This code is currently a functional prototype. Some adjustments may be required for your specific project.

## Design

```mermaid
flowchart LR
    fp(("F Prime App")) --- Bus[cFS Messaging Bus]
    cfs1(("cFS App")) --- Bus
    cfs2(("cFS App")) --- Bus
    cfs3(("...")) --- Bus
```

The `fprime_cfs` library is designed to allow F Prime implementations of cFS applications that attach to the cFS messaging bus just like any other cFS application.

```mermaid
flowchart LR
    subgraph fprime["F Prime App"]
        custom["Custom Component(s)"] <--> ccsds["ComCfs Subtopology"]
        cdh["CfsCore Subtopology"] <--> ccsds
        fpcomp["F Prime Component(s)"] <--> cdh
        custom <--> cdh
        ccsds <--> cfs((CfsBridge))
    end
    cfs --- Bus[cFS Messaging Bus]
```

The reusable components and topologies provided by F Prime can be used to construct the internals of the cFS app.  The CfsBridge component provided by this library bridges F Prime to the cFS bus allowing for the production and consumption of cFS messages.


## Components

| Component    | Purpose                                                         | SDD Link                                             |
|--------------|-----------------------------------------------------------------|------------------------------------------------------|
| CfsBridge    | Expose access to the cFS messaging bus as an F Prime Component. | [CfsBridge](./FPrimeCfs/CfsBridge/docs/sdd.md)       |
| CfsAppBridge | Route F Prime com data toward a destination cFS application's APID. | [CfsAppBridge](./FPrimeCfs/CfsAppBridge/docs/sdd.md) |
| CfsCmdFramer | Secondary framer applying the cFS command secondary header.     | [CfsCmdFramer](./FPrimeCfs/CfsCmdFramer/docs/sdd.md) |
| CfsTlmFramer | Secondary framer applying the cFS telemetry secondary header.   | [CfsTlmFramer](./FPrimeCfs/CfsTlmFramer/docs/sdd.md) |
| CfsRouter    | Route deframed space packets by APID to command, telemetry, and file consumers. | [CfsRouter](./FPrimeCfs/CfsRouter/docs/sdd.md) |
| EvsMirror    | Text logger replacement publishing F Prime events to cFS Event Services (EVS). | [EvsMirror](./FPrimeCfs/EvsMirror/docs/sdd.md)  |
| SchAppDriver | Drive F Prime rate groups from cFS scheduler (SCH) messages.    | [SchAppDriver](./FPrimeCfs/SchAppDriver/docs/sdd.md) |
| CfsSystemTime | Time component backed by cFS time services (CFE_TIME).        | [CfsSystemTime](./FPrimeCfs/CfsSystemTime/docs/sdd.md) |
| PollingTimer | Rate group timer that is polled by the main program loop.       | [PollingTimer](./FPrimeCfs/PollingTimer/docs/sdd.md) |

## Subtopologies

| Subtopology | Purpose                                                          | SDD Link                                             |
|-------------|------------------------------------------------------------------|------------------------------------------------------|
| ComCfs      | Full cFS communications stack: app bridges, cFS secondary framers, space packet framing, cFS-aware routing, and the CfsBridge. | [ComCfs](./FPrimeCfs/Subtopologies/ComCfs/docs/sdd.md) |
| CfsCore     | Core command and data handling with cFS choices: EvsMirror text logging, TlmPacketizer telemetry, CfsSystemTime time, SchAppDriver rate groups, EventManager event downlink. | [CfsCore](./FPrimeCfs/Subtopologies/CfsCore/docs/sdd.md) |

## Integration and Usage

In order to use this library, you will need to add fprime and this library as librarues in your cFS `targets.cmake`:

```cmake
list(APPEND MISSION_GLOBAL_APPLIST fprime fprime_cfs ...)
```

This will integrate the library into the cFS build and make the targets available for the F Prime build as well.

## Ground Tooling

The [python/](./python) directory provides the `fprime-cfs` Python package: tools that generate cFS GroundSystem configuration from the F Prime dictionary and run the cFS GroundSystem as the GDS for an F Prime deployment. See [python/README.md](./python/README.md).

## Work To Go

This library still needs to demonstrate how to subscribe to cFS messages that aren't strictly commands (e.g. telemetry) and how to rout these messages.

Unsupported features:
1. Cross-compilation to other F Prime platforms

