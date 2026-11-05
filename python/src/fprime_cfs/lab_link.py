"""fprime_cfs.lab_link: F Prime GDS communication plugin for the cFS lab applications

Selected with `fprime-gds --communication-selection cfs-lab`. Commands are sent as UDP
datagrams to CI_LAB (default port 1234) and telemetry is received from TO_LAB (default
port 2234), the same path the cFS GroundSystem uses. TO_LAB only transmits after it is
told where to: the adapter sends the TO_LAB enable-output command when the link opens
and repeats it while no telemetry arrives: every second until the first telemetry packet
(CI_LAB may not be listening yet when the GDS starts), then at a configurable interval, so
a cFS instance started or restarted after the GDS begins transmitting without manual
intervention. The repeat costs one TO_LAB "output enabled" event per interval on a quiet
system.

The lab applications carry data over plain UDP with no authentication or integrity
protection; this link is for isolated lab and bench networks only.
"""

import logging
import time

from fprime_gds.common.communication.adapters.udp import UdpAdapter
from fprime_gds.plugin.definitions import gds_plugin_implementation

from .packets import TO_LAB_DEFAULT_MESSAGE_ID, to_lab_enable_output_packet

LOGGER = logging.getLogger("cfs_lab_link")

# Number of enable-output datagrams sent when the link opens (UDP is unacknowledged, the command is idempotent)
ENABLE_OUTPUT_ATTEMPTS = 3
# Until the first telemetry arrives the enable-output command is repeated this often (cFS
# started alongside the GDS has no CI_LAB listening yet when open() sends its attempts)
STARTUP_RETRY_SECONDS = 1.0
# Default telemetry silence after which the enable-output command is repeated (once); 0 disables repeats
DEFAULT_ENABLE_OUTPUT_INTERVAL_SECONDS = 30.0


class CfsLabAdapter(UdpAdapter):
    """UDP link to CI_LAB and TO_LAB that keeps TO_LAB's telemetry output enabled"""

    def __init__(
        self,
        cfs_address,
        ci_lab_port,
        to_lab_port,
        telemetry_destination,
        to_lab_message_id,
        enable_telemetry,
        enable_interval=DEFAULT_ENABLE_OUTPUT_INTERVAL_SECONDS,
    ):
        super().__init__(cfs_address, ci_lab_port, to_lab_port)
        self.telemetry_destination = telemetry_destination
        self.enable_interval = enable_interval
        self.enable_packet = (
            to_lab_enable_output_packet(telemetry_destination, to_lab_message_id)
            if enable_telemetry
            else None
        )
        self.last_received = None
        self.last_enabled = None

    def __repr__(self):
        """String representation for logging"""
        return f"cFS lab link: CI_LAB@{self.address}:{self.send_port} TO_LAB->:{self.recv_port}"

    def open(self):
        """Open the UDP sockets and enable TO_LAB's output toward this host"""
        super().open()
        self.enable_output(ENABLE_OUTPUT_ATTEMPTS)

    def read(self, timeout=0.500):
        """Read telemetry, repeating the enable-output command while telemetry is silent"""
        data = super().read(timeout)
        now = time.monotonic()
        if data:
            self.last_received = now
        elif self.is_due(now):
            self.enable_output(1)
        return data

    def is_due(self, now: float) -> bool:
        """Whether telemetry silence warrants repeating the enable-output command

        Before the first telemetry packet the retry period is STARTUP_RETRY_SECONDS (or the
        interval if shorter); afterwards it is the configured interval.
        """
        if self.enable_packet is None or self.enable_interval <= 0:
            return False
        if self.last_received is None:
            period = min(STARTUP_RETRY_SECONDS, self.enable_interval)
        else:
            period = self.enable_interval
            if now - self.last_received < period:
                return False
        return self.last_enabled is None or now - self.last_enabled >= period

    def write(self, frame):
        """Send a frame to CI_LAB; an empty frame (a packet the framer dropped) is a failure"""
        if not frame:
            return False
        return super().write(frame)

    def enable_output(self, attempts: int):
        """Send the TO_LAB enable-output command through CI_LAB"""
        if self.enable_packet is None or self.send_socket is None:
            return
        LOGGER.info(
            "Enabling TO_LAB telemetry output to %s", self.telemetry_destination
        )
        for _ in range(attempts):
            self.write(self.enable_packet)
        self.last_enabled = time.monotonic()

    @classmethod
    def get_name(cls):
        """Name of this communication plugin"""
        return "cfs-lab"

    @classmethod
    def get_arguments(cls):
        """Command line arguments of this plugin, keyed by flag"""
        return {
            ("--cfs-address",): {
                "dest": "cfs_address",
                "type": str,
                "default": "127.0.0.1",
                "help": "Address of the cFS host running CI_LAB. Default: %(default)s",
            },
            ("--ci-lab-port",): {
                "dest": "ci_lab_port",
                "type": int,
                "default": 1234,
                "help": "UDP port of the CI_LAB command ingest application. Default: %(default)s",
            },
            ("--to-lab-port",): {
                "dest": "to_lab_port",
                "type": int,
                "default": 2234,
                "help": "Local UDP port receiving TO_LAB telemetry output. Default: %(default)s",
            },
            ("--telemetry-destination",): {
                "dest": "telemetry_destination",
                "type": str,
                "default": "127.0.0.1",
                "help": "IP address TO_LAB is told to send telemetry to (this host as seen by cFS). "
                "Default: %(default)s",
            },
            ("--to-lab-message-id",): {
                "dest": "to_lab_message_id",
                "type": lambda value: int(value, 0),
                "default": TO_LAB_DEFAULT_MESSAGE_ID,
                "help": "TO_LAB command message id. Default: 0x%(default)04x",
            },
            ("--no-enable-telemetry",): {
                "dest": "enable_telemetry",
                "action": "store_false",
                "default": True,
                "help": "Do not send the TO_LAB enable-output command",
            },
            ("--to-lab-enable-interval",): {
                "dest": "enable_interval",
                "type": float,
                "default": DEFAULT_ENABLE_OUTPUT_INTERVAL_SECONDS,
                "help": "Seconds of telemetry silence after which the enable-output command is repeated; "
                "0 never repeats. Default: %(default)s",
            },
        }

    @classmethod
    def check_arguments(
        cls,
        cfs_address,
        ci_lab_port,
        to_lab_port,
        telemetry_destination,
        to_lab_message_id,
        enable_telemetry,
        enable_interval,
    ):
        """Validate the plugin arguments, raising ValueError on bad values"""
        UdpAdapter.check_arguments(cfs_address, ci_lab_port, to_lab_port)
        if enable_telemetry:
            to_lab_enable_output_packet(telemetry_destination, to_lab_message_id)
        if enable_interval < 0:
            raise ValueError("TO_LAB enable interval must not be negative")

    @classmethod
    @gds_plugin_implementation
    def register_communication_plugin(cls):
        """Register this as a communication plugin"""
        return cls
