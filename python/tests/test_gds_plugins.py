"""Tests for the F Prime GDS plugins: cFS framing and the CI_LAB/TO_LAB link"""

import struct

import pytest
from fprime_gds.common.utils.config_manager import ConfigManager

from fprime_cfs.framing import FUNCTION_CODE_CONSTANT, CfsFramerDeframer
from fprime_cfs.lab_link import ENABLE_OUTPUT_ATTEMPTS, CfsLabAdapter
from fprime_cfs.packets import (
    build_command_packet,
    command_checksum,
    to_lab_enable_output_packet,
)

FW_PACKET_COMMAND = 0
FW_PACKET_TELEM = 1
FW_PACKET_LOG = 2
# Example F Prime command packet: descriptor, opcode, no arguments
COMMAND_PACKET = struct.pack(">HI", FW_PACKET_COMMAND, 0x500)


@pytest.fixture
def framer():
    ConfigManager().set_constant(FUNCTION_CODE_CONSTANT, 0)
    return CfsFramerDeframer()


def telemetry_packet(apid: int, payload: bytes, sequence: int = 0, sec_hdr=True) -> bytes:
    """Build a cFS telemetry space packet as TO_LAB forwards it"""
    stream_id = apid | (0x0800 if sec_hdr else 0)
    # cFE telemetry secondary header: time, then the 4 spare bytes of CFE_MSG_TelemetryHeader_t
    time_header = struct.pack(">IH", 1234, 5) + bytes(4) if sec_hdr else b""
    length = len(time_header) + len(payload) - 1
    return struct.pack(">HHH", stream_id, 0xC000 | sequence, length) + time_header + payload


def test_command_packet_checksum_and_layout():
    packet = build_command_packet(0x000, 0, COMMAND_PACKET, sequence_count=5)
    # Stream id: command type and secondary header flag over APID 0 (MID 0x1800)
    assert packet[:6] == struct.pack(">HHH", 0x1800, 0xC005, 2 + len(COMMAND_PACKET) - 1)
    assert packet[6] == 0
    # A complete cFS command packet XORs to zero when seeded with 0xFF
    assert command_checksum(packet) == 0
    assert packet[8:] == COMMAND_PACKET


def test_frame_wraps_fprime_command(framer):
    first = framer.frame(COMMAND_PACKET)
    second = framer.frame(COMMAND_PACKET)
    assert first == build_command_packet(FW_PACKET_COMMAND, 0, COMMAND_PACKET, 0)
    assert second == build_command_packet(FW_PACKET_COMMAND, 0, COMMAND_PACKET, 1)


def test_frame_uses_dictionary_function_code():
    ConfigManager().set_constant(FUNCTION_CODE_CONSTANT, 7)
    try:
        packet = CfsFramerDeframer().frame(COMMAND_PACKET)
    finally:
        ConfigManager().set_constant(FUNCTION_CODE_CONSTANT, 0)
    assert packet[6] == 7
    assert command_checksum(packet) == 0


def test_deframe_strips_telemetry_headers(framer):
    payload = struct.pack(">HI", FW_PACKET_LOG, 0x01000003) + b"event-args"
    packet, leftover, discarded = framer.deframe(telemetry_packet(FW_PACKET_LOG, payload))
    assert packet == payload
    assert leftover == b""
    assert discarded == b""


def test_deframe_without_secondary_header(framer):
    payload = struct.pack(">HI", FW_PACKET_TELEM, 42)
    packet, leftover, _ = framer.deframe(telemetry_packet(FW_PACKET_TELEM, payload, sec_hdr=False))
    assert packet == payload
    assert leftover == b""


def test_deframe_all_splits_concatenated_datagrams(framer):
    first = struct.pack(">HI", FW_PACKET_TELEM, 1)
    second = struct.pack(">HI", FW_PACKET_LOG, 2) + b"x"
    data = telemetry_packet(FW_PACKET_TELEM, first) + telemetry_packet(FW_PACKET_LOG, second)
    packets, leftover, discarded = framer.deframe_all(data, no_copy=False)
    assert packets == [first, second]
    assert leftover == b""
    assert discarded == b""


def test_deframe_waits_for_partial_packet(framer):
    full = telemetry_packet(FW_PACKET_TELEM, b"\x00\x01\x00\x00\x00\x2a")
    partial = full[:14]
    packet, leftover, discarded = framer.deframe(partial)
    assert packet is None
    assert leftover == partial
    assert discarded == b""
    packet, leftover, _ = framer.deframe(partial + full[14:])
    assert packet == b"\x00\x01\x00\x00\x00\x2a"


def test_deframe_discards_command_packets(framer):
    command = build_command_packet(0, 0, COMMAND_PACKET)
    packet, leftover, discarded = framer.deframe(command)
    assert packet is None
    assert leftover == b""
    assert discarded == command


def test_deframe_discards_non_space_packet(framer):
    garbage = b"\xff" * 12
    packet, leftover, discarded = framer.deframe(garbage)
    assert packet is None
    assert leftover == b""
    assert discarded == garbage


def test_deframe_warns_on_sequence_gap(framer, caplog):
    payload = struct.pack(">HI", FW_PACKET_TELEM, 1)
    framer.deframe(telemetry_packet(FW_PACKET_TELEM, payload, sequence=3))
    framer.deframe(telemetry_packet(FW_PACKET_TELEM, payload, sequence=4))
    assert "expected" not in caplog.text
    framer.deframe(telemetry_packet(FW_PACKET_TELEM, payload, sequence=9))
    assert "received sequence count 9 (expected 5)" in caplog.text


def test_enable_output_packet():
    packet = to_lab_enable_output_packet("10.0.0.5", 0x1880)
    assert packet[:6] == bytes.fromhex("1880c0000011")
    assert packet[6] == 6
    assert command_checksum(packet) == 0
    assert packet[8:] == b"10.0.0.5".ljust(16, b"\x00")


def test_enable_output_destination_too_long():
    with pytest.raises(ValueError, match="exceeds 15 characters"):
        to_lab_enable_output_packet("0123456789abcdef")


class FakeSocket:
    def __init__(self):
        self.sent = []

    def sendto(self, packet, target):
        self.sent.append((packet, target))


def make_adapter(enable=True, interval=30.0):
    adapter = CfsLabAdapter("127.0.0.1", 1234, 2234, "127.0.0.1", 0x1880, enable, interval)
    adapter.send_socket = FakeSocket()
    return adapter


def test_adapter_enables_output_on_open(monkeypatch):
    adapter = make_adapter()
    monkeypatch.setattr(CfsLabAdapter.__mro__[1], "open", lambda self: None)
    adapter.open()
    assert len(adapter.send_socket.sent) == ENABLE_OUTPUT_ATTEMPTS
    packet, target = adapter.send_socket.sent[0]
    assert target == ("127.0.0.1", 1234)
    assert packet == to_lab_enable_output_packet("127.0.0.1", 0x1880)


def test_adapter_repeats_enable_only_while_silent(monkeypatch):
    adapter = make_adapter()
    reads = [b"", b"", b"data", b""]
    monkeypatch.setattr(CfsLabAdapter.__mro__[1], "read", lambda self, timeout=0.5: reads.pop(0))
    clock = [100.0]
    monkeypatch.setattr("fprime_cfs.lab_link.time.monotonic", lambda: clock[0])
    adapter.read()  # never enabled, silent: one datagram
    assert len(adapter.send_socket.sent) == 1
    clock[0] += 1.0
    adapter.read()  # too soon after the last attempt: no resend
    assert len(adapter.send_socket.sent) == 1
    clock[0] += 40.0
    adapter.read()  # telemetry arrives: no resend
    assert len(adapter.send_socket.sent) == 1
    clock[0] += 40.0
    adapter.read()  # silent again for the interval: resend
    assert len(adapter.send_socket.sent) == 2


def test_adapter_zero_interval_never_repeats(monkeypatch):
    adapter = make_adapter(interval=0)
    monkeypatch.setattr(CfsLabAdapter.__mro__[1], "read", lambda self, timeout=0.5: b"")
    clock = [100.0]
    monkeypatch.setattr("fprime_cfs.lab_link.time.monotonic", lambda: clock[0])
    adapter.read()
    clock[0] += 1000.0
    adapter.read()
    assert adapter.send_socket.sent == []


def test_adapter_disabled_enable_sends_nothing(monkeypatch):
    adapter = make_adapter(enable=False)
    monkeypatch.setattr(CfsLabAdapter.__mro__[1], "open", lambda self: None)
    adapter.open()
    assert adapter.send_socket.sent == []


def test_adapter_check_arguments_rejects_bad_destination():
    with pytest.raises(ValueError):
        CfsLabAdapter.check_arguments("127.0.0.1", 1234, 2234, "0123456789abcdef", 0x1880, True, 30.0)
    CfsLabAdapter.check_arguments("127.0.0.1", 1234, 2234, "0123456789abcdef", 0x1880, False, 30.0)
    with pytest.raises(ValueError):
        CfsLabAdapter.check_arguments("127.0.0.1", 1234, 2234, "127.0.0.1", 0x1880, True, -1.0)
