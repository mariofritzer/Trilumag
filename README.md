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
| Firmware Panels (CH32V003) | Version 3: Erkennung, Farben, Kanten einzeln, Antippen per Bewegungssensor, Updates über den Bus mit eigenem Bootloader |
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

**Hauptpanel:** Arduino IDE mit dem Boardpaket **esp32** von Espressif, dazu die Bibliotheken **ArduinoJson** (ab 7), **PubSubClient** und **Adafruit NeoPixel**. Bei ESP32-S3, ESP32-C3 und ESP32-C6 unter Werkzeuge „USB CDC On Boot“ auf „Enabled“ stellen. Unter „Partition Scheme“ immer **Minimal SPIFFS (1.9MB APP with OTA)** wählen, sonst ist für Online-Updates kein Platz. Für den **ESP32-C6 mit Hue** zusätzlich „Zigbee mode“ auf **Zigbee ZCZR (coordinator/router)** stellen und `partitions_c6_zigbee.csv` als `partitions.csv` in den Sketch-Ordner kopieren (nur für diesen Build, sonst gilt sie für alle Chips). Die App steht in `webapp.h`; nach Änderungen daran `python3 .github/scripts/webapp_gz.py` ausführen, das erzeugt die gepackte Fassung `webapp_gz.h`, die das Hauptpanel ausliefert.

**Panels:** [ch32fun](https://github.com/cnlohr/ch32fun) und eine RISC-V-Toolchain. Ein neues Panel wird einmal mit dem WCH-LinkE programmiert: `make -C firmware/panel flashall CH32FUN=<pfad>/ch32fun/ch32fun` schreibt Bootloader, Option-Byte und Firmware. Danach bekommt es neue Versionen über den Bus vom Hauptpanel (Optionen → Panel-Firmware). Fertige Dateien gibt es bei jedem Release (`trilumag-panel.bin`, `trilumag-panel-bootloader.bin`).

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

## Signale und Fortschritt (Home Assistant)

- **Signal:** Das Gerät „Signal“ (notify) lässt die ganze Wand blinken, danach läuft alles weiter wie vorher, auch ein Effekt. Nachricht: Farbe und Anzahl, z. B. `blau 3` oder `#ff8800 5`. Farben: rot, grün, blau, gelb, orange, lila, pink, türkis, weiß.

  ```yaml
  action: notify.send_message
  target: {entity_id: notify.trilumag_signal}
  data: {message: "blau 3"}
  ```

  Per MQTT geht auch JSON an `trilumag/signal/set`: `{"color":"grün","blink":2,"ms":700}`.
- **Fortschritt:** Die Zahl „Fortschritt“ (0 bis 100 %) füllt die Wand vom Hauptpanel aus, 0 % schaltet es aus. Mit Farbe per MQTT an `trilumag/fortschritt/set`: `{"value":40,"color":"grün"}`.

## Mehrere Wände

Unter Optionen → Mehrere Wände lassen sich Wände im selben WLAN zu einer Gruppe (1 bis 9) verbinden. Sie teilen Ein/Aus, Helligkeit und Effekt mit allen Einstellungen, egal an welcher Wand man etwas ändert. Den Takt der Effekte gibt die Wand mit der kleinsten Chip-ID vor. Feste Farben einzelner Panels bleiben pro Wand.

## Energie

Trilumag zählt den Verbrauch mit, gemessen mit dem INA226 oder aus den Farben geschätzt, und zeigt ihn unter Optionen → Energie pro Tag, Monat und Jahr. Das Datum kommt per Internet (NTP, Zeitzone Österreich). In Home Assistant gibt es den Zähler „Energie“ (kWh) für das Energie-Dashboard.

## WLAN-Wächter und Neustart-Grund

Unter Optionen → WLAN lässt sich der WLAN-Wächter ein- und ausschalten (Standard: an). Ist das WLAN 3 Minuten weg, verbindet sich Trilumag neu; fehlt es nach 10 Minuten immer noch, startet es neu, außer jemand ist gerade im Setup-Netz. Hängt die Hauptschleife eine Minute, startet es ebenfalls neu. `trilumag.local` wird alle 30 Minuten neu angekündigt. Warum Trilumag zuletzt neu gestartet ist (Stromausfall, Update, Absturz, Wächter …) und wie oft es abgestürzt ist, steht unter Optionen → Info und im Ereignisprotokoll.

## Andere Trilumag im WLAN

Unter Optionen → Andere Trilumag sucht die App nach weiteren Wänden im selben WLAN (per mDNS) und listet sie mit Name, Adresse und Version auf. „Öffnen“ führt direkt zu deren Steuerungsseite. Laufen mehrere Wände, heißt nur eine davon `trilumag.local`, die anderen bekommen automatisch `trilumag-2.local` und so weiter; die Liste zeigt, welche welche ist.

Unter Optionen → Updates lässt sich dann auswählen, auf welchen Wänden eine Version installiert wird: diese Wand und/oder gefundene andere. Die anderen holen sich die Version selbst von GitHub; ist diese Wand auch dabei, kommt sie zuletzt dran. Wände ab 0.7.21 lassen sich so aktualisieren.

## Panel tauschen

In der App unter Wand das Panel antippen und „Tauschen“ wählen. Danach bleiben 5 Minuten Zeit: das alte Panel abklipsen und das neue an genau dieselbe Stelle setzen. Das neue übernimmt Farbe, Helligkeit und „Kanten einzeln“ des alten. Solange läuft unten in der App ein kleines Fenster mit der Restzeit und ✕ zum Abbrechen. Panels, die hinter dem alten hingen, gehen beim Abklipsen kurz aus und melden sich über das neue wieder; in der Simulation landen sie dazwischen in der Ablage und kommen danach von selbst an ihren Platz zurück.

## Sprache

Die App ist auf Deutsch und Englisch. Sie richtet sich nach der Sprache des Browsers; unter Optionen → Name lässt sie sich fest einstellen. Die Namen in Home Assistant und die Versionshinweise bleiben deutsch.

## Philips Hue (nur ESP32-C6)

Mit einem ESP32-C6 als Hauptpanel meldet sich Trilumag zusätzlich als Zigbee-3.0-Farblampe „Trilumag Wand“. Eine Hue Bridge (auch die Bridge Pro) nimmt sie wie eine Lampe eines anderen Herstellers auf: Ein/Aus, Helligkeit und Farbe der ganzen Wand, auch in Szenen, Routinen und mit Hue-Schaltern. Effekte und einzelne Panels bleiben in der Trilumag-App.

- Einschalten unter Optionen → Philips Hue, dann in der Hue-App unter Einstellungen → Lampen → „+“ nach neuen Lampen suchen.
- Der C6 braucht dafür einmal die neue Speicheraufteilung mit den Zigbee-Bereichen: einmal mit dem Webinstaller flashen. Einstellungen und Wand bleiben erhalten.
- WLAN und Zigbee teilen sich beim C6 eine Antenne. Hängt das Hauptpanel mit Zigbee dreimal beim Start, schaltet es Zigbee von selbst wieder aus.
- Alle anderen Chips haben kein Zigbee, dort fehlt das Kästchen in der App.

## API

| Aufruf | Zweck |
| --- | --- |
| `GET /api/state` | alle Panels mit Position, Drehung und Farbe, dazu Ein/Aus, Gesamthelligkeit, Effekt, Effekte, Paletten und Presets |
| `POST /api/set` | ein Panel: `{"id":"A3F2C1D0","state":"ON","brightness":180,"color":{"r":255,"g":0,"b":0,"w":0}}`, mehrere: `{"ids":[…],…}`, ganze Wand: `{"id":"alle","state":"ON","brightness":200,"color":{…}}` (hier ist `brightness` die Gesamthelligkeit), Helligkeit aller Panels: `{"panelBri":255}` |
| `POST /api/effect` | `{"effect":"welle","speed":50,"intensity":128,"palette":"ozean","direction":90,"spin":0}`, alle Felder optional, `"aus"` heißt Einfarbig. Beim Farbverlauf (`"verlauf"`) setzt `"color2":{"r":0,"g":80,"b":255,"w":0}` die zweite Farbe |
| `POST /api/presets` | `{"action":"save","name":"Abend"}`, `{"action":"load","id":0}`, `{"action":"delete","id":0}` |
| `GET /api/live` | aktuelles Effektbild aller Panels als RRGGBBWW |
| `GET /api/diag` | Bus-Diagnose: Zähler, Antwortzeiten pro Panel, Ereignisprotokoll |
| `GET /api/backup`, `POST /api/restore` | Sicherung herunterladen und einspielen |
| `POST /api/light` | Übergangszeit, Stromlimit und Stromsensor-Pins |
| `POST /api/touch` | Antippen: an/aus, Empfindlichkeit 1–10, Aktion für einmal (`a1`) und doppelt (`a2`) |
| `POST /api/signal` | Signal: `{"color":"blau","blink":3,"ms":700}` |
| `POST /api/progress` | Fortschritt: `{"value":40,"color":"grün"}`, `0` = aus |
| `POST /api/identify` | Panel finden: `{"id":"…"}`, das Panel blinkt 3 s weiß |
| `POST /api/favs` | Favoriten: `{"effect":"lava","on":true}` |
| `POST /api/swap` | Panel tauschen: `{"id":"altes Panel"}` startet, danach 5 Minuten Zeit; `{"stop":true}` bricht ab |
| `POST /api/ota` | Updates: `{"action":"check"}`, `{"action":"install","version":"0.7.40"}`, auch für andere Wände: `"peers":["192.168.1.61"]`, ohne diese Wand: `"self":false` |
| `POST /api/peers`, `GET /api/peers` | andere Trilumag im WLAN suchen (dauert etwa 3 s) und die Liste abholen |
| `POST /api/guard` | WLAN-Wächter: `{"on":true}` |
| `POST /api/sleep` | Sleep-Timer: `{"min":30}`, `0` beendet ihn |
| `POST /api/boot` | Nach Stromausfall: `{"mode":0}` wie vorher, `1` aus, `2` an, `3` mit `"preset"` |
| `GET /api/energy` | Energieverbrauch: heute, Monat, Jahr, gesamt und Werte pro Tag, Monat und Jahr (Wh) |
| `POST /api/sync` | Mehrere Wände im Gleichtakt: `{"on":true,"group":1}` (UDP-Port 21330 im WLAN) |
| `POST /api/zigbee` | Nur ESP32-C6: `{"action":"on","value":true}` (startet neu), `{"action":"pair"}` (neu koppeln) |
| `POST /api/panelfw` | Panel-Firmware: `{"action":"all"}`, `{"action":"one","id":"…"}`, `{"action":"auto","on":true}` |
| `/json`, `/json/state`, `/json/info`, `/presets.json` | WLED-kompatible Schnittstelle (eine Wand = ein Segment) |
| `POST /api/sim/attach` | Simulation: Panel anklipsen |
| `POST /api/sim/detach` | Simulation: Panel abklipsen |
| `POST /api/sim/rotate` | Simulation: Panel an Ort und Stelle drehen, `{"id":"…"}` |
| `POST /api/sim/remove` | Simulation: Panel aus der Ablage löschen, `{"id":"…"}` oder `{"all":true}` |
