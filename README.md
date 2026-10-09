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

## Android-App

Die App hat eine eigene Versionsnummer (1.0, 1.1, …), unabhängig von der Firmware; sie steigt nur, wenn sich an der App etwas ändert. Die Datei heißt `trilumag-app-<Version>.apk` und hängt an dem Release, mit dem diese App-Version kam: <https://github.com/mariofritzer/Trilumag/releases>. Beim ersten Installieren fragt Android, ob der Browser Apps installieren darf; das einmal erlauben.

- Beim Start sucht die App alle Trilumag im WLAN und zeigt sie als Liste mit Namen, Farbe, Ein/Aus-Schalter und Helligkeitsregler, ähnlich wie die WLED-App.
- Antippen öffnet die gewohnte Steuerung im Vollbild. ＋ fügt eine Wand über ihre IP-Adresse hinzu, langes Drücken entfernt sie aus der Liste.
- Die App sieht selbst bei GitHub nach (höchstens alle drei Stunden) und zeigt oben einen Hinweis, wenn eine neuere App-Version da ist. „Installieren“ lädt sie in der App herunter und installiert sie; dahinter wird alles unscharf, davor stehen Fortschritt und der aktuelle Schritt. Beim ersten Mal fragt Android, ob Trilumag Apps installieren darf.
- Sicherungen und das Wandbild werden unter Downloads gespeichert.
- **Hinweis bei Störung** (unten in der Liste einschalten): Die App sieht etwa alle 15 Minuten im Hintergrund nach, ob alle Panelwände erreichbar sind und ihre Panels antworten, und meldet sich sonst. Nur im eigenen WLAN, unterwegs kommt nichts.

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
- **Status** (Diagnose): „Bereit“ oder „Update läuft“. Während die Wand eine neue Firmware installiert, trennt sie alle Verbindungen nach außen (MQTT, Hue, WLED, Wetter, Suche nach anderen Wänden) und verbindet sich nach dem Neustart von selbst wieder.

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

## Raum: mehrere Panelwände zusammen

Begriffe: Eine **Panelwand** ist ein Trilumag mit seinen Panels, eine **Raumwand** eine Wand des Raums im Grundriss.

Unter Optionen → Andere Trilumag → „Raum einrichten“ (in der Android-App über ⌂ oben):

- **Grundriss von oben:** Ecken ziehen, ＋ an einer Raumwand fügt eine Ecke ein, eine Ecke oder Raumwand antippen zum Löschen. Die Panelwände (Striche) zieht man entlang der Raumwände an ihren Platz.
- **Jede Raumwand von vorne:** so, wie man im Raum davor steht, mit den echten Panel-Formen und ihren aktuellen Farben. Panelwände frei an ihre Stelle und Höhe ziehen.
- **Maße:** Raumhöhe und Seitenlänge eines Panels in cm.
- Der Raum liegt auf allen Panelwänden gleich: wo man ihn ändert, wird er an alle anderen weitergegeben. Klipst man an einer Panelwand Panels ab oder an, zieht sie ihren Eintrag selbst nach.
- **Raum-Effekte:** Der Raum-Komet läuft wie der Komet einer Panelwand: Panel für Panel, bei „Kanten einzeln“ Kante für Kante, und springt am Ende einer Panelwand zur nächsten Panelwand im Raum. Es gibt genau einen Kopf für alle Panelwände. Raum-Regenbogen und Raum-Welle laufen entlang der Raumwände über alle Panelwände. Jede Panelwand rechnet selbst mit ihrer Stelle im Raum und der Uhrzeit aus dem Internet, es werden keine Bilder verschickt. Lange Lücken zwischen den Panelwänden werden dabei kurz. Helligkeit, Tempo, Intensität und Palette gehen an alle Panelwände, eigene Paletten mit ihren Farben.
- **Mehrere Räume** (bis zu 6): oben im Raum-Fenster umschalten, „＋ Raum“ legt einen neuen an, der Name steht unter „Maße“. Eine Panelwand steht in höchstens einem Raum; „Hierher holen“ holt sie aus einem anderen.
- **Raum löschen:** löscht diesen Raum mit Grundriss, Aufstellung und Szenen auf allen Panelwänden; die Panelwände und ihre Einstellungen bleiben.
- **Raum-Szenen:** merken sich, was jede Panelwand im Raum zeigt (Effekt, Farben, Helligkeit), und stellen es auf allen wieder her (bis zu 8 je Raum).
- **Antippen:** Steht die Panelwand in einem Raum, läuft die Welle beim Antippen über alle Panelwände des Raums.
- **Home Assistant:** Jeder Raum erscheint als eigenes Licht (Ein/Aus, Helligkeit, alle Effekte inkl. Raum-Effekte) für alle seine Panelwände.
- **Weitere Raum-Effekte:** Raum-Feuerwerk (Raketen explodieren an Panels im ganzen Raum), Raum-Pingpong (ein Lichtpunkt springt zwischen den Panelwänden hin und her), Raum-Atmen (Atmen als Welle von der Mitte durch den Raum).
- Die anderen Panelwände brauchen dafür mindestens dieselbe Version.

Auch der Gleichtakt (unter „Mehrere Panelwände“) nimmt jetzt eigene Paletten mit ihren Farben mit; die andere Panelwand legt sie als „mitgenommene“ Palette ab, ohne ihre eigenen zu ändern.

## Weißton und Tageslicht-Kurve

Im Tab Farben stellt der Regler „Weißton“ Weiß zwischen 2200 K (warm) und 6500 K (kalt) ein; Home Assistant bekommt dafür den Farbtemperatur-Modus. Unter Optionen → Licht macht die „Tageslicht-Kurve“ alle Farben und Effekte abends wärmer und dunkler (ab 18 Uhr, nachts ganz warm auf 35 %, ab 6 Uhr wieder hell). Die Uhrzeit kommt per Internet.

## Eigene Paletten

Im Tab Effekte unter „Eigene Paletten“ lassen sich bis zu 4 Paletten mit 2 bis 6 Farben anlegen (als Vorlage gibt es „Rot-Gold-Weiß“). Sie stehen in der Palettenliste, in Home Assistant und in Presets wie die eingebauten.

## Wetter

Der Effekt „Wetter“ zeigt die Außentemperatur als Farbe (blau = kalt, gelb = mild, rot = heiß), bei Regen laufen blaue Tropfen über die Wand, bei Schnee weiße. Den Ort trägst du unter Optionen → Wetter ein; die Daten kommen alle 15 Minuten von Open-Meteo, kostenlos und ohne Anmeldung.

## Spiel „Simon sagt“

Unter Wand → Spiel startet ein Merkspiel: Die Wand zeigt eine Folge von Panels, man tippt sie nach, jede Runde kommt eines dazu. Dafür muss „Panels reagieren auf Antippen“ an sein, und es braucht mindestens zwei Panels mit Sensor. Der Rekord wird gespeichert.

## WLED nachahmen

Unter Optionen → WLED nachahmen findet die App WLED-Geräte im WLAN (oder man gibt die IP ein). Trilumag fragt das Gerät alle 1,5 s ab und übernimmt Änderungen an Ein/Aus, Helligkeit, Farbe, Tempo und Intensität, dazu den Effekt, wenn es ein Gegenstück gibt (etwa Rainbow, Breathe, Fire 2012, Aurora, Sparkle, Meteor, Lightning, Fireworks).

## Hue-Lampe nachahmen

Unter Optionen → Hue-Lampe nachahmen folgt die Wand einer Lampe der Hue Bridge (geht mit jedem Chip, übers WLAN): Bridge suchen, den runden Knopf auf der Bridge drücken und „Koppeln“ tippen, dann eine Lampe wählen. Trilumag fragt die Lampe jede Sekunde über die lokale Schnittstelle der Bridge (API v2) ab und übernimmt Ein/Aus, Helligkeit, Farbe, Weißton und die Effekte Kerze, Feuer, Funkeln, Prisma und Opal.

## Störungsanzeige

Ist das WLAN weg oder antwortet ein Panel nicht mehr, blinkt das Hauptpanel alle 10 Sekunden zweimal kurz (orange bzw. rot), solange die Wand an ist. Abschaltbar unter Optionen → WLAN.

## Wand als Bild

Unter Wand → „Wand als Bild speichern“ entsteht ein PNG mit den aktuellen Farben, Panelnummern, Chip-IDs, der Ausrichtung (Punkt = Kante 1) und den Datenverbindungen.

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
| `POST /api/set` | ein Panel: `{"id":"A3F2C1D0","state":"ON","brightness":180,"color":{"r":255,"g":0,"b":0,"w":0}}`, mehrere: `{"ids":[…],…}`, ganze Wand: `{"id":"alle","state":"ON","brightness":200,"color":{…}}` (hier ist `brightness` die Gesamthelligkeit), Helligkeit aller Panels: `{"panelBri":255}`, Weißton: `"kelvin":2700` statt `color` |
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
| `POST /api/palette` | Eigene Palette: `{"action":"save","slot":0,"name":"Rot-Gold-Weiß","colors":["#FF0000","#FFB000","#FFFFFF"]}`, `{"action":"delete","slot":0}` |
| `POST /api/weather` | Wetter: `{"place":"Wien"}` (Ort suchen und Wetter holen), `{"place":""}` löscht ihn |
| `POST /api/game` | Spiel „Simon sagt“: `{"action":"start"}` / `{"action":"stop"}` |
| `POST /api/hue`, `GET /api/hue` | Hue-Lampe nachahmen: `{"action":"find"}`, `{"action":"pair"}`, `{"action":"lights"}`, `{"action":"follow","id":"…","name":"…"}`, `{"action":"stop"}`, `{"action":"forget"}`; GET liefert Bridges und Lampen |
| `POST /api/mirror` | WLED nachahmen: `{"ip":"192.168.1.70"}`, `{"ip":""}` beendet es |
| `POST /api/fault` | Störungsanzeige: `{"on":true}` |
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
