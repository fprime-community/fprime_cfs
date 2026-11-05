"""Tests for the fprime_cfs configuration generators"""

import copy
import json
import pickle
from types import SimpleNamespace

import pytest

import fprime_cfs.__main__ as runner
from fprime_cfs.__main__ import enable_telemetry, require_ground_system_gui
from fprime_cfs.commands import generate_commands
from fprime_cfs.dictionary import Dictionary, DictionaryError
from fprime_cfs.generate import generate
from fprime_cfs.telemetry import TLM_GUI_MAX_ROWS, generate_telemetry

BASE_DICTIONARY = {
    "metadata": {"deploymentName": "Test"},
    "typeDefinitions": [
        {
            "kind": "alias",
            "qualifiedName": "FwPacketDescriptorType",
            "type": {"name": "U16", "kind": "integer", "size": 16, "signed": False},
            "underlyingType": {
                "name": "U16",
                "kind": "integer",
                "size": 16,
                "signed": False,
            },
        },
        {
            "kind": "alias",
            "qualifiedName": "FwTlmPacketizeIdType",
            "type": {"name": "U16", "kind": "integer", "size": 16, "signed": False},
            "underlyingType": {
                "name": "U16",
                "kind": "integer",
                "size": 16,
                "signed": False,
            },
        },
        {
            "kind": "alias",
            "qualifiedName": "FwOpcodeType",
            "type": {"name": "FwIdType", "kind": "qualifiedIdentifier"},
            "underlyingType": {
                "name": "U32",
                "kind": "integer",
                "size": 32,
                "signed": False,
            },
        },
        {
            "kind": "enum",
            "qualifiedName": "ComCfg.Apid",
            "representationType": {
                "name": "U16",
                "kind": "integer",
                "size": 16,
                "signed": False,
            },
            "enumeratedConstants": [
                {"name": "FW_PACKET_COMMAND", "value": 0},
                {"name": "FW_PACKET_PACKETIZED_TLM", "value": 4},
            ],
        },
        {
            "kind": "enum",
            "qualifiedName": "Test.Mode",
            "representationType": {
                "name": "U8",
                "kind": "integer",
                "size": 8,
                "signed": False,
            },
            "enumeratedConstants": [
                {"name": "OFF", "value": 0},
                {"name": "ON", "value": 1},
            ],
        },
    ],
    "constants": [
        {"kind": "constant", "qualifiedName": "ComCfg.SpacecraftId", "value": 68},
        {"kind": "constant", "qualifiedName": "ComCfg.TmFrameFixedSize", "value": 1024},
        {
            "kind": "constant",
            "qualifiedName": "ComCfg.FprimeCommandFunctionCode",
            "value": 0,
        },
    ],
    "telemetryChannels": [
        {
            "name": "comp.counter",
            "type": {"name": "U32", "kind": "integer", "size": 32, "signed": False},
            "id": 1,
        },
        {
            "name": "comp.flag",
            "type": {"name": "U8", "kind": "integer", "size": 8, "signed": False},
            "id": 2,
        },
        {
            "name": "comp.mode",
            "type": {"name": "Test.Mode", "kind": "qualifiedIdentifier"},
            "id": 3,
        },
    ],
    "commands": [
        {
            "name": "comp.NO_OP",
            "commandKind": "async",
            "opcode": 256,
            "formalParams": [],
        },
        {
            "name": "comp.SET_MODE",
            "commandKind": "async",
            "opcode": 257,
            "formalParams": [
                {
                    "name": "mode",
                    "type": {"name": "Test.Mode", "kind": "qualifiedIdentifier"},
                },
            ],
        },
        {
            "name": "comp.NO_OP_STRING",
            "commandKind": "async",
            "opcode": 258,
            "formalParams": [
                {
                    "name": "text",
                    "type": {"name": "string", "kind": "string", "size": 40},
                },
            ],
        },
    ],
    "telemetryPacketSets": [
        {
            "name": "TestPackets",
            "members": [
                {
                    "name": "Health",
                    "id": 1,
                    "group": 1,
                    "members": ["comp.counter", "comp.flag", "comp.mode"],
                },
            ],
        },
    ],
}


def make_dictionary(tmp_path, data):
    path = tmp_path / "dictionary.json"
    path.write_text(json.dumps(data))
    return Dictionary(path)


def test_telemetry_generation(tmp_path):
    dictionary = make_dictionary(tmp_path, BASE_DICTIONARY)
    stream_id = generate_telemetry(dictionary, tmp_path)
    assert stream_id == 0x0804

    pages = (tmp_path / "telemetry-pages.txt").read_text().splitlines()
    data_lines = [line for line in pages if not line.startswith("#")]
    assert data_lines == ["Health, GenericTelemetry.py, 0x0804, fprime-tlm.txt"]

    rows = [
        [column.strip() for column in line.split(",")]
        for line in (tmp_path / "fprime-tlm.txt").read_text().splitlines()
        if not line.startswith("#")
    ]
    # Datagram layout: 6 header + 10 cFS sec header (6 time + 4 spare) + 2 descriptor + 2 id + 11 time +
    # values, minus the 4-byte offset applied by the GUI
    assert rows[0][:4] == ["Packet Id", "14", "2", ">H"]
    assert rows[1][:4] == ["Time Seconds", "19", "4", ">I"]
    assert rows[2][:4] == ["Time Microseconds", "23", "4", ">I"]
    assert rows[3][:5] == ["comp.counter", "27", "4", ">I", "Dec"]
    assert rows[4][:5] == ["comp.flag", "31", "1", "B", "Dec"]
    assert rows[5][:9] == [
        "comp.mode",
        "32",
        "1",
        "B",
        "Enm",
        "OFF",
        "ON",
        "NULL",
        "NULL",
    ]


def test_multiple_packets_error(tmp_path):
    data = copy.deepcopy(BASE_DICTIONARY)
    data["telemetryPacketSets"][0]["members"].append(
        {"name": "Extra", "id": 2, "group": 1, "members": []}
    )
    dictionary = make_dictionary(tmp_path, data)
    with pytest.raises(DictionaryError, match="exactly one telemetry packet"):
        generate_telemetry(dictionary, tmp_path)


def test_no_packets_error(tmp_path):
    data = copy.deepcopy(BASE_DICTIONARY)
    data["telemetryPacketSets"] = []
    dictionary = make_dictionary(tmp_path, data)
    with pytest.raises(DictionaryError, match="exactly one telemetry packet"):
        generate_telemetry(dictionary, tmp_path)


def test_command_generation(tmp_path):
    dictionary = make_dictionary(tmp_path, BASE_DICTIONARY)
    message_id, skipped = generate_commands(dictionary, tmp_path)
    assert message_id == 0x1800
    assert len(skipped) == 1 and "comp.NO_OP_STRING" in skipped[0]

    pages = [
        line
        for line in (tmp_path / "command-pages.txt").read_text().splitlines()
        if not line.startswith("#")
    ]
    assert pages == [
        "F Prime Commands, fprime_cmds, 0x1800, BE, UdpCommands.py, 127.0.0.1, 1234",
        "Telemetry Output, TO_LAB_CMD, 0x1880, LE, UdpCommands.py, 127.0.0.1, 1234",
    ]

    buttons = [
        line
        for line in (tmp_path / "quick-buttons.txt").read_text().splitlines()
        if not line.startswith("#")
    ]
    assert buttons == [
        "Telemetry Output, TO_LAB_CMD, Enable Tlm, 6, 0x1880, LE, "
        "127.0.0.1, 1234, TO_LAB_OUTPUT_ENABLE_CC"
    ]

    with (tmp_path / "CommandFiles" / "fprime_cmds").open("rb") as file_handle:
        descriptions, codes, param_files = pickle.load(file_handle)
    assert descriptions == ["comp.NO_OP", "comp.SET_MODE"]
    # Every command carries the single dictionary function code
    assert codes == ["0", "0"]

    with (tmp_path / "ParameterFiles" / param_files[1]).open("rb") as file_handle:
        data_types, names, _, descriptions, flags, string_lengths = pickle.load(
            file_handle
        )
    # The framing descriptor and opcode are prepended ahead of the command arguments
    assert names == ["FramingDescriptor", "Opcode", "mode"]
    assert flags == ["--uint16", "--uint32", "--uint8"]
    assert descriptions[0].startswith("Enter 0 ")
    assert descriptions[1].startswith("Enter 257 (0x101)")
    assert "OFF=0; ON=1" in descriptions[2].replace(",", ";")


def test_missing_function_code_error(tmp_path):
    data = copy.deepcopy(BASE_DICTIONARY)
    data["constants"] = [
        c
        for c in data["constants"]
        if c["qualifiedName"] != "ComCfg.FprimeCommandFunctionCode"
    ]
    dictionary = make_dictionary(tmp_path, data)
    with pytest.raises(DictionaryError, match="FprimeCommandFunctionCode"):
        generate_commands(dictionary, tmp_path)


def test_too_many_rows_error(tmp_path):
    data = copy.deepcopy(BASE_DICTIONARY)
    for index in range(TLM_GUI_MAX_ROWS):
        name = f"comp.extra{index}"
        data["telemetryChannels"].append(
            {
                "name": name,
                "type": {"name": "U8", "kind": "integer", "size": 8, "signed": False},
                "id": 100 + index,
            }
        )
        data["telemetryPacketSets"][0]["members"][0]["members"].append(name)
    dictionary = make_dictionary(tmp_path, data)
    with pytest.raises(DictionaryError, match="display rows"):
        generate_telemetry(dictionary, tmp_path)


def test_enum_fallback_display(tmp_path):
    # Enum values outside the 0-3 tlmGUI slots fall back to decimal display
    data = copy.deepcopy(BASE_DICTIONARY)
    for definition in data["typeDefinitions"]:
        if definition["qualifiedName"] == "Test.Mode":
            definition["enumeratedConstants"].append({"name": "SPECIAL", "value": 4})
    dictionary = make_dictionary(tmp_path, data)
    generate_telemetry(dictionary, tmp_path)
    rows = [
        [column.strip() for column in line.split(",")]
        for line in (tmp_path / "fprime-tlm.txt").read_text().splitlines()
        if not line.startswith("#")
    ]
    assert rows[5][:5] == ["comp.mode", "32", "1", "B", "Dec"]


def test_float_and_bool_parameters(tmp_path):
    data = copy.deepcopy(BASE_DICTIONARY)
    data["commands"].append(
        {
            "name": "comp.SET_GAIN",
            "commandKind": "async",
            "opcode": 259,
            "formalParams": [
                {
                    "name": "gain",
                    "type": {"name": "F32", "kind": "float", "size": 32},
                },
                {
                    "name": "enable",
                    "type": {"name": "bool", "kind": "bool", "size": 8},
                },
            ],
        }
    )
    dictionary = make_dictionary(tmp_path, data)
    generate_commands(dictionary, tmp_path)
    with (tmp_path / "ParameterFiles" / "fprime_comp_SET_GAIN").open(
        "rb"
    ) as file_handle:
        _, names, _, descriptions, flags, _ = pickle.load(file_handle)
    assert names == ["FramingDescriptor", "Opcode", "gain", "enable"]
    # Floats are entered as raw IEEE-754 bits; bools as 0x00/0xFF bytes
    assert flags[2:] == ["--uint32", "--uint8"]
    assert "IEEE-754" in descriptions[2]
    assert "255 for TRUE" in descriptions[3]


def test_command_port_plumbing(tmp_path):
    dictionary = make_dictionary(tmp_path, BASE_DICTIONARY)
    ground_system = tmp_path / "cFS-GroundSystem"
    (ground_system / "Subsystems" / "tlmGUI").mkdir(parents=True)
    (ground_system / "Subsystems" / "cmdGui").mkdir(parents=True)
    generate(dictionary, ground_system, command_host="10.0.0.5", command_port=5555)
    pages = [
        line
        for line in (ground_system / "Subsystems" / "cmdGui" / "command-pages.txt")
        .read_text()
        .splitlines()
        if not line.startswith("#")
    ]
    assert pages == [
        "F Prime Commands, fprime_cmds, 0x1800, BE, UdpCommands.py, 10.0.0.5, 5555",
        "Telemetry Output, TO_LAB_CMD, 0x1880, LE, UdpCommands.py, 10.0.0.5, 5555",
    ]


def test_generate_missing_ground_system_error(tmp_path):
    dictionary = make_dictionary(tmp_path, BASE_DICTIONARY)
    with pytest.raises(DictionaryError, match="cFS-GroundSystem checkout"):
        generate(dictionary, tmp_path / "nonexistent")


def test_enable_telemetry_packet(monkeypatch):
    sent = []

    class FakeSocket:
        def __enter__(self):
            return self

        def __exit__(self, *exc):
            return False

        def sendto(self, packet, target):
            sent.append((packet, target))

    monkeypatch.setattr(runner.socket, "socket", lambda *args, **kwargs: FakeSocket())
    args = SimpleNamespace(
        telemetry_destination="127.0.0.1",
        to_lab_message_id=0x1880,
        command_address="127.0.0.1",
        command_port=1234,
    )
    enable_telemetry(args)
    assert len(sent) == 1
    packet, target = sent[0]
    assert target == ("127.0.0.1", 1234)
    # CCSDS primary header: TO_LAB command MID, sec-header flag set, length = payload + 1
    assert packet[:6] == bytes.fromhex("1880c0000011")
    # Command secondary header: OUTPUT_ENABLE function code, then XOR checksum
    assert packet[6] == 6
    checksum = 0xFF
    for byte in packet[:7] + packet[8:]:
        checksum ^= byte
    assert packet[7] == checksum
    # Payload: 16-byte destination IP string
    assert packet[8:] == b"127.0.0.1".ljust(16, b"\x00")


def test_enable_telemetry_destination_too_long():
    args = SimpleNamespace(
        telemetry_destination="0123456789abcdef",  # 16 chars: no room for NUL terminator
        to_lab_message_id=0x1880,
        command_address="127.0.0.1",
        command_port=1234,
    )
    with pytest.raises(ValueError, match="not a dotted-quad IPv4"):
        enable_telemetry(args)


def test_require_ground_system_gui_names_the_extra(monkeypatch):
    monkeypatch.setattr("importlib.util.find_spec", lambda name: None)
    with pytest.raises(RuntimeError, match=r"fprime-cfs\[groundsystem\]"):
        require_ground_system_gui()
