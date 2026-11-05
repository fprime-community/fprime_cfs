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


def test_deframe_discards_packet_without_secondary_header(framer, caplog):
    raw = telemetry_packet(FW_PACKET_TELEM, struct.pack(">HI", FW_PACKET_TELEM, 42), sec_hdr=False)
    packet, leftover, discarded = framer.deframe(raw)
    assert packet is None and leftover == b"" and discarded == raw
    assert "without cFS telemetry header" in caplog.text


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


def test_deframe_discards_oversized_length_and_resyncs(framer):
    good = telemetry_packet(0x0802, b"\x55" * 4, 1)
    bogus = struct.pack(">HHH", 0x0802, 0xC000, 0xFFFF) + b"\x00" * 10
    packet, leftover, discarded = framer.deframe(bogus + good)
    assert packet == b"\x55" * 4  # delivered in the same call, not held for the next read
    assert discarded == bogus
    assert leftover == b""


def test_adapter_write_rejects_empty_frame():
    adapter = make_adapter()
    assert adapter.write(b"") is False
    assert adapter.send_socket.sent == []
    assert adapter.write(b"\x01") is True


def test_frame_drops_packet_without_dictionary_apid(framer, caplog):
    assert framer.frame(b"\xff\xff\x01") == b""
    assert framer.frame(b"\x00") == b""
    assert "no cFS APID" in caplog.text
    assert framer.frame(b"\x08\x00\x01") == b""  # ComCfg.Apid.INVALID_UNINITIALIZED
    assert len(caplog.records) == 3


def test_framer_rejects_dictionary_without_function_code(monkeypatch):
    def missing(self, name):
        raise KeyError(name)

    monkeypatch.setattr(ConfigManager, "get_constant", missing)
    with pytest.raises(ValueError, match=FUNCTION_CODE_CONSTANT):
        CfsFramerDeframer()


def test_enable_output_destination_must_be_ipv4():
    with pytest.raises(ValueError):
        to_lab_enable_output_packet("gds-host.local")


def test_deframe_discards_short_secondary_header(framer, caplog):
    short = struct.pack(">HHH", 0x0802, 0xC000, 2) + b"\x00" * 3
    packet, leftover, discarded = framer.deframe(short)
    assert packet is None and leftover == b"" and discarded == short
    assert "truncated" in caplog.text


def test_sequence_counts_wrap_at_14_bits(framer, caplog):
    framer.uplink_sequence_counts[0] = 0x3FFF
    assert struct.unpack(">H", framer.frame(b"\x00\x00\x01")[2:4])[0] == 0xFFFF
    assert framer.frame(b"\x00\x00\x01")[2:4] == b"\xc0\x00"  # wrapped to 0
    framer.deframe(telemetry_packet(0x0802, b"\x01", 0x3FFF))
    framer.deframe(telemetry_packet(0x0802, b"\x01", 0))
    assert "sequence" not in caplog.text.lower()


def test_frame_uses_nonzero_descriptor_apid(framer):
    framed = framer.frame(b"\x00\x03" + b"\xaa" * 4)  # FW_PACKET_FILE = 3
    assert struct.unpack(">H", framed[:2])[0] == 0x1803


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
    with pytest.raises(ValueError, match="not a dotted-quad IPv4"):
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


@pytest.fixture
def clock(monkeypatch):
    now = [100.0]
    monkeypatch.setattr("fprime_cfs.lab_link.time.monotonic", lambda: now[0])
    return now


def stubbed_adapter(monkeypatch, reads=None, **kwargs):
    """Adapter whose UDP base class neither opens sockets nor blocks in read()"""
    adapter = make_adapter(**kwargs)
    monkeypatch.setattr(CfsLabAdapter.__mro__[1], "open", lambda self: None)
    monkeypatch.setattr(
        CfsLabAdapter.__mro__[1],
        "read",
        lambda self, timeout=0.5: reads.pop(0) if reads else b"",
    )
    return adapter


def sent(adapter):
    return len(adapter.send_socket.sent)


def test_adapter_enables_output_on_open(monkeypatch):
    adapter = stubbed_adapter(monkeypatch)
    adapter.open()
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS
    packet, target = adapter.send_socket.sent[0]
    assert target == ("127.0.0.1", 1234)
    assert packet == to_lab_enable_output_packet("127.0.0.1", 0x1880)


def test_adapter_retries_every_second_until_first_telemetry(monkeypatch, clock):
    adapter = stubbed_adapter(monkeypatch, reads=[b"", b"", b"", b"data", b"", b""])
    adapter.open()
    clock[0] += 0.5
    adapter.read()  # too soon after open(): no resend
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS
    clock[0] += 0.5
    adapter.read()  # one second of silence after open(): resend
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS + 1
    clock[0] += 1.0
    adapter.read()  # still nothing: resend again
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS + 2
    clock[0] += 1.0
    assert adapter.read() == b"data"  # first telemetry: back off to the interval
    clock[0] += 29.0
    adapter.read()
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS + 2
    clock[0] += 1.0
    adapter.read()  # a full interval of silence: resend
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS + 3


def test_adapter_repeats_enable_only_while_silent(monkeypatch, clock):
    adapter = stubbed_adapter(monkeypatch, reads=[b"data", b"", b"data", b""])
    assert adapter.read() == b"data"  # telemetry before any enable: nothing sent
    assert sent(adapter) == 0
    clock[0] += 40.0
    adapter.read()  # silent for the interval: one datagram
    assert sent(adapter) == 1
    clock[0] += 1.0
    assert adapter.read() == b"data"  # telemetry arrives unchanged: no resend
    clock[0] += 1.0
    adapter.read()  # recent telemetry: no resend
    assert sent(adapter) == 1


def test_adapter_zero_interval_never_repeats(monkeypatch, clock):
    adapter = stubbed_adapter(monkeypatch, interval=0)
    adapter.open()
    adapter.read()
    clock[0] += 1000.0
    adapter.read()
    assert sent(adapter) == ENABLE_OUTPUT_ATTEMPTS


def test_adapter_disabled_enable_sends_nothing(monkeypatch, clock):
    adapter = stubbed_adapter(monkeypatch, enable=False)
    adapter.open()
    clock[0] += 1000.0
    adapter.read()
    assert sent(adapter) == 0


def test_adapter_write_rejects_empty_frame():
    adapter = make_adapter()
    assert adapter.write(b"") is False
    assert adapter.send_socket.sent == []
    assert adapter.write(b"\x01") is True


def test_adapter_check_arguments_rejects_bad_destination():
    with pytest.raises(ValueError):
        CfsLabAdapter.check_arguments("127.0.0.1", 1234, 2234, "0123456789abcdef", 0x1880, True, 30.0)
    CfsLabAdapter.check_arguments("127.0.0.1", 1234, 2234, "0123456789abcdef", 0x1880, False, 30.0)
    with pytest.raises(ValueError):
        CfsLabAdapter.check_arguments("127.0.0.1", 1234, 2234, "127.0.0.1", 0x1880, True, -1.0)
