# Taupunktlueftung

## Überblick
Realisierung einer Taupunkt gesteuerten Lüftung, die die Lüftung nur dann einschaltet, wenn die Taupunkt-Temperatur der Umgebungsluft (Abluftraum - bspw. außerhalb des Gebäudes) deutlich niedriger ist, als die Taupunkt-Temperatur des Raumes, der be- bzw. entlüftet werden soll.

## Elektrotechnischer Hinweis
Ich mache an dieser Stelle darauf aufmerksam, dass diese Steuerung an einer 230V/50Hz Netzspannung betrieben wird. Es sind die einschlägigen Vorschriften für das Arbeiten, Anschließen und Betreiben einer elektrischen Anlage an 230V/50Hz zu beachten! Personen, die weder Erfahrung noch die nötige Ausbildung in der Elektrotechnik besitzen rate ich daher ausdrücklich vom Nachbau ab.
Dies ist ein Hobby-Projekt mit keinerlei Zualssung für den regelmäßigen Betrieb / Anschluss am 230V/50Hz Netz.

## Umsetzung
### Elektronik & Hardware
Für die Steuerung kommt ein Microcontroller vom Typ Atmel 328P zum Einsatz. Der angesteuerte Lüfter wird über ein Koppel-Relais vom Typ Finder 40.51 (1x UM) angesteuert.
Die Messung der Temperaturen und relativen Luftfeuchten des Abluftraumes / zu belüftenden Raumes wird über Sensoren vom Typ GY-21 HTU21 realisiert, die über einen I2C-Multiplexer an den Microcontroller angebunden werden.
Eine Daten-Ausgabe findet auf einem LCD-Display (DOT-Matrix) mit 4 Zeilen zu je 20 Zeichen (4x20) statt, welches über I2C an den Microcontroller angebunden wird. Die Hintergrundbelechtung des Displays kann über einen Taster aktiviert werden.
Über einen Kipp-Schalter kann die Steuerung in den
1. dauerhaft ein Modus (Lüfter läuft ständig)
2. deaktivierten Modus (Lüfter ist dauerhaft deaktiviert)
3. in den Automatik Modus (Taupunkt gesteuerte Be- bzw. Entlüftung)
versetzt werden.

## Verwendete Software
|Programm   |Verwendung                               |Dateiformat |
|-----------|-----------------------------------------|------------|
| [FreeCAD](https://www.freecad.org)    | CAD Konstruktionen | .FCStd |
| [KiCAD] (https://www.kicad.org/)| Schaltpläne | .kicad_sch |
| [Lochmaster 4](https://www.electronic-software-shop.com/support/kostenlose-datei-viewer/) | Platinen-Layout für Lochraster-Platinen |.lm4 |

