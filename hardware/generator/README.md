# Platinen-Generator

Erzeugt Schaltplan und Layout beider Platinen mit der KiCad-Python-API (getestet mit KiCad 7, öffnet in KiCad 10).

Nebenplatine: `python3 selfroute.py panel`, danach `repair.py` / `ripup.py`, bis die DRC keine offenen Verbindungen mehr meldet.

Hauptplatine: `./auto.sh`, danach die Nacharbeit in dieser Reihenfolge:
1. `usbfan.py` – Fanout am USB-C-Stecker J4 von Hand (0,5-mm-Raster)
2. `usbroute.py` – CC1/CC2, D+/D−, VBUS von den Fanout-Enden aus verlegen
3. `fixmain.py` – zu große Antennen-Sperrfläche des Moduls entfernen, Sicherungsclips brücken, Masse-Inseln
4. `fix2main.py` – U6 (INA226) Masse und SDA, Taster-Pins
5. `stitchfrag.py`, `gndfix.py C5.2 U3.1` – Masseflächen-Inseln anbinden
