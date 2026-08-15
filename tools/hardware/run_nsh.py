#!/usr/bin/env python3
"""Run NSH commands over the ESP32-P4 USB Serial/JTAG console."""

import argparse
import pathlib
import sys
import time

import serial
from serial.serialutil import SerialException


def open_port(port_name: str, deadline: float) -> serial.Serial:
    while time.monotonic() < deadline:
        try:
            port = serial.Serial()
            port.port = port_name
            port.baudrate = 115200
            port.timeout = 0.1
            port.write_timeout = 2
            port.dtr = False
            port.rts = False
            port.open()
            return port
        except (OSError, SerialException):
            time.sleep(0.2)

    raise SerialException(f"{port_name} did not become available")


def read_until_prompt(
    port: serial.Serial, timeout: float, command: bytes | None = None
) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < deadline:
        try:
            chunk = port.read(port.in_waiting or 1)
        except (OSError, SerialException):
            port.close()
            port = open_port(port.port, time.monotonic() + 10)
            continue

        if chunk:
            data.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            if command is None:
                if b"nsh>" in data:
                    break
            else:
                echo = data.find(command)
                if echo >= 0 and data.find(b"nsh>", echo + len(command)) >= 0:
                    break
        else:
            time.sleep(0.02)

    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("commands", nargs="+")
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--expect", action="append", default=[])
    parser.add_argument("--log", type=pathlib.Path, required=True)
    args = parser.parse_args()

    port = open_port(args.port, time.monotonic() + 20)
    transcript = bytearray()
    try:
        port.write(b"\x03\r\n")
        transcript.extend(read_until_prompt(port, 5))
        time.sleep(0.25)
        port.read(port.in_waiting or 1)
        for command in args.commands:
            marker = f"\n===== COMMAND: {command} =====\n".encode()
            sys.stdout.buffer.write(marker)
            sys.stdout.buffer.flush()
            transcript.extend(marker)
            port.reset_input_buffer()
            encoded = command.encode("ascii")
            port.write(encoded + b"\r\n")
            port.flush()
            transcript.extend(read_until_prompt(port, args.timeout, encoded))
    finally:
        port.close()

    args.log.parent.mkdir(parents=True, exist_ok=True)
    args.log.write_bytes(transcript)

    missing = [item for item in args.expect if item.encode() not in transcript]
    for item in missing:
        print(f"MISSING expected text: {item}", file=sys.stderr)

    return 2 if missing or b"nsh>" not in transcript else 0


if __name__ == "__main__":
    raise SystemExit(main())
