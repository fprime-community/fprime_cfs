module FPrimeCfs {
    @ Rate group driver triggered by cFS scheduler (SCH) messages. Scheduler
    @ messages are cFS commands: the CfsRouter routes the scheduler APID via
    @ its cFS command route to this component's cfsCommandIn port. Each
    @ received message with the expected function code produces one tick on
    @ CycleOut, driving the Svc.RateGroupDriver.
    passive component SchAppDriver {

        @ Implement tick interface
        import Drv.Tick

        @ Routed cFS scheduler command and payload buffer return
        import FPrimeCfs.CfsCommandHandler

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ A message with an unexpected function code was received; no tick was produced
        event UnexpectedMessage(
                functionCode: U8    @< The function code of the received message
                dataSize: U32       @< The size of the message payload in bytes
            ) \
            severity warning high \
            format "Unexpected scheduler message: function code {}, payload size {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }
}
