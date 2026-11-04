# ======================================================================
# CfsRouterCfg.fpp (unit test configuration override)
# Routing table exercising all route categories for the CfsRouter unit tests
# ======================================================================
module FPrimeCfs {

    @ Number of F Prime command output ports on the CfsRouter
    constant CFS_ROUTER_FPRIME_COMMAND_PORTS = 10

    @ Number of cFS command output ports on the CfsRouter
    constant CFS_ROUTER_CFS_COMMAND_PORTS = 10

    @ Number of cFS telemetry output ports on the CfsRouter
    constant CFS_ROUTER_CFS_TELEMETRY_PORTS = 10

    @ Maximum number of buffers simultaneously outstanding on the pass-through routes
    constant CFS_ROUTER_MAX_PENDING_BUFFERS = 10

    @ Number of entries in the APID routing table
    constant CFS_ROUTER_ROUTE_TABLE_SIZE = 4

    @ Test routing table with one route of each category
    constant CFS_ROUTER_ROUTE_TABLE = [
        { apid = ComCfg.Apid.FW_PACKET_COMMAND, functionCode = ComCfg.FprimeCommandFunctionCode, routeType = CfsRouteType.FPRIME_COMMAND, portIndex = 0 },
        { apid = ComCfg.Apid.FW_PACKET_HAND,    functionCode = 0x2A, routeType = CfsRouteType.CFS_COMMAND,    portIndex = 0 },
        { apid = ComCfg.Apid.FW_PACKET_TELEM,   functionCode = 0, routeType = CfsRouteType.CFS_TELEMETRY,  portIndex = 0 },
        { apid = ComCfg.Apid.FW_PACKET_FILE,    functionCode = 0, routeType = CfsRouteType.FILE,           portIndex = 0 },
    ]
}
