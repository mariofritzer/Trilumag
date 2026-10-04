# Trilumag

Dreieckige RGBW-Lichtpanels zum Selberbauen, ähnlich wie Nanoleaf: Die Panels werden mit Magnet-Pogo-Steckern aneinandergeklipst, erkennen sich gegenseitig und erscheinen in der App genau so, wie sie an der Wand hängen. Gesteuert wird alles von einem ESP32 im Hauptpanel, per Web-App, HTTP-API und Home Assistant.

**Webinstaller:** https://mariofritzer.github.io/Trilumag/

## Stand

Aktuell: **Version 0.7**. Was sich wann geändert hat, steht im [Versionsverlauf](CHANGELOG.md).

Ab Version 0.2 gibt es zwei Betriebsarten, umschaltbar in der App: **Simulation** (Panels werden in der App per Ziehen angeklipst) und **Bus** (echte Panels über RS-485). Das Protokoll steht in [docs/protokoll.md](docs/protokoll.md).

| Teil | Stand |
| --- | --- |
| Firmware Hauptpanel (ESP32) | Simulation und echter Bus, Web-App im Stil von WLED mit fester Verbindung, Effekte mit Paletten, Presets, Pin-Einstellungen pro Board, MQTT mit Home-Assistant-Discovery, WLAN per Improv |
| Bus zu den Panels (RS-485) | Protokoll v1 fertig, am PC mit echter Firmware simuliert und getestet, Steckbrett-Test folgt |
| Firmware Panels (CH32V003) | Version 1: Erkennung, Farben, blau pulsieren bis zur ersten Farbe |
| Platinen | Entwurf v0.1 |

## Aufbau

- **Hauptpanel:** ESP32-S3, 24V-Eingang mit Sicherung und Verpolschutz, RS-485-Master, eigene LEDs
- **Panels:** CH32V003, RS-485, WS2814 24V RGBW (10 cm pro Kante)
- **Kanten:** je ein Paar Magnet-Pogo-Stecker (männlich + weiblich), 24V, GND, A, B und eine SNS-Leitung zur Nachbarerkennung
- **Kantennummerierung** bei jedem Panel gleich, gegen den Uhrzeigersinn: Kante 1 unten, 2 rechts, 3 links (Spitze oben)

## Ordner

| Ordner | Inhalt |
| --- | --- |
| `firmware/trilumag` | Arduino-Sketch für das Hauptpanel |
| `firmware/panel` | Firmware für die Panels (CH32V003, ch32fun) |
| `docs` | Bus-Protokoll |
| `installer` | Webinstaller-Seite (ESP Web Tools) |
| `.github/workflows` | baut die Firmware für ESP32, ESP32-S3, ESP32-C3 und ESP32-C6 und veröffentlicht den Installer |

## Selbst kompilieren

**Hauptpanel:** Arduino IDE mit dem Boardpaket **esp32** von Espressif, dazu die Bibliotheken **ArduinoJson** (ab 7), **PubSubClient** und **Adafruit NeoPixel**. Bei ESP32-S3, ESP32-C3 und ESP32-C6 unter Werkzeuge „USB CDC On Boot“ auf „Enabled“ stellen. Unter „Partition Scheme“ immer **Minimal SPIFFS (1.9MB APP with OTA)** wählen, sonst ist für Online-Updates kein Platz.

**Panels:** [ch32fun](https://github.com/cnlohr/ch32fun) und eine RISC-V-Toolchain, dann `make -C firmware/panel CH32FUN=<pfad>/ch32fun/ch32fun`. Fertige Dateien gibt es auf der Installer-Seite.

## Web-App

Die App ist aufgebaut wie bei WLED: oben Ein/Aus und Gesamthelligkeit der ganzen Wand, unten fünf Tabs.

- **Farben:** Farbrad mit Helligkeit, Weißanteil, Schnellfarben und Hex-Eingabe. Ohne Auswahl gilt die Farbe für die ganze Wand. In der Vorschau oben angetippte Panels bekommen ihre eigene Farbe. Läuft ein Effekt, der eine Farbe benutzt, färbt das Farbrad den Effekt ein, statt ihn zu beenden.
- **Effekte:** Tempo, Intensität, Effektliste mit Suche und Paletten.
- **Wand:** Panels anordnen (in der Simulation per Ziehen), Details, Helligkeit und Ein/Aus pro Panel.
- **Presets:** Szenen speichern, abrufen und löschen (bis zu 16).
- **Optionen:** WLAN (gefundene Netze zum Antippen), Betriebsart, Board und Pins, Farbreihenfolge mit Farbtest, MQTT mit Schalter „MQTT aktiv“.

Die App hält wie WLED eine feste Verbindung (WebSocket auf Port 81). Das Hauptpanel schickt Änderungen und das Effektbild etwa 15-mal pro Sekunde von selbst. Ist die Verbindung weg, fragt die App per HTTP nach, bis sie wieder steht.

Updates über den Webinstaller behalten WLAN, Einstellungen, Farben und Presets. Nur „Gerät löschen“ beim Installieren setzt alles zurück.

## Effekte und Paletten

Effekte laufen immer über die ganze Wand: Einfarbig, Regenbogen, Regenbogenwelle, Atmen, Farbwechsel, Funkeln, Ausbreiten (Wellen vom Hauptpanel nach außen), Feuer und Polarlicht. Die **Intensität** ändert je nach Effekt die Streuung, die Tiefe des Atmens, wie oft es funkelt, die Breite der Wellen oder wie stark das Feuer flackert.

**Paletten:** Standard (die eigenen Farben des Effekts), Regenbogen, Effektfarbe, Ozean, Lava, Wald, Sonnenuntergang, Party, Pastell und Eis.

Das Hauptpanel rechnet die Bilder selbst und schickt etwa 25 pro Sekunde mit einem FRAME-Befehl an alle Panels. Neu angeklipste Panels pulsieren erst 5-mal blau (etwa 8 Sekunden) und laufen dann mit. Bekommt ein einzelnes Panel eine feste Farbe, endet der Effekt. Der zuletzt gewählte Effekt läuft nach einem Neustart weiter.

## Presets

Ein Preset merkt sich Ein/Aus, Gesamthelligkeit, den Effekt mit Tempo, Intensität, Palette und Effektfarbe und die Farbe jedes bekannten Panels über seine Chip-ID. Ist ein Panel beim Laden gerade nicht an der Wand, bekommt es seine Farbe beim nächsten Anklipsen.

## Home Assistant

In der App unter Optionen „MQTT aktiv“ einschalten und die Adresse des Brokers eintragen. Danach erscheinen automatisch:

- **Ein Licht pro Panel**
- **„Alle Panels“:** Ein/Aus, Gesamthelligkeit, Farbe und die Effektliste.
- **Schieberegler:** „Effekt-Tempo“ und „Effekt-Intensität“.
- **Auswahlen:** „Palette“ und „Preset“.

## API

| Aufruf | Zweck |
| --- | --- |
| `GET /api/state` | alle Panels mit Position, Drehung und Farbe, dazu Ein/Aus, Gesamthelligkeit, Effekt, Effekte, Paletten und Presets |
| `POST /api/set` | ein Panel: `{"id":"A3F2C1D0","state":"ON","brightness":180,"color":{"r":255,"g":0,"b":0,"w":0}}`, mehrere: `{"ids":[…],…}`, ganze Wand: `{"id":"alle","state":"ON","brightness":200,"color":{…}}` (hier ist `brightness` die Gesamthelligkeit) |
| `POST /api/effect` | `{"effect":"welle","speed":50,"intensity":128,"palette":"ozean"}`, alle Felder optional, `"aus"` heißt Einfarbig |
| `POST /api/presets` | `{"action":"save","name":"Abend"}`, `{"action":"load","id":0}`, `{"action":"delete","id":0}` |
| `GET /api/live` | aktuelles Effektbild aller Panels als RRGGBBWW |
| `POST /api/sim/attach` | Simulation: Panel anklipsen |
| `POST /api/sim/detach` | Simulation: Panel abklipsen |
