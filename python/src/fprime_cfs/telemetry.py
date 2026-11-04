"""fprime_cfs.telemetry: cFS GroundSystem telemetry configuration generation

Generates the tlmGUI configuration (telemetry-pages.txt and a packet definition file) for
the single fixed F Prime telemetry packet.

The datagram delivered to the cFS GroundSystem is the space packet forwarded from the
cFS software bus by the TO_LAB telemetry output application. F Prime telemetry framing
(FPrimeCfs.CfsTlmFramer) inserts a cFS telemetry secondary header, so the packet is:

    +--------------------+-------------------+------------+--------+------------+------------- - - -
    | primary header (6) | cFS sec header (10)| descriptor | pkt id | time (11)  | channel values...
    +--------------------+-------------------+------------+--------+------------+------------- - - -

Offsets written to the definition file are relative offsets: the tlmGUI adds the
telemetry header offset from the main GroundSystem window (4 by default) to every item,
so 4 is subtracted from each real offset here.

F Prime serializes values big-endian while the tlmGUI launches pages in little-endian
mode. The tlmGUI strips a leading "<" from the item format it builds (endian prefix plus
the definition file type column) before calling struct.unpack, so multi-byte items use an
explicit ">" prefix in the type column (e.g. ">I") to force big-endian decoding.
"""

from pathlib import Path
from typing import List, Tuple

from .dictionary import Dictionary, DictionaryError, ResolvedType

# CCSDS space packet primary header size in bytes
SPACE_PACKET_HEADER_SIZE = 6
# cFS telemetry secondary header size in bytes: 4-byte seconds, 2-byte subseconds, then the
# 4 spare bytes of CFE_MSG_TelemetryHeader_t (cFE pads the header to 16 bytes)
CFS_TLM_SEC_HDR_SIZE = 10
# Space packet stream identifier secondary header flag (big-endian 16 bits)
STREAM_ID_SEC_HDR = 0x0800
# Fw.Time serialized size in bytes: U16 base, U8 context, U32 seconds, U32 microseconds
TIME_SIZE = 11
# Telemetry header offset the GroundSystem main window applies to every item (version 1)
TLM_GUI_GLOBAL_OFFSET = 4
# The tlmGUI enumeration display supports exactly four slots, indexed by raw value 0-3
TLM_GUI_ENUM_SLOTS = 4
# The tlmGUI preallocates storage for at most 40 display rows
TLM_GUI_MAX_ROWS = 40

PACKET_DEFINITION_FILE = "fprime-tlm.txt"
TELEMETRY_PAGES_FILE = "telemetry-pages.txt"

_INTEGER_FORMATS = {
    (1, False): "B",
    (1, True): "b",
    (2, False): "H",
    (2, True): "h",
    (4, False): "I",
    (4, True): "i",
    (8, False): "Q",
    (8, True): "q",
}
_FLOAT_FORMATS = {4: "f", 8: "d"}


def struct_format(resolved: ResolvedType) -> str:
    """Python struct format character for a resolved primitive type"""
    if resolved.kind in ("integer", "enum"):
        try:
            return _INTEGER_FORMATS[(resolved.size, resolved.signed)]
        except KeyError:
            raise DictionaryError(
                f"No struct format for {resolved.size}-byte integer '{resolved.name}'"
            )
    if resolved.kind == "float":
        try:
            return _FLOAT_FORMATS[resolved.size]
        except KeyError:
            raise DictionaryError(
                f"No struct format for {resolved.size}-byte float '{resolved.name}'"
            )
    if resolved.kind == "bool":
        return "B"
    raise DictionaryError(
        f"No struct format for type '{resolved.name}' of kind '{resolved.kind}'"
    )


def _type_column(resolved: ResolvedType) -> str:
    """Definition file type column: big-endian prefix for multi-byte items"""
    fmt = struct_format(resolved)
    return fmt if resolved.size == 1 else f">{fmt}"


def _display_columns(resolved: ResolvedType) -> Tuple[str, List[str]]:
    """Display type and the four enumeration display slots for a resolved type"""
    enums = ["NULL"] * TLM_GUI_ENUM_SLOTS
    if resolved.kind == "enum" and resolved.enum_constants:
        values = list(resolved.enum_constants.values())
        if all(0 <= value < TLM_GUI_ENUM_SLOTS for value in values):
            for name, value in resolved.enum_constants.items():
                enums[value] = name
            return "Enm", enums
    return "Dec", enums


def generate_telemetry(dictionary: Dictionary, tlm_gui_dir: Path) -> int:
    """Generate tlmGUI configuration for the single F Prime telemetry packet

    Returns the stream identifier (packet id) of the generated telemetry page.
    """
    packet = dictionary.get_packet()
    descriptor_size = dictionary.resolve_definition(
        dictionary.get_type_definition("FwPacketDescriptorType")
    ).size
    packet_id_type = dictionary.resolve_definition(
        dictionary.get_type_definition("FwTlmPacketizeIdType")
    )

    rows = []
    offset = SPACE_PACKET_HEADER_SIZE + CFS_TLM_SEC_HDR_SIZE + descriptor_size
    rows.append(
        (
            "Packet Id",
            offset,
            packet_id_type.size,
            _type_column(packet_id_type),
            "Dec",
            ["NULL"] * TLM_GUI_ENUM_SLOTS,
        )
    )
    offset += packet_id_type.size
    # Fw.Time: U16 time base, U8 time context, U32 seconds, U32 microseconds
    rows.append(
        ("Time Seconds", offset + 3, 4, ">I", "Dec", ["NULL"] * TLM_GUI_ENUM_SLOTS)
    )
    rows.append(
        (
            "Time Microseconds",
            offset + 7,
            4,
            ">I",
            "Dec",
            ["NULL"] * TLM_GUI_ENUM_SLOTS,
        )
    )
    offset += TIME_SIZE

    for channel_name in packet.members:
        resolved = dictionary.get_channel_type(channel_name)
        display, enums = _display_columns(resolved)
        rows.append(
            (
                channel_name,
                offset,
                resolved.size,
                _type_column(resolved),
                display,
                enums,
            )
        )
        offset += resolved.size

    if len(rows) > TLM_GUI_MAX_ROWS:
        raise DictionaryError(
            f"Packet '{packet.name}' produces {len(rows)} display rows; "
            f"the tlmGUI supports at most {TLM_GUI_MAX_ROWS}"
        )

    tlm_gui_dir = Path(tlm_gui_dir)
    definition_path = tlm_gui_dir / PACKET_DEFINITION_FILE
    with definition_path.open("w") as file_handle:
        file_handle.write(
            "# F Prime packetized telemetry definition generated by fprime-cfs-config\n"
            f"# Packet: {packet.name} (id {packet.id})\n"
            "# Description, offset, length, struct type, display type, enum 0-3 display strings\n"
            "# Offsets exclude the 4-byte telemetry header offset applied by the GroundSystem GUI\n"
        )
        for description, real_offset, size, type_column, display, enums in rows:
            file_handle.write(
                f"{description}, {real_offset - TLM_GUI_GLOBAL_OFFSET}, {size}, "
                f"{type_column}, {display}, {', '.join(enums)}\n"
            )

    # F Prime telemetry framing produces space packets with a cFS telemetry secondary
    # header: the stream identifier is the secondary header flag over the bare APID
    stream_id = dictionary.get_apid("FW_PACKET_PACKETIZED_TLM") | STREAM_ID_SEC_HDR
    pages_path = tlm_gui_dir / TELEMETRY_PAGES_FILE
    with pages_path.open("w") as file_handle:
        file_handle.write(
            "# F Prime telemetry pages generated by fprime-cfs-config\n"
            "# Description, Python class, packet id (hex), telemetry definition file\n"
            f"{packet.name}, GenericTelemetry.py, {stream_id:#06x}, {PACKET_DEFINITION_FILE}\n"
        )
    return stream_id
