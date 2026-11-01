#!/usr/bin/env python3
"""Erzeugt die Simulationsseite für den Webinstaller: die echte App aus webapp.h, davor sim.js als Ersatz für das Hauptpanel.

  python3 .github/scripts/sim_page.py <Zielordner> [Version]
Ergebnis: <Zielordner>/simulation.html und <Zielordner>/sim.js (sim.js liegt im Ordner installer/).
"""
import os, sys, shutil

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
out = sys.argv[1] if len(sys.argv) > 1 else "site"
ver = sys.argv[2] if len(sys.argv) > 2 else "Simulation"
src = open(os.path.join(root, "firmware", "trilumag", "webapp.h"), encoding="utf-8").read()
html = src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]

head = f"""<script>window.TRILUMAG_SIM_VERSION={ver!r};</script>
<script src="sim.js"></script>
<meta name="description" content="Trilumag ausprobieren: die App mit einer simulierten Panelwand, ganz ohne ESP32.">
<style>
#simBar{{position:relative;z-index:50;display:flex;gap:10px;align-items:center;justify-content:center;
  background:#16203d;color:#e7eaef;border-bottom:1px solid #6e8bff;padding:6px 16px;padding-top:calc(6px + env(safe-area-inset-top));
  font:13px/1.2 system-ui,-apple-system,"Segoe UI",sans-serif;white-space:nowrap}}
#simBar b{{color:#6e8bff}}#simBar span{{overflow:hidden;text-overflow:ellipsis}}
#simBar a,#simBar button{{font:inherit;color:#0d0f13;background:#6e8bff;border:0;border-radius:999px;padding:5px 10px;cursor:pointer;text-decoration:none}}
#simBar button{{background:transparent;color:#98a1ae;border:1px solid #2a303a}}
@media (max-width:480px){{#simBar .long{{display:none}}}}
</style>
"""
bar = """<div id="simBar" role="note"><span><b>Simulation</b><span class="long"> · läuft nur im Browser, ohne ESP32</span></span>
<button type="button" onclick="if(confirm('Simulation zurücksetzen? Panelwand, Farben und Presets gehen auf den Anfang.'))trilumagSim.reset()">Zurücksetzen</button>
<a href="./">Installer</a></div>
"""
i = html.lower().find("<head>")
assert i >= 0, "kein <head> in webapp.h"
i += len("<head>")
html = html[:i] + "\n" + head + html[i:]
import re
m = re.search(r"<body[^>]*>", html, re.I)
assert m, "kein <body> in webapp.h"
html = html[:m.end()] + "\n" + bar + html[m.end():]
html = html.replace("<title>Trilumag</title>", "<title>Trilumag Simulation</title>", 1)
os.makedirs(out, exist_ok=True)
open(os.path.join(out, "simulation.html"), "w", encoding="utf-8").write(html)
shutil.copy(os.path.join(root, "installer", "sim.js"), os.path.join(out, "sim.js"))
print(f"simulation.html: {len(html)} Zeichen, Version {ver}")
