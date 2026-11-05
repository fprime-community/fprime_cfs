"""fprime_cfs.packets: cFS software bus packet layouts shared by the ground tooling

Messages on the cFS software bus are CCSDS space packets. Commands carry the cFS command
secondary header (function code, checksum); telemetry carries the cFS telemetry secondary
header (big-endian 32-bit seconds, 16-bit subseconds) followed by the 4 spare bytes that
pad `CFE_MSG_TelemetryHeader_t` to 16 bytes, as `FPrimeCfs.CfsTlmFramer` emits. The
helpers here build and read those layouts for the lab link (CI_LAB/TO_LAB over UDP) used by
the GDS tooling.
"""

import ipaddress
import struct
from typing import Tuple

# Space packet stream identifier bits (CCSDS primary header, big-endian 16 bits)
STREAM_ID_VERSION_MASK = 0xE000
STREAM_ID_TYPE_COMMAND = 0x1000
STREAM_ID_SEC_HDR = 0x0800
STREAM_ID_APID_MASK = 0x07FF
# Sequence control: unsegmented packet flags over the 14-bit sequence count
SEQUENCE_FLAGS_UNSEGMENTED = 0xC000
SEQUENCE_COUNT_MASK = 0x3FFF
SEQUENCE_COUNT_MODULUS = SEQUENCE_COUNT_MASK + 1

# Sanity bound on the announced packet length: the cFS default CFE_MISSION_SB_MAX_SB_MSG_SIZE.
# A mission raising that limit above 32 KiB must raise this bound too.
MAX_PACKET_SIZE = 32768

PRIMARY_HEADER_FORMAT = ">HHH"
PRIMARY_HEADER_SIZE = struct.calcsize(PRIMARY_HEADER_FORMAT)
COMMAND_SECONDARY_HEADER_SIZE = 2
# 6-byte time plus the 4 spare bytes of CFE_MSG_TelemetryHeader_t
TELEMETRY_SECONDARY_HEADER_SIZE = 10

# TO_LAB command interface (shipped with the cFS GroundSystem checkout)
TO_LAB_DEFAULT_MESSAGE_ID = 0x1880
TO_LAB_OUTPUT_ENABLE_CC = 6
# TO_LAB enable-output payload: a 16-character destination IP string
TO_LAB_DEST_IP_SIZE = 16


def command_checksum(packet: bytes) -> int:
    """Compute the cFS command checksum byte for a packet whose checksum byte is zero

    cFS (CFE_MSG) requires the XOR of every byte of the complete command packet, seeded
    with 0xFF, to equal zero; the returned byte, stored in the checksum position, makes
    it so.
    """
    checksum = 0xFF
    for byte in packet:
        checksum ^= byte
    return checksum


def build_command_packet(
    apid: int, function_code: int, payload: bytes, sequence_count: int = 0
) -> bytes:
    """Build a complete cFS command packet around the supplied payload

    The stream identifier sets the command type bit and the secondary header flag over
    the APID; the sequence control marks an unsegmented packet.
    """
    stream_id = (apid & STREAM_ID_APID_MASK) | STREAM_ID_TYPE_COMMAND | STREAM_ID_SEC_HDR
    sequence = SEQUENCE_FLAGS_UNSEGMENTED | (sequence_count & SEQUENCE_COUNT_MASK)
    # The length field holds the number of bytes following the primary header, minus one
    length = COMMAND_SECONDARY_HEADER_SIZE + len(payload) - 1
    header = struct.pack(PRIMARY_HEADER_FORMAT, stream_id, sequence, length)
    secondary = bytearray([function_code & 0xFF, 0])
    secondary[1] = command_checksum(header + bytes(secondary) + payload)
    return header + bytes(secondary) + payload


def to_lab_enable_output_packet(
    destination: str, message_id: int = TO_LAB_DEFAULT_MESSAGE_ID
) -> bytes:
    """Build the TO_LAB enable-output command carrying the telemetry destination IP

    The packet is exactly what the cFS GroundSystem's "Enable Tlm" quick button sends.
    TO_LAB consumes the destination as a NUL-terminated string within a fixed-size field.
    """
    try:
        ipaddress.IPv4Address(destination)
    except ValueError as exc:
        raise ValueError(
            f"Telemetry destination '{destination}' is not a dotted-quad IPv4 address; "
            "TO_LAB accepts the string unchecked, reports output enabled, and sends to an "
            "unresolved address"
        ) from exc
    payload = destination.encode("ascii").ljust(TO_LAB_DEST_IP_SIZE, b"\x00")
    return build_command_packet(
        message_id & STREAM_ID_APID_MASK, TO_LAB_OUTPUT_ENABLE_CC, payload
    )


def parse_primary_header(data: bytes, offset: int = 0) -> Tuple[int, int, int]:
    """Unpack the CCSDS primary header at offset within data

    Returns the stream identifier, the sequence control word, and the total packet
    length in bytes (primary header included).
    """
    stream_id, sequence, length = struct.unpack_from(PRIMARY_HEADER_FORMAT, data, offset)
    return stream_id, sequence, PRIMARY_HEADER_SIZE + length + 1
