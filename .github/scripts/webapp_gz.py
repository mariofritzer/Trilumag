#!/usr/bin/env python3
"""Erzeugt firmware/trilumag/webapp_gz.h: die Web-App aus webapp.h, mit gzip gepackt.

Das Hauptpanel liefert die App gepackt aus (Content-Encoding: gzip); das spart rund drei Viertel
des Flash-Platzes. Bearbeitet wird nur webapp.h, danach dieses Skript laufen lassen
(die Build-Pipeline macht das bei jedem Build selbst):
  python3 .github/scripts/webapp_gz.py
"""
import gzip, os

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "firmware", "trilumag")
src = open(os.path.join(root, "webapp.h"), encoding="utf-8").read()
html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0].encode("utf-8")
gz = gzip.compress(html, compresslevel=9, mtime=0)
lines = [",".join(f"0x{b:02X}" for b in gz[i:i + 24]) for i in range(0, len(gz), 24)]
with open(os.path.join(root, "webapp_gz.h"), "w", encoding="utf-8") as f:
    f.write("#pragma once\n// Automatisch erzeugt aus webapp.h mit .github/scripts/webapp_gz.py – nicht von Hand ändern.\n")
    f.write(f"// {len(html)} Bytes, gepackt {len(gz)} Bytes\n")
    f.write(f"const size_t WEBAPP_GZ_LEN = {len(gz)};\n")
    f.write("const uint8_t WEBAPP_GZ[] PROGMEM = {\n" + ",\n".join(lines) + "\n};\n")
print(f"webapp_gz.h: {len(html)} -> {len(gz)} Bytes")
