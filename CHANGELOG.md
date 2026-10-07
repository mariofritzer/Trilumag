# Versionsverlauf

Die Versionsnummer im Webinstaller und in der App setzt sich aus der Version in der Firmware und der Nummer des automatischen Builds zusammen, zum Beispiel **0.5.11** = Version 0.5, Build 11. Bis Build 9 hat der Build die Nummer immer als 0.2.x ausgegeben, die tatsächlichen Stände stehen deshalb unten mit beiden Nummern.

## 0.7.61 · 7. Oktober 2026

**Android-App 1.0.1**
- Neuer Knopf „Nach Update suchen“ ganz unten neben der App-Version: sagt „Die App ist aktuell“ oder zeigt das Update an.
- Die App sieht bei jedem Öffnen nach einer neuen Version (auch aus dem Hintergrund), höchstens einmal pro Stunde.

## 0.7.57 · 7. Oktober 2026

**Android-App 1.0**
- Die App hat jetzt eine eigene Versionsnummer, unabhängig von der Firmware, und sie steht im Dateinamen: `trilumag-app-1.0.apk`.
- Eine neue APK gibt es nur noch, wenn sich an der App wirklich etwas ändert; die App meldet sich also nicht mehr bei jedem Firmware-Release.
- Ältere Apps finden diese Version noch einmal unter dem alten Namen und bieten sie zum Installieren an.

## 0.7.55 · 7. Oktober 2026

**Android-App**
- „Installieren“ beim Hinweis auf eine neue App-Version lädt sie jetzt in der App herunter und installiert sie selbst. Dahinter wird alles unscharf, davor stehen Fortschritt (Prozent und MB) und der aktuelle Schritt.
- Beim ersten Mal fragt Android, ob Trilumag Apps installieren darf; danach geht es von selbst weiter.
- Diese Version noch einmal wie bisher über den Browser installieren, ab dann klappt es in der App.

## 0.7.53 · 7. Oktober 2026

**Absturz beim Start behoben**
- War eine Hue-Lampe oder ein WLED-Gerät zum Nachahmen gewählt, fragte die Wand es schon beim Start ab, bevor das WLAN lief, und startete deshalb immer wieder neu. Jetzt wird erst mit WLAN abgefragt.
- Neu: sicherer Start. Stürzt die Wand dreimal hintereinander gleich beim Start ab, startet sie ohne Hue- und WLED-Nachahmen und vermerkt das im Protokoll.

**Panel tauschen**
- Die Meldung „Tausch erledigt“ kam in seltenen Fällen nicht in der App an.

## 0.7.51 · 6. Oktober 2026

**Android-App**
- Neu: `trilumag-app.apk` liegt bei jedem Release. Die App findet alle Trilumag im WLAN und zeigt sie als Liste mit Farbe, Ein/Aus-Schalter und Helligkeit, wie die WLED-App.
- Antippen öffnet die Steuerung im Vollbild; ＋ fügt eine Wand über ihre IP hinzu, langes Drücken entfernt sie.
- Die App sieht selbst nach, ob es eine neuere Version gibt, und zeigt dann oben einen Hinweis zum Herunterladen.

**Updates**
- Während ein Update geladen und installiert wird, ruhen die Abfragen für Hue und WLED; die Verbindung zur Hue Bridge bleibt offen statt jedes Mal neu aufgebaut zu werden. Das behebt, dass sich 0.7.49 bei laufendem Hue-Nachahmen nicht installieren ließ.

## 0.7.49 · 6. Oktober 2026

**Hue-Lampe nachahmen**
- Die Karte zeigt jetzt, was die Bridge über die Lampe meldet: aus, oder an mit Helligkeit und Farbe bzw. Weißton.
- Läuft die Lampe gerade im Hue Sync (Fernseher, PC), steht das dort; die Bridge gibt diese Farben nicht heraus. Die Wand bleibt dann, wie sie ist, statt auszugehen.

## 0.7.48 · 6. Oktober 2026

**Hue-Lampe nachahmen**
- Neu unter Optionen, mit jedem Chip: Die Wand folgt einer Lampe deiner Hue Bridge (Ein/Aus, Helligkeit, Farbe, Weißton und Effekte wie Kerze oder Feuer).
- Einrichten: Bridge suchen, den Knopf auf der Bridge drücken, „Koppeln“, Lampe wählen.

**Komet**
- Die Intensität bestimmt jetzt die Schweiflänge: bei 255 ein langer Schweif über viele Panels, bei 0 ein kurzer.

## 0.7.47 · 6. Oktober 2026

**Eigene Paletten**
- Im Tab Effekte bis zu 4 eigene Paletten mit 2 bis 6 Farben anlegen, ändern und löschen. Als Vorlage gibt es „Rot-Gold-Weiß“.
- Sie stehen in der Palettenliste, in Home Assistant und in Presets wie die eingebauten.

**Kanten einzeln**
- Der Komet läuft in jedem Panel alle drei Kanten nacheinander ab und springt dann von der letzten Kante ins Nachbarpanel.
- Einschalt-Animation, Fortschritt und die Welle beim Antippen laufen ebenfalls Kante für Kante.

**App**
- Farbwechsel, Weißton, Ein/Aus und Presets blenden auch in der Vorschau weich über, so wie an der Wand.

## 0.7.45 · 6. Oktober 2026

**Licht**
- Neuer Regler „Weißton“ im Tab Farben: Weiß von 2200 K (warm) bis 6500 K (kalt). Home Assistant bekommt dafür den Farbtemperatur-Modus.
- Tageslicht-Kurve (Optionen → Licht): ab 18 Uhr wärmer und dunkler, nachts warm auf 35 %, ab 6 Uhr wieder normal.

**7 neue Effekte (jetzt 24)**
- Herzschlag, Plasma, Regen, Sternenhimmel, Feuerwerk, Matrix und Wetter.
- Wetter: Farbe nach Außentemperatur, bei Regen blaue, bei Schnee weiße Tropfen. Ort unter Optionen → Wetter (Daten von Open-Meteo, ohne Anmeldung).

**Neu**
- Spiel „Simon sagt“ unter Wand: Folge nachtippen, jede Runde ein Panel mehr, Rekord wird gespeichert.
- WLED nachahmen (Optionen): Trilumag übernimmt Ein/Aus, Helligkeit, Farbe, Tempo, Intensität und passende Effekte eines WLED-Geräts im WLAN.
- Störungsanzeige: Hauptpanel blinkt kurz orange (WLAN weg) oder rot (Panel antwortet nicht). Abschaltbar unter Optionen → WLAN.
- Wand als Bild speichern: PNG mit Farben, Panelnummern, Chip-IDs, Ausrichtung und Datenverbindungen.

**App**
- Die Vorschau der Wand läuft flüssiger: 25 Bilder pro Sekunde, und der Zustand wird nur noch bei echten Änderungen geschickt (vorher wegen des Stroms mehrmals pro Sekunde).

## 0.7.42 · 6. Oktober 2026

**Updates auf mehreren Wänden**
- Unter Optionen → Updates wählst du jetzt aus, wo eine Version installiert wird: diese Wand und/oder die gefundenen anderen Trilumag im WLAN.
- Die anderen Wände holen sich die passende Datei für ihren Chip selbst; diese Wand kommt zuletzt dran.
- Neben jeder Wand steht, wie weit das Update ist. Funktioniert auch mit älteren Wänden ab etwa 0.7.21.

## 0.7.40 · 6. Oktober 2026

**Malen**
- Die Karte „Malen“ ist in den Tab Farben umgezogen. Gemalt wird auf der Vorschau oben; sie bleibt beim Malen unter der Kopfzeile stehen, damit Farben und Wand gleichzeitig zu sehen sind.
- Beim Wechsel in einen anderen Tab schaltet sich Malen von selbst aus.

## 0.7.39 · 6. Oktober 2026

**Andere Trilumag im WLAN**
- Neu unter Optionen: „Andere Trilumag“ listet alle weiteren Wände im selben WLAN mit Name, Adresse und Version auf. „Öffnen“ führt direkt zu deren Steuerungsseite.
- Wände mit älterer Firmware erscheinen unter ihrer Adresse (etwa „trilumag-2“), ab dieser Version mit ihrem Namen.

**Wand**
- Neue Karte „Alle Panels“: Helligkeit aller Panels auf einmal stellen. Neue Panels starten mit 100 % statt 71 %.
- Ein feiner weißer Strich zwischen zwei Panels zeigt, über welche Kante der Bus zum vorigen Panel läuft.
- Simulation: Panels lassen sich an Ort und Stelle drehen (der Punkt markiert Kante 1).
- Simulation: Panels in der Ablage einzeln löschen (✕) oder die Ablage leeren.

**App**
- Der Name der Wand oben links wird auf schmalen Handys nicht mehr abgeschnitten; die Statuszeile daneben kürzt sich stattdessen.

## 0.7.37 · 5. Oktober 2026

**WLAN-Wächter und Neustart-Grund**
- Neu unter Optionen → WLAN, ein- und ausschaltbar (Standard: an).
- Ist das WLAN 3 Minuten weg, verbindet sich Trilumag neu; nach 10 Minuten startet es neu, außer jemand ist gerade im Setup-Netz.
- Hängt die Software eine Minute, startet Trilumag ebenfalls neu. trilumag.local wird alle 30 Minuten neu angekündigt.
- Unter Info stehen jetzt „Letzter Neustart“ (Stromausfall, Update, Absturz, Wächter …) und die Zahl der Abstürze.

**Panel tauschen**
- Unter Wand das Panel antippen und „Tauschen“ wählen, dann bleiben 5 Minuten: altes Panel abklipsen, neues an dieselbe Stelle.
- Das neue übernimmt Farbe, Helligkeit und „Kanten einzeln“. Ein kleines Fenster unten zeigt die Restzeit, ✕ bricht ab.
- In der Simulation kommen Panels, die hinter dem alten hingen, danach von selbst an ihren Platz zurück.

**Effekte**
- Neuer Effekt Farbverlauf: von Farbe 1 zu Farbe 2 und zurück, Farbe 2 im Tab Farben. Mit Tempo wandert der Verlauf.
- Favoriten: Stern pro Effekt, Favoriten stehen oben.
- Jeder Effekt hat in der Liste eine kleine animierte Vorschau.

**App**
- Die App gibt es auf Englisch. Sie richtet sich nach der Browsersprache und lässt sich unter Optionen → Name fest einstellen.
- Die App wird gepackt ausgeliefert, lädt schneller und braucht rund 100 KB weniger Flash.

## 0.7.35 · 5. Oktober 2026

**Effekt-Richtung**
- Im Tab Effekte gibt es den neuen Regler „Richtung“ in 15°-Schritten einmal ringsum, also nach rechts, nach unten, nach links und so weiter.
- „Richtung dreht sich“: nein, langsam (eine Runde pro Minute) oder schnell (eine Runde in 12 s).
- Wirkt bei Effekten, die über die Wand laufen: Regenbogen, Regenbogenwelle, Lauflicht, Polarlicht, Lava und Spirale.
- Presets und der Gleichtakt mehrerer Wände übernehmen die Richtung. Alle Wände einer Gruppe brauchen dafür diese Version.

**App**
- Farbwechsel blenden jetzt auch in der App-Vorschau weich über, so wie an der Wand.
- Das Kästchen „Philips Hue (Zigbee)“ steht direkt unter „Home Assistant (MQTT)“.

**Fehlerbehebung**
- Die Welle beim Antippen, das Signal und die Einschalt-Animation fielen manchmal aus, wenn sie im falschen Moment ausgelöst wurden.

## 0.7.34 · 5. Oktober 2026

**7 neue Effekte (jetzt 16)**
- Lauflicht, Spirale, Gewitter, Kerzenlicht, Disco, Komet und Lava.
- Paletten, Tempo und Intensität wirken bei allen. Auch in Home Assistant und WLED-Programmen auswählbar.

**Welle beim Antippen**
- Tippst du ein Panel an, läuft ein heller Ring von dort über die Wand und verblasst. Er kommt zusätzlich zur eingestellten Aktion.
- Lässt sich unter Optionen → Antippen abschalten.

**Einschalt-Animation**
- Beim Einschalten leuchten die Panels der Reihe nach vom Hauptpanel aus auf.
- Unter Optionen → Licht wählbar: aus, langsam, mittel (Standard) oder schnell.

**Ansicht drehen und spiegeln**
- Unter Wand lässt sich die Ansicht in 30°-Schritten drehen oder spiegeln, damit sie zur echten Wand passt.
- Die Einstellung gilt auch für die Effekte: Links und rechts, oben und unten passen dann zu deiner Wand.

**Signal und Fortschritt (Home Assistant)**
- Das neue Gerät „Signal“ lässt die Wand blinken, z. B. mit der Nachricht „blau 3“. Danach läuft alles weiter wie vorher, auch ein Effekt.
- Die Zahl „Fortschritt“ (0 bis 100 %) füllt die Wand vom Hauptpanel aus. 0 % schaltet die Anzeige aus.
- Unter Optionen lassen sich beide ausprobieren.

## 0.7.31 · 5. Oktober 2026

**Name der Wand**
- Unter Optionen → Name bekommt die Wand einen eigenen Namen. Er steht oben in der App, im Browser-Tab, in Home Assistant und in WLED-Programmen.
- Die Adresse bleibt trilumag.local.

**Panel finden**
- Unter Wand gibt es beim angetippten Panel den Knopf „Finden“. Das echte Panel blinkt dann 3 Sekunden weiß, auch wenn die Wand aus ist, und in der App blinkt es mit.

**Malen**
- Unter Wand gibt es das neue Kästchen „Malen“: eine Farbe wählen und mit dem Finger über die Panels wischen.
- Zur Wahl stehen 8 Farben, Warmweiß, Kaltweiß, „Aus“ und eine eigene Farbe.

**Sleep-Timer**
- Oben neben der Helligkeit sitzt ein Mond-Knopf mit 15 min bis 2 h. Die Wand blendet am Ende langsam aus (höchstens 5 Minuten) und geht dann aus.
- Am Mond steht die Restzeit. Ein- oder Ausschalten von Hand beendet den Timer.

**Nach Stromausfall**
- Unter Optionen → Licht lässt sich wählen: wie vorher, aus, an oder ein bestimmtes Preset.
- Das gilt nur nach echtem Stromausfall. Nach Updates und Neustarts aus der App bleibt alles, wie es war.

**Energie**
- Neues Kästchen unter Optionen mit dem Verbrauch von heute, diesem Monat, diesem Jahr und gesamt, dazu ein Balkendiagramm nach Tagen, Monaten oder Jahren.
- Gemessen wird mit dem INA226, ohne Sensor wird der Verbrauch geschätzt. Das Datum kommt per Internet (NTP), Zeitzone Österreich.
- In Home Assistant gibt es den Zähler „Energie“ (kWh) für das Energie-Dashboard.

**Betriebsstunden**
- Jedes Panel zählt, wie lange es schon geleuchtet hat.
- Das steht als „Leuchtdauer“ beim Panel unter Wand und als Spalte „Std.“ in der Bus-Diagnose.

**Mehrere Wände im Gleichtakt**
- Wände im selben WLAN lassen sich zu einer Gruppe (1 bis 9) verbinden. Sie teilen Ein/Aus, Helligkeit und Effekt, egal an welcher Wand man etwas ändert.
- Die Effekte laufen im selben Takt. Gefundene Wände erscheinen mit ihrem Namen in der Liste.

## 0.7.27 · 5. Oktober 2026

**App**
- Das Kästchen „Philips Hue (Zigbee)“ steht jetzt bei jedem Chip unter Optionen. Ohne ESP32-C6 ist es ausgegraut, mit dem Hinweis „Nur mit ESP32-C6“.

## 0.7.26 · 5. Oktober 2026

**Philips Hue (nur ESP32-C6)**
- Mit einem ESP32-C6 als Hauptpanel meldet sich Trilumag zusätzlich als Zigbee-Farblampe „Trilumag Wand“. Eine Hue Bridge, auch die Bridge Pro, nimmt sie wie eine Lampe eines anderen Herstellers auf.
- Hue steuert Ein/Aus, Helligkeit und Farbe der ganzen Wand, auch in Szenen, Routinen und mit Hue-Schaltern. Weißtöne gehen auf die weißen LEDs.
- Änderungen aus der App oder Home Assistant sieht die Hue-App nach etwa einer Sekunde.
- Unter Optionen gibt es das Kästchen „Philips Hue (Zigbee)“. Dort schaltest du Zigbee ein und koppelst neu. Gesucht wird in der Hue-App unter Einstellungen → Lampen → „+“.
- Der C6 braucht dafür einmal die neue Speicheraufteilung: einmal mit dem Webinstaller flashen, Einstellungen und Wand bleiben erhalten.
- Startet das Hauptpanel mit Zigbee dreimal nicht richtig, schaltet es Zigbee von selbst wieder aus.
- Alle anderen Chips haben kein Zigbee.

**App**
- Unter Stromlimit lässt sich der Bereich „Stromsensor INA226“ auf- und zuklappen. Zugeklappt zeigt er an, ob der Sensor gefunden wurde.
- In der Bus-Diagnose lassen sich die Ereignisse auf- und zuklappen. Zugeklappt steht die Zahl der Einträge daneben.

## 0.7.23 · 4. Oktober 2026

**Antippen**
- Panels mit Bewegungssensor (LIS2DH12) reagieren auf Antippen.
- Unter Optionen gibt es das Kästchen „Antippen“. Dort stellst du ein, was einmal und was doppelt Antippen tut: Panel ein/aus, Wand ein/aus, nächstes Preset oder nächster Effekt. Auch die Empfindlichkeit (1 bis 10) lässt sich dort einstellen.
- Das angetippte Panel leuchtet in der App kurz auf.
- In Home Assistant gibt es das Ereignis „Antippen“ für eigene Automationen, mit der Art (einmal oder doppelt) und dem Panel.
- In der Simulation probierst du es unter Wand aus: Panel antippen, dann „einmal“ oder „doppelt“.

**Panel-Updates über den Bus**
- Die Panels haben jetzt einen eigenen Bootloader. Das Hauptpanel bringt die passende Panel-Firmware mit und spielt sie über den Bus auf. Pro Panel dauert das etwa eine Sekunde, die Wand leuchtet dabei weiter.
- Unter Optionen gibt es dafür das Kästchen „Panel-Firmware“. Es zeigt veraltete Panels mit Fortschritt an. Du kannst einzelne oder alle Panels aktualisieren oder das automatisch erledigen lassen (Standard).
- Bricht ein Update ab, etwa weil der Strom ausfällt, bleibt das Panel im Bootloader. Das Hauptpanel findet es dort von selbst und spielt die Firmware fertig auf.
- Ein neues Panel wird einmal mit dem WCH-LinkE programmiert (`make flashall`). Danach kommen alle Updates über den Bus.
- Panel-Firmware und Bootloader liegen bei jedem Release als Dateien bei.

**Anklips-Zähler**
- Jedes Panel zählt, wie oft es angeklipst wurde. Der Zähler bleibt dauerhaft gespeichert.
- Er steht unter Wand beim Panel und als Spalte in der Bus-Diagnose.
- Neustarts, das Einschalten der ganzen Wand und Updates zählen nicht mit.

**Kanten einzeln**
- Unter Wand gibt es eine Liste aller Panels zum An- und Abhaken, dazu „Alle an“ und „Alle aus“.

**Panel-Hardware (Schaltplan aktualisiert)**
- Die Steuerung im Panel läuft jetzt mit 3,3 V. Neu sind ein Spannungsregler, ein Bus-Baustein für 3,3 V (MAX3485) und ein Pegelwandler für die LED-Daten.
- Kante 2 liegt jetzt auf PC4, Kante 3 auf PC0. PC1 und PC2 sind der I²C-Bus für den Sensor.

**Fehlerbehebung**
- Beim Umschalten von „Kanten einzeln“ blieb die Vorschau der Wand manchmal hängen, wenn ein Effekt lief.
- Ein Panel, das kurz keinen Kontakt hatte, aber noch Strom hat, wird jetzt zuverlässig neu erkannt. Das gilt auch für die Panels dahinter.

## 0.7.21 · 4. Oktober 2026

**Fehlerbehebung: weiße Seite**
- Nach 0.7.20 blieb die App bei manchen Geräten weiß. Die rund 70 KB große Seite wurde vor dem Senden komplett in den Arbeitsspeicher kopiert, und dafür war nicht immer genug Platz frei.
- Jetzt wird sie direkt aus dem Flash gesendet, ohne Kopie.

**Notfall-Seite `/update`**
- Unter `http://trilumag.local/update` gibt es eine kleine Seite, die auch dann lädt, wenn die App selbst nicht geht.
- Ein Knopf installiert die neueste Version von GitHub, alternativ lässt sich eine Firmware-Datei hochladen.

## 0.7.20 · 4. Oktober 2026

**Stromlimit**
- Eigenes Kästchen „Stromlimit“ unter Optionen, mit Anzeige des aktuellen Stroms.
- Die Pins für den Stromsensor INA226 wählst du direkt dort. Sie gelten sofort, ohne Neustart.
- Jedes Board hat eine Vorgabe, die mit ★ markiert ist: S3-DevKitC SDA 1 / SCL 2, ESP32-DevKit 21 / 22, C3 SuperMini 5 / 6, C6 6 / 7.
- Ist ein Sensor-Pin schon für den Bus oder die LEDs belegt, wird er abgelehnt.

**Bus-Diagnose**
- Neues Kästchen unter Optionen: pro Panel Adresse, Antwortzeit, verpasste Antworten und Panel-Firmware.
- Dazu Zähler für gesendete Bilder, Erkennungsrunden, verpasste Antworten und gestörte Übertragungen.
- Ein Ereignisprotokoll zeigt Anklipsen, Abklipsen, Probleme bei der Erkennung und Neustarts.
- Die Bus-Werte stehen auch beim angetippten Panel unter Wand.

**Kanten einzeln**
- Unter Wand lässt sich „Kanten einzeln“ pro Panel einschalten. Effekte geben dem Panel dann drei Farben, eine je Kante.
- Wellen und Verläufe laufen so sichtbar durch die Dreiecke. Die Vorschau zeigt die drei Teile.
- Panel-Firmware 2 bringt dafür den Sammelbefehl FRAME3 mit. Panels mit Firmware 1 bekommen die drei Farben über den vorhandenen Befehl EDGES und funktionieren ohne neue Firmware.

**WLED-kompatible Schnittstelle**
- Trilumag meldet sich zusätzlich als WLED-Gerät mit einem Segment, der ganzen Wand.
- Unterstützt werden `/json`, `/json/state`, `/json/info`, `/json/eff`, `/json/pal` und `/presets.json`.
- Damit funktionieren die WLED-Integration von Home Assistant und andere WLED-Programme: Ein/Aus, Helligkeit, Farbe, Effekt, Tempo, Intensität, Palette und Presets.

**Schaltplan**
- Der optionale Stromsensor U6 (INA226) mit Shunt R14 (5 mΩ) ist im Schaltplan des Hauptpanels eingetragen.

## 0.7.18 · 4. Oktober 2026

**Update-Absicherung**
- Eine neue Version muss sich nach dem Start 45 Sekunden bewähren. Stürzt sie in dieser Zeit ab, hängt sie oder fällt der Strom aus, nimmt das Gerät beim nächsten Start automatisch wieder die vorige Version.
- Die App meldet „Zurück auf die vorige Version“ und markiert die Version in der Liste mit „startete nicht“. Automatische Updates probieren sie nicht noch einmal.
- Fällt der Strom während des Downloads aus, läuft ohnehin die alte Version weiter, weil das Update in den freien Programmplatz geschrieben wird.

**Licht**
- Weiche Übergänge: Farb-, Preset-, Effekt- und Ein/Aus-Wechsel blenden über eine einstellbare Zeit über (Standard 0,7 s).
- Stromlimit wie bei WLED: Trilumag schätzt den Strom aller Panels und dimmt gleichmäßig, bevor das Netzteil überlastet wird.
- Optionaler Stromsensor INA226 über I²C: misst Strom, Spannung und Leistung. Mit ihm regelt das Stromlimit nach Messung.
- Home Assistant: neue Sensoren Strom, Leistung und (mit Sensor) Spannung.
- Neue Panels pulsieren jetzt über das Hauptpanel. Alle Farben kommen über einen gemeinsamen Weg mit Überblendung und Stromlimit an die Panels.

**Sichern und Wiederherstellen**
- Unter Optionen → Sicherung lassen sich Einstellungen, Presets, Farben und die simulierte Wand als Datei herunterladen und wieder einspielen. Die WLAN-Zugangsdaten sind nicht dabei.

**Behoben**
- Der Farbtest unter Hardware wird nicht mehr sofort vom normalen Bild überschrieben.

## 0.6.16 · 4. Oktober 2026

**Simulation**
- Die simulierte Wand bleibt erhalten. Angeklipste Panels mit Position und Drehung und die Ablage übersteht jetzt Neustarts und Updates. Vorher fing die Simulation nach jedem Neustart mit leerer Wand an.

## 0.6.15 · 4. Oktober 2026

**Updates**
- In der Versionsliste der App zeigt „Änderungen anzeigen“ zu jeder Version den vollständigen Abschnitt aus dem Versionsverlauf. Die App lädt ihn direkt von der Installer-Seite. Ohne Internet am Handy verweist sie auf GitHub.

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
