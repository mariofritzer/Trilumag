#!/usr/bin/env python3
"""Erzeugt firmware/trilumag/panel_fw.h aus der gebauten Panel-Firmware.

Das Hauptpanel trägt die aktuelle Panel-Firmware in sich und spielt sie über den Bus auf
Panels mit älterer Version (siehe docs/protokoll.md, Abschnitt Panel-Updates).
Aufruf: python3 .github/scripts/panel_fw_header.py firmware/panel/panel.bin firmware/panel/panel.c firmware/trilumag/panel_fw.h
"""
import re, sys, zlib

bin_path, src_path, out_path = sys.argv[1:4]
data = open(bin_path, "rb").read()
ver = int(re.search(r"#define FW_VERSION (\d+)", open(src_path, encoding="utf-8").read()).group(1))
if len(data) > 0x3FC0:
    sys.exit(f"Panel-Firmware zu groß: {len(data)} Bytes (höchstens 16320)")
pad = data + b"\xff" * (-len(data) % 64)          # auf ganze Flash-Seiten auffüllen
crc = 0xFFFF
for b in pad:                                       # CRC-16/CCITT wie im Bootloader
    crc ^= b << 8
    for _ in range(8):
        crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
lines = [", ".join(f"0x{b:02X}" for b in pad[i:i + 16]) for i in range(0, len(pad), 16)]
with open(out_path, "w", encoding="utf-8") as f:
    f.write("#pragma once\n// Automatisch erzeugt aus firmware/panel/panel.bin – nicht von Hand ändern.\n")
    f.write(f"// Panel-Firmware Version {ver}, {len(data)} Bytes, {len(pad) // 64} Seiten\n")
    f.write(f"const uint8_t PANEL_FW_VERSION = {ver};\n")
    f.write(f"const uint16_t PANEL_FW_PAGES = {len(pad) // 64};\n")
    f.write(f"const uint16_t PANEL_FW_CRC = 0x{crc:04X};\n")
    f.write("const uint8_t PANEL_FW[] = {\n  " + ",\n  ".join(lines) + "\n};\n")
print(f"panel_fw.h: Version {ver}, {len(data)} Bytes, CRC {crc:04X}")
