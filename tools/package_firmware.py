#!/usr/bin/env python3
"""Create and validate the fixed POV OTA package envelope."""
import argparse
import struct
import zlib
from typing import Tuple

MAGIC = b"POVOTA01"
VERSION = 1
HEADER_SIZE = 256
_FIELDS = struct.Struct("<8sB16sI32s")


def make_header(board: str, build_id: str, payload: bytes) -> bytes:
    board_raw = board.encode("ascii")
    build_raw = build_id.encode("ascii")
    if not board_raw or len(board_raw) > 16 or len(build_raw) > 32:
        raise ValueError("invalid board or build identifier")
    fields = _FIELDS.pack(MAGIC, VERSION, board_raw.ljust(16, b"\0"),
                          len(payload), build_raw.ljust(32, b"\0"))
    return (fields + struct.pack("<I", zlib.crc32(fields) & 0xffffffff)).ljust(HEADER_SIZE, b"\0")


def parse_package(data: bytes) -> Tuple[str, str, bytes]:
    if len(data) <= HEADER_SIZE:
        raise ValueError("missing firmware payload")
    fields = data[:_FIELDS.size]
    magic, version, board, size, build = _FIELDS.unpack(fields)
    crc, = struct.unpack_from("<I", data, _FIELDS.size)
    if magic != MAGIC or version != VERSION or crc != (zlib.crc32(fields) & 0xffffffff):
        raise ValueError("invalid package header")
    if any(data[_FIELDS.size + 4:HEADER_SIZE]) or size != len(data) - HEADER_SIZE:
        raise ValueError("invalid package length")
    return board.rstrip(b"\0").decode("ascii"), build.rstrip(b"\0").decode("ascii"), data[HEADER_SIZE:]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--output", required=True)
    p.add_argument("--board", required=True)
    p.add_argument("--build-id", required=True)
    a = p.parse_args()
    with open(a.input, "rb") as f:
        payload = f.read()
    with open(a.output, "wb") as f:
        f.write(make_header(a.board, a.build_id, payload) + payload)


if __name__ == "__main__":
    main()
