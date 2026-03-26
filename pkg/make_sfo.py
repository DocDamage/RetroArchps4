#!/usr/bin/env python3
"""
make_sfo.py — Generate a PS4 param.sfo for RetroArch homebrew.

The SFO (System File Object) binary format is:

  Header   (20 bytes)
  Index    (num_entries * 16 bytes)
  Key table  (null-terminated ASCII strings, padded to 4-byte alignment)
  Data table (strings padded to 4-byte alignment; uint32s at 4 bytes)

All integers are little-endian.

Usage:
    python3 make_sfo.py [options]

Options:
    --title     TITLE       Game/app title (default: RetroArch)
    --title-id  TITLEID     9-char title ID, e.g. RARCH0001 (default: RETROARCH)
    --app-ver   VER         App version string, e.g. 01.00 (default: 01.00)
    --version   VER         Master version string (default: 01.00)
    -o, --output PATH       Output file (default: pkg/sce_sys/param.sfo)
"""

import struct
import argparse
import os


# SFO data-format codes
FMT_UTF8_SPECIAL = 0x0004  # special UTF-8 (used for CATEGORY, APP_VER, etc.)
FMT_UTF8         = 0x0402  # ordinary UTF-8 string
FMT_UINT32       = 0x0404  # 32-bit unsigned integer


def _pad4(n):
    """Round n up to the next multiple of 4."""
    return (n + 3) & ~3


def make_sfo(title="RetroArch", title_id="RETROARCH",
             app_ver="01.00", version="01.00"):
    """Return the raw bytes of a param.sfo suitable for PS4 homebrew."""

    content_id = "UP0000-{:s}_00-RETROARCH000000".format(title_id)

    # Fields must be sorted alphabetically by key (SFO spec requirement).
    # Each entry: (key, fmt, value)
    fields = sorted([
        ("APP_VER",            FMT_UTF8_SPECIAL, app_ver),
        ("ATTRIBUTE",          FMT_UINT32,        0),
        ("ATTRIBUTE2",         FMT_UINT32,        0x20),
        ("CATEGORY",           FMT_UTF8_SPECIAL, "gd"),
        ("CONTENT_ID",         FMT_UTF8,          content_id),
        ("DOWNLOAD_DATA_SIZE", FMT_UINT32,        0),
        ("SYSTEM_VER",         FMT_UINT32,        0),
        ("TITLE",              FMT_UTF8,          title),
        ("TITLE_ID",           FMT_UTF8_SPECIAL, title_id),
        ("VERSION",            FMT_UTF8_SPECIAL, version),
    ], key=lambda f: f[0])

    num = len(fields)
    header_size    = 20
    index_size     = num * 16
    key_table_off  = header_size + index_size  # start of key table in file

    # --- Build key table --------------------------------------------------
    key_table  = b""
    key_offsets = []
    for key, _fmt, _val in fields:
        key_offsets.append(len(key_table))
        key_table += key.encode("ascii") + b"\x00"
    # Pad to 4-byte boundary
    key_table += b"\x00" * (_pad4(len(key_table)) - len(key_table))

    data_table_off = key_table_off + len(key_table)  # start of data table

    # --- Build data table -------------------------------------------------
    data_table   = b""
    data_offsets = []
    data_lens    = []
    data_maxlens = []

    for _key, fmt, val in fields:
        data_offsets.append(len(data_table))
        if fmt == FMT_UINT32:
            raw      = struct.pack("<I", val)
            data_len = 4
            data_max = 4
        else:
            raw      = val.encode("utf-8") + b"\x00"
            data_len = len(raw)
            data_max = _pad4(data_len)
            raw      = raw + b"\x00" * (data_max - data_len)
        data_lens.append(data_len)
        data_maxlens.append(data_max)
        data_table += raw

    # --- Assemble ---------------------------------------------------------
    # Header
    out = struct.pack("<IIIII",
        0x46535000,     # magic: "\x00PSF" in LE
        0x00000101,     # version 1.1
        key_table_off,
        data_table_off,
        num,
    )

    # Index table
    for i in range(num):
        _key, fmt, _val = fields[i]
        out += struct.pack("<HHIII",
            key_offsets[i],
            fmt,
            data_lens[i],
            data_maxlens[i],
            data_offsets[i],
        )

    out += key_table
    out += data_table
    return out


def main():
    parser = argparse.ArgumentParser(
        description="Generate a PS4 param.sfo for RetroArch homebrew.")
    parser.add_argument("--title",    default="RetroArch",
                        help="App title shown on the PS4 home screen")
    parser.add_argument("--title-id", default="RETROARCH",
                        help="9-character title ID (e.g. RARCH0001)")
    parser.add_argument("--app-ver",  default="01.00",
                        help="App version (e.g. 01.00)")
    parser.add_argument("--version",  default="01.00",
                        help="Master version (e.g. 01.00)")
    parser.add_argument("-o", "--output", default="pkg/sce_sys/param.sfo",
                        help="Output path")
    args = parser.parse_args()

    sfo = make_sfo(
        title    = args.title,
        title_id = args.title_id,
        app_ver  = args.app_ver,
        version  = args.version,
    )

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(sfo)
    print("param.sfo: {:d} bytes -> {:s}".format(len(sfo), args.output))


if __name__ == "__main__":
    main()
