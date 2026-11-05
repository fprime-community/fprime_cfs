"""F Prime cFS GroundSystem runner

This script is designed to replace fprime-gds with the cFS GroundSystem GDS. It generates
cFS GroundSystem telemetry and command configuration from the F Prime dictionary,
optionally launches the cFS application, and runs the cFS GroundSystem GUI against the
generated configuration. The GroundSystem talks directly to the CI_LAB (command ingest)
and TO_LAB (telemetry output) applications on the cFS software bus over UDP; the runner
sends the TO_LAB enable-output command so telemetry flows without manual intervention.
"""

import atexit
import importlib.util
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

from pathlib import Path
from typing import Any, Dict, Tuple

from fprime_gds.executables.cli import (
    BinaryDeployment,
    ConfigDrivenParser,
    DictionaryParser,
    LogDeployParser,
    ParserBase,
)
from fprime_gds.executables.run_deployment import launch_app

from .dictionary import Dictionary
from .generate import generate
from .packets import TO_LAB_DEFAULT_MESSAGE_ID, to_lab_enable_output_packet
# Delay before sending the enable-output command, allowing a launched cFS application to boot
ENABLE_TELEMETRY_DELAY_SECONDS = 3.0
# Number of enable-output datagrams sent (UDP is unacknowledged and the command is idempotent)
ENABLE_TELEMETRY_ATTEMPTS = 3


class CfsGroundSystemParser(ParserBase):
    """Parser for cFS GroundSystem specific arguments"""

    DESCRIPTION = "cFS GroundSystem settings for use with F Prime"

    def get_arguments(self) -> Dict[Tuple[str, ...], Dict[str, Any]]:
        """Arguments to handle the cFS GroundSystem"""
        arguments = {
            ("--ground-system-dir",): {
                "action": "store",
                "default": None,
                "type": Path,
                "required": True,
                "help": "Path to the cFS-GroundSystem directory (e.g. tools/cFS-GroundSystem)",
            },
            ("--no-copy",): {
                "action": "store_true",
                "default": False,
                "help": "Write configuration into the cFS-GroundSystem directory in place "
                "instead of a temporary copy",
            },
            ("--command-address",): {
                "action": "store",
                "default": "127.0.0.1",
                "help": "UDP address of the CI_LAB command ingest application. Default: %(default)s",
            },
            ("--command-port",): {
                "action": "store",
                "type": int,
                "default": 1234,
                "help": "UDP port of the CI_LAB command ingest application. Default: %(default)s",
            },
            ("--telemetry-destination",): {
                "action": "store",
                "default": "127.0.0.1",
                "help": "Destination IP TO_LAB sends telemetry to (the GroundSystem host). "
                "Default: %(default)s",
            },
            ("--to-lab-message-id",): {
                "action": "store",
                "type": lambda value: int(value, 0),
                "default": TO_LAB_DEFAULT_MESSAGE_ID,
                "help": "Message id of the TO_LAB command application. Default: 0x1880",
            },
            ("--no-enable-telemetry",): {
                "action": "store_true",
                "default": False,
                "help": "Do not send the TO_LAB enable-output command on startup",
            },
        }
        return arguments

    def handle_arguments(self, args, **kwargs):
        """Handle arguments as parsed"""
        if not (args.ground_system_dir / "GroundSystem.py").is_file():
            raise Exception(
                f"{args.ground_system_dir} does not contain GroundSystem.py"
            )
        return args


def construct_ground_system(parsed_args, dictionary: Dictionary) -> Path:
    """Construct the cFS GroundSystem directory holding the generated configuration

    Unless --no-copy was supplied, the cFS-GroundSystem checkout is copied to a temporary
    directory (destroyed on exit) so the checkout is left pristine.
    """
    ground_system_dir = parsed_args.ground_system_dir
    if not parsed_args.no_copy:
        working_dir = Path(tempfile.mkdtemp()) / "cFS-GroundSystem"
        atexit.register(lambda: shutil.rmtree(working_dir.parent, ignore_errors=True))
        shutil.copytree(ground_system_dir, working_dir)
        ground_system_dir = working_dir
    print(f"[INFO] Generating cFS GroundSystem configuration in {ground_system_dir}")
    generate(
        dictionary,
        ground_system_dir,
        command_host=parsed_args.command_address,
        command_port=parsed_args.command_port,
        to_lab_message_id=parsed_args.to_lab_message_id,
    )
    return ground_system_dir


def enable_telemetry(parsed_args):
    """Send the TO_LAB enable-output command through the CI_LAB command ingest port

    Sends the cFS command packet carrying the telemetry destination IP, exactly as the
    GroundSystem's "Enable Tlm" quick button would.
    """
    packet = to_lab_enable_output_packet(
        parsed_args.telemetry_destination, parsed_args.to_lab_message_id
    )
    print(
        f"[INFO] Enabling TO_LAB telemetry output to {parsed_args.telemetry_destination}"
    )
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, (parsed_args.command_address, parsed_args.command_port))


def launch_ground_system(ground_system_dir: Path):
    """Launch the cFS GroundSystem GUI

    The GroundSystem is launched in its own session: on window close it SIGKILLs its
    entire process group, which would otherwise take the runner (and the atexit cleanup
    of the temporary configuration directory) down with it.
    """
    if importlib.util.find_spec("PyQt5") is None:
        raise RuntimeError(
            "the cFS GroundSystem GUI needs PyQt5: pip install 'fprime-cfs[groundsystem]'"
        )
    print("[INFO] Launching the cFS GroundSystem")
    process = subprocess.Popen(
        [sys.executable, str(ground_system_dir / "GroundSystem.py")],
        env=os.environ.copy(),
        start_new_session=True,
    )

    def kill():
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except OSError:
                pass

    atexit.register(kill)
    return process


def parse_args():
    """Parse the arguments for the F Prime cFS GroundSystem runner"""
    argument_handlers = [
        DictionaryParser,
        BinaryDeployment,
        LogDeployParser,
        CfsGroundSystemParser,
    ]
    if "FPRIME_GDS_CONFIG_PATH" in os.environ:
        ConfigDrivenParser.set_default_configuration(
            Path(os.environ["FPRIME_GDS_CONFIG_PATH"])
        )
    args, _ = ConfigDrivenParser.parse_args(
        argument_handlers, "Run F Prime deployment and the cFS GroundSystem GDS"
    )
    return args


def main():
    """Main entrypoint for the F Prime cFS GroundSystem runner"""
    parsed_args = parse_args()
    try:
        dictionary = Dictionary(parsed_args.dictionary)
        ground_system_dir = construct_ground_system(parsed_args, dictionary)

        processes = []
        if not parsed_args.noapp and parsed_args.app is not None:
            # cFS applications take no GDS connection arguments by default
            if parsed_args.application_arguments is None:
                parsed_args.application_arguments = []
            processes.append(launch_app(parsed_args))
        processes.append(launch_ground_system(ground_system_dir))
        if not parsed_args.no_enable_telemetry:
            # UDP is unacknowledged: repeat the (idempotent) enable command so a
            # slow-booting cFS application still receives one
            for _ in range(ENABLE_TELEMETRY_ATTEMPTS):
                time.sleep(ENABLE_TELEMETRY_DELAY_SECONDS)
                enable_telemetry(parsed_args)
        print(
            "[INFO] F Prime cFS GroundSystem is now running. CTRL-C to shutdown all components."
        )
        processes[-1].wait()
    except KeyboardInterrupt:
        print("[INFO] CTRL-C received. Exiting.")
    except Exception as exc:
        print(
            f"[ERROR] Shutting down F Prime cFS GroundSystem due to error: {exc}",
            file=sys.stderr,
        )
        return 1
    # Processes are killed atexit
    return 0


if __name__ == "__main__":
    sys.exit(main())
