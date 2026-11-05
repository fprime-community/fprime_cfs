"""fprime_cfs.framing: F Prime GDS framing plugin for the cFS software bus packet layout

Selected with `fprime-gds --framing-selection cfs`. Uplink: each F Prime command packet
is wrapped as a complete cFS command packet (CCSDS primary header over the F Prime
command APID, cFS command secondary header with the F Prime command function code and
checksum), ready for CI_LAB to place on the software bus. Downlink: each telemetry
space packet forwarded by TO_LAB has its primary header and 10-byte cFS telemetry
secondary header (time plus the CFE_MSG_TelemetryHeader_t spare bytes) removed, leaving
the bare F Prime packet the GDS decoders expect. This is the ground-side inverse of
FPrimeCfs.CfsCmdFramer / FPrimeCfs.CfsTlmFramer.
"""

import logging

from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.common.utils.config_manager import ConfigManager
from fprime_gds.plugin.definitions import gds_plugin_implementation

from .packets import (
    PRIMARY_HEADER_SIZE,
    SEQUENCE_COUNT_MASK,
    SEQUENCE_COUNT_MODULUS,
    STREAM_ID_APID_MASK,
    STREAM_ID_SEC_HDR,
    STREAM_ID_TYPE_COMMAND,
    STREAM_ID_VERSION_MASK,
    TELEMETRY_SECONDARY_HEADER_SIZE,
    build_command_packet,
    parse_primary_header,
)

LOGGER = logging.getLogger("cfs_framing")

# Dictionary constant naming the cFS command function code carrying F Prime commands
FUNCTION_CODE_CONSTANT = "ComCfg.FprimeCommandFunctionCode"


class CfsFramerDeframer(FramerDeframer):
    """Frames F Prime commands as cFS command packets; deframes cFS telemetry packets"""

    def __init__(self):
        config = ConfigManager()
        # The APID enumeration and function code come from the deployment dictionary
        self.apid_obj = config.get_type("ComCfg.Apid")()
        self.function_code = config.get_constant(FUNCTION_CODE_CONSTANT)
        self.uplink_sequence_counts = {}
        self.downlink_sequence_counts = {}

    def frame(self, data):
        """Wrap an F Prime packet in a cFS command packet on the APID of its descriptor"""
        self.apid_obj.deserialize(data, offset=0)
        apid = self.apid_obj.numeric_value & STREAM_ID_APID_MASK
        sequence_count = self.uplink_sequence_counts.get(apid, 0)
        self.uplink_sequence_counts[apid] = (
            sequence_count + 1
        ) % SEQUENCE_COUNT_MODULUS
        return build_command_packet(apid, self.function_code, data, sequence_count)

    def deframe(self, data, no_copy=False):
        """Strip the cFS headers from the next complete telemetry space packet in data

        Returns the F Prime packet, the bytes following it, and any discarded bytes.
        Data shorter than the packet it starts is kept as leftover until more arrives;
        data that does not start with a valid telemetry primary header is discarded.
        """
        if data is None or len(data) < PRIMARY_HEADER_SIZE:
            return None, data if data else b"", b""
        stream_id, sequence, packet_length = parse_primary_header(data)
        if stream_id & STREAM_ID_VERSION_MASK:
            LOGGER.warning("Discarding %d bytes: not a space packet", len(data))
            return None, b"", bytes(data)
        if len(data) < packet_length:
            return None, data, b""
        packet, leftover = bytes(data[:packet_length]), data[packet_length:]
        if stream_id & STREAM_ID_TYPE_COMMAND:
            LOGGER.warning(
                "Discarding command packet on stream id 0x%04X", stream_id
            )
            return None, leftover, packet
        self.check_sequence_count(stream_id & STREAM_ID_APID_MASK, sequence)
        payload_offset = PRIMARY_HEADER_SIZE
        if stream_id & STREAM_ID_SEC_HDR:
            payload_offset += TELEMETRY_SECONDARY_HEADER_SIZE
        if len(packet) < payload_offset:
            LOGGER.warning(
                "Discarding truncated packet on stream id 0x%04X", stream_id
            )
            return None, leftover, packet
        return packet[payload_offset:], leftover, b""

    def check_sequence_count(self, apid: int, sequence: int):
        """Warn when an APID's sequence count skips, indicating dropped packets"""
        count = sequence & SEQUENCE_COUNT_MASK
        expected = self.downlink_sequence_counts.get(apid)
        if expected is not None and count != expected:
            LOGGER.warning(
                "APID 0x%03X received sequence count %d (expected %d)",
                apid,
                count,
                expected,
            )
        self.downlink_sequence_counts[apid] = (count + 1) % SEQUENCE_COUNT_MODULUS

    @classmethod
    def get_name(cls):
        """Name of this framing plugin"""
        return "cfs"

    @classmethod
    @gds_plugin_implementation
    def register_framing_plugin(cls):
        """Register this as a framing plugin"""
        return cls
