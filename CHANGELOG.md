# Versionsverlauf

Die Versionsnummer im Webinstaller und in der App setzt sich aus der Version in der Firmware und der Nummer des automatischen Builds zusammen, zum Beispiel **0.5.11** = Version 0.5, Build 11. Bis Build 9 hat der Build die Nummer immer als 0.2.x ausgegeben, die tatsächlichen Stände stehen deshalb unten mit beiden Nummern.

## 0.6.14 · 4. Oktober 2026

**Wichtig: bitte einmal über den Webinstaller installieren**
- Die Speicheraufteilung bietet jetzt 1,9 MB pro Programm statt 1,25 MB. Mit 0.6.13 war die Firmware für ESP32-C3 und C6 dafür zu groß, beim ESP32 und S3 war es knapp. Ein Online-Update hätte dort nicht geklappt. Die neue Aufteilung kommt nur über den Webinstaller aufs Gerät, danach gehen Online-Updates.
- WLAN, Einstellungen, Farben und Presets bleiben erhalten, ihr Speicherbereich liegt an derselben Stelle.

## 0.6.13 · 4. Oktober 2026

**Online-Updates**
- Die App zeigt unter Optionen → Updates alle verfügbaren Versionen an, auch ältere für Downgrades, und installiert sie auf Knopfdruck.
- Mit „Automatisch aktualisieren“ installiert das Hauptpanel neue Versionen von selbst. Es sieht alle 6 Stunden nach.
- Gibt es eine neue Version, zeigt der Tab Optionen einen Punkt.
- Eigene Builds lassen sich als Datei hochladen.
- Die Firmware jeder Version liegt als GitHub-Release bereit. Die Downloads prüft das Hauptpanel über HTTPS mit den Stammzertifikaten. Der Build kontrolliert jedes Mal, dass die echten Zertifikate der GitHub-Server dazu passen.

**Webinstaller**
- Das Hauptpanel beginnt jede Antwort an den Webinstaller mit einem Zeilenwechsel. Vorher konnte ein halb ausgegebener Log-Text die Antwort verdecken, und der Installer bot kein WLAN an.

**App**
- Zwischen den Kästchen ist wieder Abstand.
- Jedes Kästchen lässt sich über seine Überschrift zu- und aufklappen. Die App merkt sich das.
- WLAN: deutliche Anzeige „Verbunden mit …“ mit Empfangsqualität, Signalstärke in dBm und IP-Adresse.
- Wand: Die Ablage steht über der Wand. Beim Ziehen eines Panels aus der Ablage rückt die Wand ins Bild.
- Trilumag-Logo als Symbol im Browser-Tab.

## 0.5.11 · 4. Oktober 2026

**Verbindung**
- Die App hält wie WLED eine feste Verbindung zum Hauptpanel (WebSocket auf Port 81). Zustand und Effektbild kommen von selbst, etwa 15-mal pro Sekunde, Befehle laufen über dieselbe Verbindung. Ist sie weg, fragt die App per HTTP nach, bis sie wieder steht. Oben rechts steht „live“ oder „HTTP“.
- Die festen Listen (Effekte, Paletten) kommen nur noch beim ersten Mal mit.
- Ist ein MQTT-Broker eingetragen, aber nicht erreichbar, hängt das Hauptpanel nicht mehr bis zu 3 Sekunden.

**App**
- Gesamthelligkeit und Panel-Helligkeit in Prozent.
- Schalter „MQTT aktiv“. Ausgeschaltet bleiben Adresse und Zugangsdaten gespeichert.
- Gefundene WLANs als Liste zum Antippen, mit Signalstärke. Ohne WLAN sucht die App beim Öffnen der Optionen gleich selbst.

**Webinstaller**
- Updates behalten WLAN, Einstellungen, Farben und Presets. Vorher hat jedes Update diesen Speicherbereich gelöscht. Nur „Gerät löschen“ setzt noch alles zurück.
- Die WLAN-Suche wiederholt sich, wenn sie fehlschlägt, zum Beispiel während das Hauptpanel gerade selbst verbinden will.

## 0.4.10 · 4. Oktober 2026

**Neue Web-App im Stil von WLED**
- Oben Ein/Aus und Gesamthelligkeit der ganzen Wand, unten die Tabs Farben, Effekte, Wand, Presets und Optionen.
- Farbrad mit Helligkeit, Weißanteil, Schnellfarben (auch Warm- und Kaltweiß), Hex-Eingabe und Zufall.
- Vorschau der Wand, die live mitleuchtet. Angetippte Panels bekommen ihre eigene Farbe.
- Läuft ein Effekt mit Effektfarbe, färbt das Farbrad den Effekt ein, statt ihn zu beenden. „Einfarbig“ ist ein eigener Eintrag in der Effektliste.

**Effekte**
- 10 Paletten: Standard, Regenbogen, Effektfarbe, Ozean, Lava, Wald, Sonnenuntergang, Party, Pastell und Eis.
- Regler „Intensität“ für alle Effekte.

**Presets**
- Bis zu 16 gespeicherte Szenen mit Ein/Aus, Gesamthelligkeit, Effekt und der Farbe jedes Panels.

**Home Assistant**
- Neu: Auswahl „Preset“, Auswahl „Palette“ und Regler „Effekt-Intensität“.
- „Alle Panels“ steuert die Gesamthelligkeit.

**Behoben**
- Ein Tippen auf ein Panel in der Vorschau ging manchmal verloren.

*Builds 8 und 9 sind fehlgeschlagen. Der Build schreibt Compiler-Fehler seitdem direkt in den fehlgeschlagenen Lauf und übernimmt die Version aus der Firmware.*

## 0.3 (Build 7, angezeigt als 0.2.7) · 4. Oktober 2026

**WLAN**
- Die Verbindung läuft im Hintergrund. Der Webinstaller bekommt sofort eine Antwort und zeigt „Connect to Wi-Fi“ bzw. „Change Wi-Fi“ wieder an.
- Die Einrichtung über den Webinstaller hat 30 Sekunden Zeit und meldet auch eine spätere Verbindung als Erfolg.
- Klappt ein neues WLAN nicht, geht das Hauptpanel zurück ins alte.

## 0.3 (Build 6, angezeigt als 0.2.6) · 4. Oktober 2026

**Effekte**
- Ein Effektwechsel passiert sofort.
- Der Webserver gibt leere Browser-Verbindungen nach 200 ms frei, statt bis zu 5 Sekunden zu warten.
- Der WLAN-Sparmodus ist aus.
- Jeder Effekt beginnt von vorn, „Atmen“ startet hell.
- Neu angeklipste Panels pulsieren bei laufendem Effekt erst 5-mal blau (etwa 8 Sekunden) und machen dann mit.

## 0.3 (Build 5, angezeigt als 0.2.5) · 4. Oktober 2026

**Effekte**
- Effekte für die ganze Wand: Regenbogen, Regenbogenwelle, Atmen, Farbwechsel, Funkeln, Ausbreiten, Feuer, Polarlicht.
- Einstellbar sind Tempo, Helligkeit und Effektfarbe.
- Das Hauptpanel rechnet die Bilder selbst und schickt sie mit einem FRAME-Befehl an alle Panels. Die Panel-Firmware bleibt unverändert.
- Erreichbar über die App, die HTTP-API (`/api/effect`) und Home Assistant (Effektliste und Regler „Effekt-Tempo“).

## 0.2 (Build 4) · 4. Oktober 2026

**Bus und Panels**
- Echter RS-485-Bus nach [Protokoll v1](docs/protokoll.md): Erkennung über die SNS-Leitungen, Adressvergabe, Abklipsen und Wiederanklipsen.
- Firmware für die Panels (CH32V003), auf der Installer-Seite zum Herunterladen.

**Einstellungen**
- Pin-Vorlagen für ESP32-S3-DevKitC, ESP32-DevKit, ESP32-C3 SuperMini und ESP32-C6, eigene Belegung mit Prüfung.
- Farbreihenfolge der LEDs mit Farbtest.
- MQTT-Broker in der App einstellbar.

## 0.1 (Builds 1 bis 3) · 4. Oktober 2026

**Erste Version**
- Firmware für das Hauptpanel mit Simulation: Panels werden in der App an freie Kanten gezogen und schnappen ein.
- Neue Panels sind erst dunkel und pulsieren dann blau, bis sie ihre erste Farbe bekommen.
- Webinstaller auf GitHub Pages mit WLAN-Einrichtung (Improv).
- Automatischer Build für ESP32, ESP32-S3 und ESP32-C3, mit Build 3 auch ESP32-C6.
- HTTP-API und Home Assistant über MQTT mit Auto-Discovery.
