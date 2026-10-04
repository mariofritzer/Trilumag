# Trilumag

Dreieckige RGBW-Lichtpanels zum Selberbauen, ähnlich wie Nanoleaf: Die Panels werden mit Magnet-Pogo-Steckern aneinandergeklipst, erkennen sich gegenseitig und erscheinen in der App genau so, wie sie an der Wand hängen. Gesteuert wird alles von einem ESP32 im Hauptpanel, per Web-App, HTTP-API und Home Assistant.

**Webinstaller:** https://mariofritzer.github.io/Trilumag/

## Stand

Version 0.1 läuft im **Simulationsmodus**: Die Panels werden in der App per Ziehen angeklipst. Damit lassen sich App, Erkennung, Drehung im Dreiecksraster und Home Assistant schon testen, bevor die Hardware gebaut ist.

| Teil | Stand |
| --- | --- |
| Firmware Hauptpanel (ESP32) | Simulation, Web-App, MQTT mit Home-Assistant-Discovery, WLAN per Improv |
| Bus zu den Panels (RS-485) | in Arbeit, Steckbrett-Prototyp |
| Firmware Panels (CH32V003) | geplant |
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
| `installer` | Webinstaller-Seite (ESP Web Tools) |
| `.github/workflows` | baut die Firmware für ESP32, ESP32-S3 und ESP32-C3 und veröffentlicht den Installer |

## Selbst kompilieren

Arduino IDE mit dem Boardpaket **esp32** von Espressif, dazu die Bibliotheken **ArduinoJson** (ab 7) und **PubSubClient**. Bei ESP32-S3 und ESP32-C3 unter Werkzeuge „USB CDC On Boot“ auf „Enabled“ stellen.

## Home Assistant

In `trilumag.ino` die Adresse des MQTT-Brokers eintragen. Jedes Panel erscheint dann automatisch als eigenes Licht, dazu ein Licht „Alle Panels“.

## API

| Aufruf | Zweck |
| --- | --- |
| `GET /api/state` | alle Panels mit Position, Drehung und Farbe |
| `POST /api/set` | Farbe setzen: `{"id":"A3F2C1D0" oder "alle","state":"ON","brightness":180,"color":{"r":255,"g":0,"b":0,"w":0}}` |
| `POST /api/sim/attach` | Simulation: Panel anklipsen |
| `POST /api/sim/detach` | Simulation: Panel abklipsen |
