"""fprime_cfs.framing: F Prime GDS framing plugin for the cFS software bus packet layout

Selected with `fprime-gds --framing-selection cfs`. Uplink: each F Prime command packet
is wrapped as a complete cFS command packet (CCSDS primary header over the F Prime
command APID, cFS command secondary header with the F Prime command function code and
checksum), ready for CI_LAB to place on the software bus. Downlink: each telemetry
space packet forwarded by TO_LAB has its primary header and 10-byte cFS telemetry
secondary header (time plus the CFE_MSG_TelemetryHeader_t spare bytes) removed, leaving
the bare F Prime packet the GDS decoders expect. This is the ground-side inverse of
FPrimeCfs.CfsCmdFramer / FPrimeCfs.CfsTlmFramer.

The fprime-gds SpacePacketFramerDeframer is not reused: the uplink must place the cFS
command secondary header inside the cFE XOR checksum, and the downlink must drop the
cFE telemetry secondary header and split datagrams that carry several packets.
"""

import logging

from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.common.utils.config_manager import ConfigManager
from fprime_gds.plugin.definitions import gds_plugin_implementation

from fprime_gds.common.models.serialize.type_exceptions import TypeException

from .packets import (
    MAX_PACKET_SIZE,
    PRIMARY_HEADER_SIZE,
    SEQUENCE_FLAGS_UNSEGMENTED,
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
        try:
            self.function_code = config.get_constant(FUNCTION_CODE_CONSTANT)
        except Exception as exc:
            raise ValueError(
                f"framing 'cfs' needs dictionary constant {FUNCTION_CODE_CONSTANT}; "
                "is this an fprime_cfs deployment dictionary?"
            ) from exc
        self.uplink_sequence_counts = {}
        self.downlink_sequence_counts = {}

    def frame(self, data):
        """Wrap an F Prime packet in a cFS command packet on the APID of its descriptor

        A packet whose descriptor is not a dictionary APID is dropped rather than raised, so
        that one bad client packet cannot stop the uplink: an empty frame is returned, which
        CfsLabAdapter.write refuses to send (fprime-gds then reports a failed send).
        """
        try:
            self.apid_obj.deserialize(data, offset=0)
        except TypeException as exc:
            LOGGER.warning("Dropping %d-byte packet with no cFS APID: %s", len(data), exc)
            return b""
        apid = self.apid_obj.numeric_value
        if apid & ~STREAM_ID_APID_MASK:
            LOGGER.warning(
                "Dropping %d-byte packet: descriptor 0x%04X is not a cFS APID", len(data), apid
            )
            return b""
        sequence_count = self.uplink_sequence_counts.get(apid, 0)
        self.uplink_sequence_counts[apid] = (
            sequence_count + 1
        ) % SEQUENCE_COUNT_MODULUS
        return build_command_packet(apid, self.function_code, data, sequence_count)

    def deframe(self, data, no_copy=False):
        """Strip the cFS headers from the next complete telemetry space packet in data

        Returns the F Prime packet, the bytes following it, and any discarded bytes.
        Data shorter than the packet it starts is kept as leftover until more arrives.
        Bytes that are not a valid cFS telemetry packet (bad header, command packet, no
        secondary header, truncated) are discarded and the scan continues within the
        same call, so a good packet behind them is delivered without waiting for more
        data, as fprime-gds's deframe_all expects.
        """
        discarded = b""
        while data is not None and len(data) >= PRIMARY_HEADER_SIZE:
            stream_id, sequence, packet_length = parse_primary_header(data)
            if stream_id & STREAM_ID_VERSION_MASK or packet_length > MAX_PACKET_SIZE:
                skip = self.resync_offset(data)
                LOGGER.warning("Discarding %d bytes: not a valid space packet header", skip)
                discarded += bytes(data[:skip])
                data = data[skip:]
                continue
            if len(data) < packet_length:
                break
            packet, data = bytes(data[:packet_length]), data[packet_length:]
            if stream_id & STREAM_ID_TYPE_COMMAND:
                LOGGER.warning("Discarding command packet on stream id 0x%04X", stream_id)
            elif not stream_id & STREAM_ID_SEC_HDR:
                LOGGER.warning(
                    "Discarding packet without cFS telemetry header on stream id 0x%04X",
                    stream_id,
                )
            elif packet_length < PRIMARY_HEADER_SIZE + TELEMETRY_SECONDARY_HEADER_SIZE:
                LOGGER.warning("Discarding truncated packet on stream id 0x%04X", stream_id)
            else:
                self.check_sequence_count(stream_id & STREAM_ID_APID_MASK, sequence)
                payload = packet[PRIMARY_HEADER_SIZE + TELEMETRY_SECONDARY_HEADER_SIZE :]
                return payload, data, discarded
            discarded += packet
        return None, data if data else b"", discarded

    @staticmethod
    def resync_offset(data) -> int:
        """Offset of the next plausible cFS telemetry header after a bad one, else len(data)

        Plausible means what deframe() accepts: version 0, unsegmented (cFS never segments),
        secondary header present (cFE telemetry always carries one) and a bounded length.
        """
        for offset in range(1, len(data) - PRIMARY_HEADER_SIZE + 1):
            stream_id, sequence, packet_length = parse_primary_header(data, offset)
            if (
                not stream_id & STREAM_ID_VERSION_MASK
                and stream_id & STREAM_ID_SEC_HDR
                and sequence & SEQUENCE_FLAGS_UNSEGMENTED == SEQUENCE_FLAGS_UNSEGMENTED
                and packet_length <= MAX_PACKET_SIZE
            ):
                return offset
        return len(data)

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
