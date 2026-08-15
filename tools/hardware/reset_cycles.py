#!/usr/bin/env python3
"""Record repeatable ESP32-P4 NSH reset-cycle evidence on Windows."""

import argparse
import pathlib
import sys
import time

import serial
from serial.serialutil import SerialException

from run_nsh import open_port, read_until_prompt


def pulse_rts(port_name: str) -> None:
    try:
        with serial.Serial(port_name, 115200, timeout=0.1) as port:
            port.dtr = False
            port.rts = True
            time.sleep(0.15)
            port.rts = False
    except (OSError, SerialException):
        # USB Serial/JTAG invalidates the handle while it re-enumerates.
        pass


def wait_until_absent(port_name: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            port = serial.Serial(port_name, 115200, timeout=0.1)
            port.close()
        except (OSError, SerialException):
            return
        time.sleep(0.1)

    raise SerialException(f"{port_name} never disappeared")


def command(port: serial.Serial, value: str, timeout: float) -> bytes:
    encoded = value.encode("ascii")
    port.reset_input_buffer()
    port.write(encoded + b"\r\n")
    port.flush()
    return read_until_prompt(port, timeout, encoded)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--cycles", type=int, required=True)
    parser.add_argument("--mode", choices=("reboot", "rts", "manual-power"),
                        required=True)
    parser.add_argument("--log", type=pathlib.Path, required=True)
    args = parser.parse_args()

    transcript = bytearray()
    failures = 0
    for cycle in range(1, args.cycles + 1):
        marker = f"\n===== {args.mode} cycle {cycle} =====\n".encode()
        sys.stdout.buffer.write(marker)
        sys.stdout.buffer.flush()
        transcript.extend(marker)

        if args.mode == "manual-power":
            print(f"Cycle {cycle}: disconnect and reconnect board power now.",
                  flush=True)
            try:
                wait_until_absent(args.port, 60)
            except SerialException as error:
                print(f"FAIL: {error}", flush=True)
                failures += 1
                continue
        elif args.mode == "rts":
            pulse_rts(args.port)
        else:
            port = open_port(args.port, time.monotonic() + 20)
            try:
                port.write(b"\x03\r\n")
                read_until_prompt(port, 5)
                port.write(b"reboot\r\n")
                port.flush()
                read_until_prompt(port, 2, b"reboot")
            except (OSError, SerialException):
                pass
            finally:
                port.close()

        port = open_port(args.port, time.monotonic() + 20)
        try:
            boot = read_until_prompt(port, 10)
            uname = command(port, "uname -a", 5)
        finally:
            port.close()

        data = boot + uname
        transcript.extend(data)
        passed = b"NuttX 13.0.0" in uname and b"nsh>" in uname
        print(f"cycle {cycle}: {'PASS' if passed else 'FAIL'}", flush=True)
        failures += 0 if passed else 1

    summary = (f"\n{args.mode}: {args.cycles - failures}/"
               f"{args.cycles} passed\n").encode()
    sys.stdout.buffer.write(summary)
    transcript.extend(summary)
    args.log.parent.mkdir(parents=True, exist_ok=True)
    args.log.write_bytes(transcript)
    return 0 if failures == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
