module FPrimeCfs {

    @ Number of Fw.Com input ports on the CfsAppBridge
    constant CfsAppBridgeComPorts = 5

    @ App-layer bridge placed at the head of a framing chain (in place of
    @ Svc.ComQueue). Accepts either a cFS command (function code + payload
    @ buffer) or an F Prime com buffer, and emits the data with a populated
    @ frame context on a single buffer + context output:
    @
    @ - cfsCommandIn: the function code is set in the context and the APID is
    @   the configured command APID.
    @ - comIn: the packet descriptor is read from the com buffer and mapped to
    @   the corresponding APID.
    passive component CfsAppBridge {

        @ cFS command input: function code plus payload buffer. The buffer is
        @ copied and returned to the sender on bufferReturnOut before this call
        @ returns.
        guarded input port cfsCommandIn: CfsCommand

        @ F Prime com buffer input (e.g. from a packetizer, event manager, or
        @ telemetry database)
        guarded input port comIn: [CfsAppBridgeComPorts] Fw.Com

        @ Raw cFS payload with a caller-built frame context (APID, function code);
        @ the payload is copied and the caller's buffer is returned on cfsDataReturnOut
        guarded input port cfsDataIn: Svc.ComDataWithContext

        @ Returns buffers received on cfsDataIn to their sender
        output port cfsDataReturnOut: Svc.ComDataWithContext

        @ Data output: the copied data with a populated frame context
        output port dataOut: Svc.ComDataWithContext

        @ Return of the buffers sent on dataOut; they are deallocated here
        sync input port dataReturnIn: Svc.ComDataWithContext

        @ Port for returning buffers received on cfsCommandIn to their sender
        output port bufferReturnOut: Fw.BufferSend

        @ Port to allocate a buffer for the outgoing data
        output port bufferAllocate: Fw.BufferGet

        @ Port to deallocate outgoing buffers once they are returned
        output port bufferDeallocate: Fw.BufferSend

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ The allocator did not return a buffer large enough for the outgoing
        @ data; the data was dropped
        event AllocationFailed(
                $size: U32 @< The requested allocation size in bytes
            ) \
            severity warning high \
            format "Failed to allocate a {} byte buffer for outgoing data"

        @ The packet descriptor of an incoming com buffer could not be read or
        @ did not map to a known APID; the data was forwarded with the unknown
        @ APID
        event UnknownDescriptor(
                descriptor: U16 @< The packet descriptor read from the com buffer
            ) \
            severity warning low \
            format "Com buffer with unknown packet descriptor {} forwarded with the unknown APID"

        ###############################################################################
        # Standard AC Ports for Events
        ###############################################################################

        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }
}
