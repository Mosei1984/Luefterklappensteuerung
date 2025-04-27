Lüfterklappe Steuerung
Eine präzise Steuerung für Lüfterklappen mit Schrittmotor, TMC2209 Treiber und Endschaltern, implementiert auf einem Raspberry Pi Pico.

Funktionen:
Automatisches Homing mit Min- und Max-Endschaltern
Konfigurierbare Software-Endanschläge
TMC2209 UART-Steuerung mit StallGuard-Erkennung
Fehlerbehandlung und automatische Wiederherstellung
Serielle Befehlsschnittstelle

Hardware:
Mikrocontroller: Raspberry Pi Pico (RP2040)
Motortreiber: TMC2209 mit UART-Schnittstelle
Motor: Schrittmotor
Endschalter: 2x (Min und Max Position)

Anschlüsse:

Komponente	Pin
Step	2
Direction	3
Enable	4
Min Switch	5
Max Switch	6
UART TX	4 (GPIO4)
UART RX	5 (GPIO5)

Installation:

Klonen Sie das Repository:

git clone https://github.com/yourusername/luefterklappe.git

Öffnen Sie das Projekt in PlatformIO.

Kompilieren und hochladen:

pio run -t upload

Verwendung:
Serielle Befehle:

Die Steuerung akzeptiert folgende Befehle über die serielle Schnittstelle (115200 baud):

Befehl	Beschreibung:

GOTO xxx	Bewegt den Motor zur Position xxx (in Schritten)
POS?	Gibt die aktuelle Position zurück
HOME	Startet den Homing-Prozess manuell
RESET	Setzt die Steuerung nach einem Fehler zurück
SOFTMIN xxx	Setzt die minimale Software-Endposition
SOFTMAX xxx	Setzt die maximale Software-Endposition
SOFTENDSTOPS ON	Aktiviert die Software-Endanschläge
SOFTENDSTOPS OFF	Deaktiviert die Software-Endanschläge
SOFTENDSTOPS?	Zeigt den Status der Software-Endanschläge

Betriebsmodi:
Die Steuerung durchläuft verschiedene Zustände:

INIT: Initialisierung und Start des Homing-Prozesses
HOMING_MIN: Suche nach dem minimalen Endschalter
HOMING_MAX: Suche nach dem maximalen Endschalter
READY: Normaler Betriebsmodus, bereit für Befehle
ERROR_DETECTED: Fehler erkannt (Endschalter unerwartet ausgelöst oder Stall erkannt)
WAIT_RESET: Warten auf Reset-Befehl oder automatischen Reset nach Timeout
AUTO_REHOME: Automatisches Neustart des Homing-Prozesses

Konfiguration:
TMC2209 Einstellungen:

Der TMC2209 wird mit folgenden Einstellungen konfiguriert:

StallGuard aktiviert
CoolStep deaktiviert
StallGuard-Schwellwert: 100 (anpassbar in der Konstante SG_THRESHOLD)
Motor-Einstellungen
Maximale Geschwindigkeit: 400 Schritte/Sekunde
Beschleunigung: 1000 Schritte/Sekunde²
Homing-Geschwindigkeit: 200 Schritte/Sekunde
Fehlerbehandlung

Die Steuerung erkennt folgende Fehlerzustände:

Unerwartete Endschalter-Auslösung: Wenn ein Endschalter in die falsche Richtung ausgelöst wird
StallGuard-Erkennung: Wenn der Motor blockiert oder überlastet ist

Bei einem Fehler:

Der Motor wird sofort gestoppt
Der Treiber wird deaktiviert (Enable-Pin HIGH)
Die Steuerung wechselt in den WAIT_RESET-Zustand
Nach 10 Minuten erfolgt ein automatischer Reset, wenn kein manueller Reset erfolgt
Entwicklung
Abhängigkeiten
AccelStepper Bibliothek
Arduino Framework für RP2040
Kompilieren
pio run

Hochladen
pio run -t upload

Serielle Überwachung
pio device monitor -b 115200

Lizenz
MIT

Autor
Mosei1984

Hinweise
Die Steuerung verwendet Hardware UART1 des Pico für die TMC2209-Kommunikation
Stellen Sie sicher, dass die Endschalter korrekt angeschlossen und konfiguriert sind
Nach dem Homing werden die Software-Endanschläge automatisch auf den vollen Bewegungsbereich gesetzt
