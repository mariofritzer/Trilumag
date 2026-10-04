# Trilumag-Bus-Protokoll v1 (Panel-Firmware 3)

Das Hauptpanel (ESP32) ist der einzige Master. Panels (CH32V003) senden nur, wenn sie gefragt werden. Physisch läuft alles über RS-485 halbduplex mit **250 000 Baud, 8N1**.

## Rahmen

Beide Richtungen benutzen denselben Aufbau:

| Byte | Inhalt |
| --- | --- |
| 0 | `0xA5` Startbyte |
| 1 | Adresse |
| 2 | Befehl |
| 3 | Länge der Daten (0 bis 200) |
| 4 … | Daten |
| letztes | CRC-8 (Polynom `0x07`, Startwert `0x00`) über Adresse, Befehl, Länge und Daten |

**Adressen vom Master an Panels**

| Adresse | Bedeutung |
| --- | --- |
| `0x00` | alle Panels (Broadcast, keine Antwort) |
| `0x01`–`0x3E` | ein Panel mit dieser Kurzadresse |
| `0x7F` | ein noch nicht adressiertes Panel, das über seine Chip-ID angesprochen wird. Die Daten beginnen mit den 4 Bytes Chip-ID (little endian) |

**Antworten von Panels** tragen als Adresse `0x80 | eigene Kurzadresse`, ein noch nicht adressiertes Panel also `0x80`. Der Befehl der Antwort ist derselbe wie der der Anfrage.

Ein Panel antwortet frühestens 100 µs nach dem Ende der Anfrage. Der Master wartet höchstens 3 ms auf eine Antwort.

## Befehle

| Code | Name | Ziel | Daten | Antwort |
| --- | --- | --- | --- | --- |
| `0x01` | PING | Panel | – | `[Zustand, SNS-Maske, Firmware-Version, Antippen, Fähigkeiten]` (Version 2 kennt FRAME3, ab Version 3 die letzten zwei Bytes) |
| `0x02` | COLOR | Panel oder alle | `[R, G, B, W]` | – |
| `0x03` | EDGES | Panel | `[R,G,B,W] × 3` für Kante 1 bis 3 | – |
| `0x04` | FRAME | alle | `[erste Adresse, Anzahl, (R,G,B,W) × Anzahl]` | – |
| `0x05` | PULSE | Panel | – | – |
| `0x06` | ORDER | Panel oder alle | 4 Bytes: welcher Farbkanal an Position 1–4 gesendet wird (0 = R, 1 = G, 2 = B, 3 = W) | – |
| `0x08` | TOUCH | Panel oder alle | `[Schwelle]` in 16 mg, 0 = Antippen aus (ab Firmware 3) | – |
| `0x07` | FRAME3 | alle | `[erste Adresse, Anzahl, (R,G,B,W) × 3 × Anzahl]`, drei Farben pro Panel für Kante 1 bis 3, ab Panel-Firmware 2 | – |
| `0x10` | BEACON | alle | `[1 = an, 0 = aus]` | – |
| `0x11` | DISCOVER | alle | `[Runde, Zeitschlitze]` | nur neue Panels mit Kontakt: `[Chip-ID ×4, SNS-Maske]` |
| `0x12` | PROBE | `0x7F` | `[Chip-ID ×4, Kante oder 0xFF]` | `[]` |
| `0x14` | ASSIGN | `0x7F` | `[Chip-ID ×4, Kurzadresse]` | `[frisch]` von der neuen Adresse (ab Firmware 3: 1 = seit dem Einschalten die erste Adresse) |
| `0x15` | RESET | Panel oder alle | – | – |
| `0x20` | BOOT | Panel | – | `[1 = springt in den Bootloader, 0 = keiner da]` |

**Antippen (PING-Byte 4):** Bits 0–1 = Art des letzten Ereignisses (1 = einmal, 2 = doppelt), Bits 2–7 = laufende Nummer. Ändert sich das Byte, hat jemand das Panel angetippt.
**Fähigkeiten (PING-Byte 5):** Bit 0 = Bewegungssensor LIS2DH12 gefunden, Bit 1 = Bootloader vorhanden.
**Zustände:** 0 = dunkel, 1 = pulsiert blau, 2 = leuchtet in zugewiesener Farbe.
**SNS-Maske:** Bit 0 = Kante 1, Bit 1 = Kante 2, Bit 2 = Kante 3. Ein gesetztes Bit heißt: Diese SNS-Leitung liest gerade Low.
Die Farbwerte in COLOR, EDGES und FRAME sind bereits mit der Helligkeit verrechnet.

## SNS-Leitungen

Jede Kante hat eine eigene SNS-Leitung, die nur die beiden sich berührenden Kanten verbindet. Jede Seite hat 100 Ω in Serie und arbeitet entweder als **Leuchtfeuer** (Ausgang Low) oder als **Fühler** (Eingang mit Pull-up).

- Adressierte Panels und das Hauptpanel sind standardmäßig Leuchtfeuer auf allen Kanten.
- Neue, noch nicht adressierte Panels sind Fühler. Ein Low an einer Kante heißt: Dort hängt ein bereits eingegliedertes Panel.
- Für die Ortung schaltet der Master mit `BEACON 0` alle Leuchtfeuer ab. Dann zieht das neue Panel auf Befehl `PROBE` genau eine seiner Kanten auf Low, und das Nachbarpanel sieht das in seiner SNS-Maske.

## Ablauf beim Anklipsen

1. Das neue Panel startet dunkel, ohne Adresse, alle SNS-Leitungen als Fühler.
2. Der Master schickt alle 400 ms `DISCOVER`. Neue Panels mit mindestens einer Low-Kante antworten in ihrem Zeitschlitz (abhängig von Chip-ID und Runde, damit sich zwei gleichzeitig angeklipste Panels nicht dauerhaft überlagern).
3. Für jede gemeldete Kante: `BEACON 0`, `PROBE` an das neue Panel, dann `PING` an alle eingegliederten Panels und Lesen der eigenen SNS-Pins. Wer jetzt Low sieht, ist der Nachbar an dieser Kante. Danach `PROBE 0xFF` und `BEACON 1`.
4. Aus Nachbar, dessen Kante und der eigenen Kante des neuen Panels berechnet der Master Position und Drehung im Raster.
5. `ASSIGN` vergibt die kleinste freie Kurzadresse. Ab jetzt ist das Panel selbst Leuchtfeuer, und dahinter angeklipste Panels werden in der nächsten Runde gefunden.
6. Der Master schickt `ORDER` mit der eingestellten Farbreihenfolge. Ist die Chip-ID bekannt, bekommt das Panel per `COLOR` seine alte Farbe. Sonst schickt der Master `PULSE`, und das Panel pulsiert blau, bis die erste Farbe kommt.

## Abklipsen

Der Master fragt alle eingegliederten Panels etwa alle 150 ms mit `PING` ab. Antwortet ein Panel dreimal hintereinander nicht, gilt es als abgeklipst. Danach prüft der Master, welche Panels noch über das Raster mit dem Hauptpanel verbunden sind. Die anderen gelten ebenfalls als getrennt, sie haben ja auch keinen Strom mehr.

## Anklips-Zähler

Das Hauptpanel zählt pro Chip-ID dauerhaft, wie oft ein Panel angeklipst wurde. Gezählt wird nur, wenn das Panel bei ASSIGN „frisch“ meldet, also wirklich gerade Strom bekommen hat. Nach einem Neustart des Hauptpanels oder einem Update meldet es 0. Beim Einschalten der ganzen Wand wird nicht gezählt: bis 5 s nach dem letzten gefundenen Panel, mindestens 15 s nach dem Start.

## Panel-Updates über den Bus

Im Boot-Bereich des CH32V003 (1920 Bytes) liegt der Trilumag-Bootloader (`firmware/panel/bootloader`). Das Option-Byte lässt das Panel nach jedem Einschalten dort beginnen. Ist die Firmware vollständig, startet er sie sofort.

Ablauf beim Update (das Hauptpanel trägt die aktuelle Panel-Firmware in sich):

1. `BOOT` an die Kurzadresse. Das Panel antwortet und springt in den Bootloader.
2. `BL_INFO`, bis der Bootloader antwortet.
3. `BL_WRITE` für jede Seite zu 64 Bytes. Vor der ersten Seite markiert der Bootloader die Firmware als unvollständig (letzte Flash-Seite).
4. `BL_DONE` mit Seitenzahl und CRC-16/CCITT. Passt sie, löscht der Bootloader die Markierung und startet die neue Firmware.
5. `ASSIGN` mit der alten Kurzadresse, danach `ORDER` und `TOUCH`. Eine Neuerkennung ist nicht nötig.

Bricht das Update ab (Strom weg, Hauptpanel neu gestartet), bleibt das Panel im Bootloader. Das Hauptpanel fragt alle 5 s mit `BL_SCAN`, findet es und spielt die Firmware automatisch neu auf.

| Code | Name | Ziel | Daten | Antwort |
| --- | --- | --- | --- | --- |
| `0x21` | BL_INFO | `0x7F` | `[Chip-ID ×4]` | `[Bootloader-Version, Firmware ok]` |
| `0x22` | BL_WRITE | `0x7F` | `[Chip-ID ×4, Seite, 64 Bytes]` | `[1 = geschrieben und geprüft]` |
| `0x23` | BL_DONE | `0x7F` | `[Chip-ID ×4, Seiten, CRC lo, CRC hi, Version]` | `[1 = passt]`, danach Neustart |
| `0x24` | BL_SCAN | alle | `[Runde, Zeitschlitze (Zweierpotenz)]` | nur Panels im Bootloader: `[Chip-ID ×4, Firmware ok]` |
| `0x25` | BL_RUN | `0x7F` | `[Chip-ID ×4]` | `[1]`, danach Start der Firmware |

Antworten des Bootloaders tragen die Adresse `0x80`.

## Start

Beim Start schickt der Master `RESET`, damit alle Panels ihre Adresse vergessen, und dann `ORDER` mit der eingestellten Farbreihenfolge. Danach findet die normale Erkennung die Wand Panel für Panel neu.
