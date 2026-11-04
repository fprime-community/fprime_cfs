# ======================================================================
# CfsRouterCfg.fpp
# Constants for configuring the CfsRouter routing table and port counts
#
# Projects override this file (register_fprime_config CONFIGURATION_OVERRIDES)
# to supply their own APID routing table. Any cFS APID to be routed must also
# be present in the project's ComCfg.Apid enumeration; the space packet
# deframer maps unknown APIDs to INVALID_UNINITIALIZED, which takes the
# unknown route.
# ======================================================================
module FPrimeCfs {

    @ Number of F Prime command output ports on the CfsRouter
    constant CFS_ROUTER_FPRIME_COMMAND_PORTS = 10

    @ Number of cFS command output ports on the CfsRouter
    constant CFS_ROUTER_CFS_COMMAND_PORTS = 10

    @ Number of cFS telemetry output ports on the CfsRouter
    constant CFS_ROUTER_CFS_TELEMETRY_PORTS = 10

    @ Maximum number of buffers simultaneously outstanding on the pass-through
    @ routes (cFS command, cFS telemetry, unknown) awaiting return
    constant CFS_ROUTER_MAX_PENDING_BUFFERS = 10

    @ Number of entries in the APID routing table
    constant CFS_ROUTER_ROUTE_TABLE_SIZE = 2

    @ The APID routing table: APID -> (route type, output port index).
    @ This default configuration routes F Prime commands and file packets;
    @ projects override with their own cFS APIDs and routes.
    constant CFS_ROUTER_ROUTE_TABLE = [
        { apid = ComCfg.Apid.FW_PACKET_COMMAND, functionCode = ComCfg.FprimeCommandFunctionCode, routeType = CfsRouteType.FPRIME_COMMAND, portIndex = 0 },
        { apid = ComCfg.Apid.FW_PACKET_FILE,    functionCode = 0, routeType = CfsRouteType.FILE,           portIndex = 0 },
    ]
}
