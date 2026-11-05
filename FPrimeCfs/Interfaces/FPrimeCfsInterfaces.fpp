#####
# FPrimeCfs interfaces:
#
# Port sets for components that consume cFS messages routed by CfsRouter
# (handlers) and for components that produce raw cFS messages for the
# ComCfs framers (producers).
#####

module FPrimeCfs {

    @ A consumer of routed cFS commands. Connect CfsRouter.cfsCommandOut[i]
    @ to cfsCommandIn and bufferReturnOut to CfsRouter.bufferReturnIn.
    @ The payload buffer must be returned on bufferReturnOut when done.
    interface CfsCommandHandler {
        @ Routed cFS command: APID, function code and payload bytes
        guarded input port cfsCommandIn: FPrimeCfs.CfsCommand

        @ Returns ownership of the command payload buffer to the router
        output port bufferReturnOut: Fw.BufferSend
    }

    @ A consumer of routed cFS telemetry. Connect CfsRouter.cfsTelemetryOut[i]
    @ to cfsTelemetryIn and bufferReturnOut to CfsRouter.bufferReturnIn.
    @ The payload buffer must be returned on bufferReturnOut when done.
    interface CfsTelemetryHandler {
        @ Routed cFS telemetry: APID, packet time and payload bytes
        guarded input port cfsTelemetryIn: FPrimeCfs.CfsTelemetry

        @ Returns ownership of the telemetry payload buffer to the router
        output port bufferReturnOut: Fw.BufferSend
    }

    @ A producer of raw cFS commands. The producer fills a payload buffer and a
    @ ComCfg.FrameContext (apid = destination command APID, functionCode) and
    @ sends the pair on cfsCommandOut to ComCfs.cfsCommandDataIn; the buffer
    @ comes back on cfsCommandReturnIn once the bridge has copied it. The
    @ framers add the cFS headers and checksum.
    interface CfsCommandProducer {
        @ Raw command payload with its frame context (APID, function code)
        output port cfsCommandOut: Svc.ComDataWithContext

        @ Returns the command payload buffer once the bridge has copied it
        sync input port cfsCommandReturnIn: Svc.ComDataWithContext
    }

    @ A producer of raw cFS telemetry. The producer fills a payload buffer and a
    @ ComCfg.FrameContext (apid = telemetry APID) and sends the pair on
    @ cfsTelemetryOut to ComCfs.cfsTelemetryDataIn; the buffer comes back on
    @ cfsTelemetryReturnIn once the bridge has copied it. The framers add the
    @ cFS headers and time tag.
    interface CfsTelemetryProducer {
        @ Raw telemetry payload with its frame context (APID)
        output port cfsTelemetryOut: Svc.ComDataWithContext

        @ Returns the telemetry payload buffer once the bridge has copied it
        sync input port cfsTelemetryReturnIn: Svc.ComDataWithContext
    }

}
