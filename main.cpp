/*
Taupunkt-Luefter-Steuerung Version 2.0
2026-03-29 Markus Schnackenberg

In Version 2.0 Ergänzung um Inkremental-Geber (Dreh-Encoder) mit Menu-Steuerung zur Einstellung der Schaltpunkte (Delta-Tauwert zur Einschaltung des Lüfters,
Hysteres zum Ausschaltenm des Lüfters, minimale Innen- und Außen-Temperatur für eine Belüftung). Die Schaltpunkte werden auf initiale-Werte gesetzt, im
internen EEPROM des Mikrocontrollers gespeichert und können dort entsprechend über das Menu verändert und dauerhaft gespeichert werden.
Der Betrieb der Tau-Punkt-Lüftung ist auch ohne den Dreh-Encoder möglich (mit den initialen Werten für die Schaltpunkte).

Sensoren: GY-21 (Temperatur und Luftfeuchte) angebunden über PCA9548A I2C-Multiplexer
          Multiplexer notwendig, da die verwendeten Sensoren GY-21 eine feste I2C-Adresse 0x40
          verwenden und die Adresse nicht 2-fach auf dem I2C-Bus existieren kann
Display:  LCD-DOT-Matrix, HD44780 kompatibel, 4x20 Zeichen mit I2C-Anbindung
*/

#include "functions.h"            // Projekt-Datei - Auslagerung von globalen Variablen, Funktionen und Bilbliothek-Einbindungen

void setup()
{
  wdt_enable(WDTO_8S);                 // Watchdog timer auf 8 Sekunden stellen
  pinMode(RELAISPIN, OUTPUT);          // Relaispin als Output definieren
  digitalWrite(RELAISPIN, RELAIS_AUS); // Relais und damit den Lüfter ausschalten
  pinMode(MAN_AUS, INPUT_PULLUP);      // Pin für Lüfter manuell aus als Input definieren (mit internem PullUp-Widerstand)
  pinMode(MAN_EIN, INPUT_PULLUP);      // Pin für Lüfter manuell ein als Input definieren (mit internem PullUp-Widerstand)
  pinMode(LICHT, INPUT_PULLUP);        // Pin für Display-Beleuchtungs-Taster als Input definieren (mit internem PullUp-Widerstand)
  pinMode(MENUBUTTON, INPUT_PULLUP);   // Pin vom Dreh-Encoder Taster zum Aufruf des Menu & Eingabe-Bestätigungen im Menu (mit internem PullUp-Widerstand)
  pinMode(ENCODER_A, INPUT_PULLUP);    // Pin A vom Dreh-Encoder, entprelltes Signal über Schmitt-Trigger Inverter & Inverter (CD4093BE)
  pinMode(ENCODER_B, INPUT_PULLUP);    // Pin B vom Dreh-Encoder, entprelltes Signal über Schmitt-Trigger Inverter & Inverter (CD4093BE)
  state=INIT;                          // State Machine auf Status INIT setzen
  
  lcd.init();                     // LCD-Display initialisieren
  lcd.backlight();                // LCD-Display Belechtung einschalten

  I2C_multiplexer_channel(0);     // I2C-Multiplexer Kanal 0 anwählen
  GY21_sensor_init(0x40);         // Initialisieren von Sensor 1 (Innen)
  I2C_multiplexer_channel(1);     // I2C-Multiplexer Kanal 1 anwählen
  GY21_sensor_init(0x40);         // Initialisieren von Sensor 2 (Aussen)

  attachInterrupt(digitalPinToInterrupt(ENCODER_B), encoder_interrupt, CHANGE);

  
  byte check_eeprom = EEPROM[0];  // Speichervariable für EEPROM ist leer Überprüfung
  if (check_eeprom != 0x1A)       // WENN EEPROM leer (Prüfkennzeichnung Speicheradresse 0 ist NICHT 0x1A), DANN
  {                               // >>
    EEPROM.put(0, 0x1A);          // schreibe Prüfkennzeichnung ins EEPROM (Speicheradresse 0, Wert 0x1A)
    EEPROM.put(10, SCHALTmin);    // schreibe Standard-SCHALTmin ins EEPROM (Speicheradresse 10, Wert 5.0)
    EEPROM.put(20, HYSTERESE);    // schreibe Standard-HYSTERESE ins EEPROM (Speicheradresse 20, Wert 1.0)
    EEPROM.put(30, TEMP1_min);    // schreibe Standard-Min-Temperatur innen ins EEPROM (Speicheradresse 30, Wert 3.0)
    EEPROM.put(40, TEMP2_min);    // schreibe Standard-Min-Temperatur aussen ins EEPROM (Speicheradresse 40, Wert -10.0)
  }                               // <<
  else                            // SONST
  {                               // >>
    EEPROM.get(10, SCHALTmin);    // lese EEPROM Speicheradresse 10 und schreibe den Wert in die Variable SCHALTmin
    EEPROM.get(20, HYSTERESE);    // lese EEPROM Speicheradresse 20 und schreibe den Wert in die Variable HYSTERESE
    EEPROM.get(30, TEMP1_min);    // lese EEPROM Speicheradresse 30 und schreibe den Wert in die Variable TEMP1min
    EEPROM.get(40, TEMP2_min);    // lese EEPROM Speicheradresse 30 und schreibe den Wert in die Variable TEMP2min
  }
}

void loop()
{
  char ch1[8];        // Speichervariable für Luftfeuchte Sensor 1 (Innen) - nach Wandlung in Zeichenkette für Display-Ausgabe
  char ct1[8];        // Speichervariable für Temperatur Sensor 1 (Innen) - nach Wandlung in Zeichenkette für Display-Ausgabe
  char ch2[8];        // Speichervariable für Luftfeuchte Sensor 2 (Aussen) - nach Wandlung in Zeichenkette für Display-Ausgabe
  char ct2[8];        // Speichervariable für Temperatur Sensor 2 (Aussen) - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cdp1[8];       // Speichervariable für errechneten Taupunkt Innen - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cdp2[8];       // Speichervariable für errechneten Taupunkt Aussen - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cDeltaTP[8];   // Speichervariable für errechneten Delta-Taupunkt zwischen Innen und Aussen - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cSCHALTmin[8]; // Speichervariable für Delta-Taupunkt Schaltschwelle Lüfter ein - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cHYSTERESE[8]; // Speichervariable für Hysterese Schaltschwelle Lüfter aus - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cTEMP1min[8];  // Speichervariable für minimale Innen-Temperatur Belüftung - nach Wandlung in Zeichenkette für Display-Ausgabe
  char cTEMP2min[8];  // Speichervariable für minimale Aussen-Temperatur Belüftung - nach Wandlung in Zeichenkette für Display-Ausgabe

  display_light(LICHT);                                     // Display-Hintergrundbeleuchtung steuern über Funktionsbaustein
  write_to_display(display0, display1, display2, display3); // Display-Ausgabe schreiben über Funktionsbaustein (Übergabe der Inhalte der 4 Zeilen)

  if ((digitalRead(MENUBUTTON) == LOW) && (state != MENU))  // WENN Encoder-Button gedrückt UND State Machine nicht im Status "MENU", DANN
  {                                                         // >>
    Encoder_Position = 0;                                   // setze Encoder_Positions-Variable auf Wert 0
    state=MENU;                                             // Setze State Machine Status "MENU"
    delay(200);                                             // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" ins Menu)
  }                                                         // <<

  if (state == SENSOR_FEHLER)                       // WENN State Machine im Status "Sensor Fehler", DANN
  {                                                 // >>
    state=DATEN;                                    // Setze State Machine Status "DATEN"
  }                                                 // <<

  I2C_multiplexer_channel(0);                       // I2C-Multiplexer Kanal 0 anwählen (Sensor Innen)
  double h1 = GY21_read_humidity(0x40);             // Luftfeuchte Sensor 1 (Innen) auslesen und in "h1" speichern 
  double t1 = GY21_read_temperature(0x40);          // Temperatur Sensor 1 (Innen) auslesen und in "t1" speichern
  float dp1 = calculate_dewpoint(t1, h1);           // Taupunkt-Temperatur Innen errechnen und in "dp1" speichern
  
  if (h1 < 0)                                       // WENN Luftfeuchte Sensor 1 (Innen) in "h1" kleiner als 0, DANN
  {                                                 // >>
    state = SENSOR_FEHLER;                          // Setze State Machine Status "SENSOR_FEHLER"
    strcpy( display2, "  SENSOR 1 innen    " );     // Setze Display-Zeile 3 auf "SENSOR 1 innen"
  }                                                 // <<
  else if (state != INIT)                           // WENN SONST State Machine nicht im Status "INIT"
  {                                                 // >>
    strcpy( display2, "                    " );     // Setze Display-Zeile 3 auf leer
  }                                                 // <<

  I2C_multiplexer_channel(1);                       // I2C-Multiplexer Kanal 1 anwählen (Sensor Aussen)
  double h2 = GY21_read_humidity(0x40);             // Luftfeuchte Sensor 2 (Aussen) auslesen und in "h2" speichern
  double t2 = GY21_read_temperature(0x40);          // Temperatur Sensor 2 (Aussen) auslesen und in "t2" speichern
  float dp2 = calculate_dewpoint(t2, h2);           // Taupunkt-Temperatur Aussen errechnen und in "dp2" speichern

  if (h2 < 0)                                       // WENN Luftfeuchte Sensor 2 (Aussen) in "h2" kleiner als 0, DANN
  {                                                 // >>
    state = SENSOR_FEHLER;                          // Setze State Machine Status "SENSOR_FEHLER"
    strcpy( display3, "  SENSOR 2 aussen   " );     // Setze Inhalt von Display-Variable Zeile 4 auf "SENSOR 2 aussen"
  }                                                 // <<
  else if (state != INIT)                           // WENN SONST State Machine nicht im Status "INIT"
  {                                                 // >>
    strcpy( display3, "                    " );     // Setze Inhalt von Display-Variable Zeile 4 auf leer
  }                                                 // <<

  float DeltaTP = dp1 - dp2;                        // Delta-Taupunkt von Innen zu Aussen berechnen und in "DeltaTP" speichern

  static unsigned long lastexec = 0;                // Einmalige Initialisierung der Variable "lastexec" für Zeitstempel letzte Ausführung
  unsigned long timepassed = millis() - lastexec;   // Vergangene Zeit seit letzer Ausführung errechnen und in "timepassed" speichern

  switch (state)                                    /* --> State Machine - Fall Auswertung <-- */
  {
    case INIT:                                      // >>>> State Machine IST im Status "INIT"
      digitalWrite(RELAISPIN, RELAIS_EIN);          // Lüfter über Relais einschalten (Lüfter-Test)
      if (timepassed > 1000)                        // WENN State Machine länger als 7 Sekunden im Status "INIT"
      {                                             // >>
        dtostrf(SCHALTmin, 5, 1, cSCHALTmin);       // Wandle SCHALTmin-Wert in Zeichenkette und speichere in Variable "cSCHALTmin"
        dtostrf(HYSTERESE, 5, 1, cHYSTERESE);       // Wandle HYSTERESE-Wert in Zeichenkette und speichere in Variable "cHYSTERES"
        dtostrf(TEMP1_min, 5, 1, cTEMP1min);        // Wandle TEMP1min-Wert (minimnale Innen-Temperatur) in Zeichenkette und speichere in Variable "cTEMP1min"
        dtostrf(TEMP2_min, 5, 1, cTEMP2min);        // Wandle TEMP2min-Wert (minimnale Aussen-Temperatur) in Zeichenkette und speichere in Variable "cTEMP2min"
        strcpy( display0, "TauSchalt : " );         // Setze Inhalt von Display-Variable Zeile 1
        strcat( display0, cSCHALTmin );             // Hänge Zeichenkette für minimale Delta-Taupunkt-Schalt-Temperatur an Display-Variable 1
        strcat( display0, "C  " );                  // Hänge "C  " an Display-Variable 1 an
        strcpy( display1, "Hysterese : " );         // Setze Inhalt von Display-Variable Zeile 2
        strcat( display1, cHYSTERESE );             // Hänge Zeichenkette für Hysterese an Display-Variable 2
        strcat( display1, "C  " );                  // Hänge "C  " an Display-Variable 2 an
        strcpy( display2, "TempInMin : " );         // Setze Inhalt von Display-Variable Zeile 3
        strcat( display2, cTEMP1min );              // Hänge Zeichenkette für minimale Temperatur innen an Display-Variable 3
        strcat( display2, "C  " );                  // Hänge "C  " an Display-Variable 3 an
        strcpy( display3, "TempOutMin: " );         // Setze Inhalt von Display-Variable Zeile 4
        strcat( display3, cTEMP2min );              // Hänge Zeichenkette für minimale Temperatur aussen an Display-Variable 4
        strcat( display3, "C  " );                  // Hänge "C  " an Display-Variable 4 an
        digitalWrite(RELAISPIN, RELAIS_AUS);        // Lüfter über Relais ausschalten (Ende Lüfter-Test)
      }                                             // <<
      if (timepassed > 5000)                        // WENN State Machine länger als 14 Sekunden im Status "INIT", DANN
      {                                             // >>
        lastexec = millis();                        // Setze letzte Ausführung über Variable "lastexec" auf aktuelle Laufzeit
        state=DATEN;                                // State Machine auf Status "DATEN"
      }                                             // <<
      wdt_reset();                                  // Watchdog zurücksetzen
      break;                                        // <<<< Switch case INIT Ende

    case EIN:                                       // >>>> State Maschine IST im Status "EIN"
      digitalWrite(RELAISPIN, RELAIS_EIN);          // Schalte Lüfter über Relais ein
      strcpy( display0, "Luefter manuell  EIN" );   // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, "Temperatur- und Tau-" );   // Setze Inhalt von Display-Variable Zeile 2
      strcpy( display2, "punkt-Steuerung sind" );   // Setze Inhalt von Display-Variable Zeile 3
      strcpy( display3, "deaktiviert!        " );   // Setze Inhalt von Display-Variable Zeile 4
      if (timepassed > 7000)                        // WENN State Machine länger als 7 Sekunden im Status "EIN"
      {                                             // >>
        lastexec = millis();                        // Setze letzte Ausführung über Variable "lastexec" auf aktuelle Laufzeit
        state=DATEN;                                // State Machine auf Status "DATEN" setzen
      }                                             // <<
      wdt_reset();                                  // Watchdog zurücksetzen
      break;                                        // <<<< Switch case EIN Ende

    case AUTO:                                                                                  // >>>> State Maschine IST im Status "AUTO"
      dtostrf(DeltaTP, 5, 1, cDeltaTP);                                                         // DeltaTaupunkt-Wert in Zeichenkette umwandeln und in "cDeltaTP" speichern
      if ((DeltaTP > (SCHALTmin)) && (t1 > (TEMP1_min)) && (t2 > (TEMP2_min)))                  // WENN DeltaTaupunkt größer der Schaltschwelle UND Temperaturen größer den Minimal-Werten
      {                                                                                         // >>
        digitalWrite(RELAISPIN, RELAIS_EIN);                                                    // Schalte Lüfter über Relais ein
        strcpy( display1, "  Lueftung  laeuft  " );                                             // Setze Inhalt von Display-Variable Zeile 2
        strcpy( display2, "  Temperaturen  ok  " );                                             // Setze Inhalt von Display-Variable Zeile 3
      }                                                                                         // <<
      else if ((DeltaTP < (SCHALTmin - HYSTERESE)) || (t1 < (TEMP1_min)) || (t2 < (TEMP2_min))) // WENN SONST DeltaTaupunkt kleiner der Schaltschwelle minus Hysteres ODER Temperaturen kleiner den Minimal-Werten
      {                                                                                         // >>
        digitalWrite(RELAISPIN, RELAIS_AUS);                                                    // Schalte Lüfter über Relais aus
        strcpy( display1, "    Lueftung aus    " );                                             // Setze Inhalt von Display-Variable Zeile 2
        if ((t1 < (TEMP1_min)) || (t2 < (TEMP2_min)))                                           // WENN Temperaturen kleiner den Minimal-Werten
        {                                                                                       // >>>
          strcpy( display2, "Temperaturen niedrig" );                                           // Setze Inhalt von Display-Variable Zeile 3
        }                                                                                       // <<<
        else                                                                                    // SONST
        {                                                                                       // >>>
          strcpy( display2, "  Temperaturen  ok  " );                                           // Setze Inhalt von Display-Variable Zeile 3
        }                                                                                       // <<<
      }                                                                                         // <<
      strcpy( display0, "  Automatik  Modus  " );                                               // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display3, "DeltaTaupunkt " );                                                     // Setze Inhalt von Display-Variable Zeile 4
      strcat( display3, cDeltaTP );                                                             // Hänge Zeichenkette für DeltaTaupunkt-Wert an Display-Variable 4
      strcat( display3, "%" );                                                                  // Hänge "%" an Display-Variable 4 an
      if (timepassed > 7000)                                                                    // WENN State Machine länger als 7 Sekunden im Status "AUTO"
      {                                                                                         // >>
        lastexec = millis();                                                                    // Setze letzte Ausführung über Variable "lastexec" auf aktuelle Laufzeit
        state=DATEN;                                                                            // State Machine auf Status "DATEN" setzen
      }                                                                                         // <<
      wdt_reset();                                                                              // Watchdog zurücksetzen
      break;                                                                                    // <<<< Switch case AUTO Ende

    case AUS:                                       // >>>> State Maschine IST im Status "AUS"
      digitalWrite(RELAISPIN, RELAIS_AUS);          // Schalte Lüfter über Relais aus
      strcpy( display0, "Luefter manuell  AUS" );   // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, "Temperatur- und Tau-" );   // Setze Inhalt von Display-Variable Zeile 2
      strcpy( display2, "punkt-Steuerung sind" );   // Setze Inhalt von Display-Variable Zeile 3
      strcpy( display3, "deaktiviert!        " );   // Setze Inhalt von Display-Variable Zeile 4
      if (timepassed > 7000)                        // WENN State Machine länger als 7 Sekunden im Status "AUS"
      {                                             // >>
        lastexec = millis();                        // Setze letzte Ausführung über Variable "lastexec" auf aktuelle Laufzeit
        state=DATEN;                                // State Machine auf Status "DATEN" setzen
      }                                             // <<
      wdt_reset();                                  // Watchdog zurücksetzen
      break;                                        // <<<< Switch case AUS Ende

    case SENSOR_FEHLER:                                                     // >>>> State Maschine IST im Status "SENSOR_FEHLER"
      if((digitalRead(MAN_EIN) == HIGH) && (digitalRead(MAN_AUS) == HIGH))  // WENN Modus-Wahlschalter im Modus "Automatik" (Mittel-Stellung des Schalters)
      {                                                                     // >>
        digitalWrite(RELAISPIN, RELAIS_AUS);                                // Schalte Lüfter über Relais aus
      }                                                                     // <<
      strcpy( display0, "   SENSOR FEHLER    " );                           // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, "                    " );                           // Setze Inhalt von Display-Variable Zeile 2
      wdt_reset();                                                          // Watchdog zurücksetzen
      break;                                                                // <<<< Switch case SENSOR_FEHLER Ende

    case DATEN:                               // >>>> State Maschine IST im Status "DATEN"
      dtostrf(h1, 5, 1, ch1);                 // Wandle Luftfeuchte-Wert Sensor 1 (Innen) in Zeichenkette und speichere in Variable "ch1"
      dtostrf(h2, 5, 1, ch2);                 // Wandle Luftfeuchte-Wert Sensor 2 (Aussen) in Zeichenkette und speichere in Variable "ch2"
      dtostrf(t1, 5, 1, ct1);                 // Wandle Temperatur-Wert Sensor 1 (Innen) in Zeichenkette und speichere in Variable "ct1"
      dtostrf(t2, 5, 1, ct2);                 // Wandle Temperatur-Wert Sensor 2 (Aussen) in Zeichenkette und speichere in Variable "ct2"
      dtostrf(dp1, 4, 1, cdp1);               // Wandle Taupunkt-Wert Innen in Zeichenkette und speichere in Variable "cdp1"
      dtostrf(dp2, 4, 1, cdp2);               // Wandle Taupunkt-Wert Aussen in Zeichenkette und speichere in Variable "cdp2"
      strcpy( display0, "Innen: " );          // Setze Inhalt von Display-Variable Zeile 1
      strcat( display0, ch1 );                // Hänge Luftfeuchte-Wert Innen an Zeichenkette in Display-Variable Zeile 1 an
      strcat( display0, "% " );               // Hänge "% " an Zeichenkette in Display-Variable Zeile 1 an
      strcat( display0, ct1 );                // Hänge Temperatur-Wert Innen an Zeichenkette in Display-Variable Zeile 1 an
      strcat( display0, "C" );                // Hänge "C" an Zeichenkette in Display-Variable Zeile 1 an
      strcpy( display1, "Taupunkt  " );       // Setze Inhalt von Display-Variable Zeile 2
      strcat( display1, cdp1 );               // Hänge Taupunkt-Wert Innen an Zeichenkette in Display-Variable Zeile 2 an
      strcat( display1, "C     " );           // Hänge "C     " an Zeichenkette in Display-Variable Zeile 2 an
      strcpy( display2, "Aussen:" );          // Setze Inhalt von Display-Variable Zeile 3
      strcat( display2, ch2 );                // Hänge Luftfeuchte-Wert Aussen an Zeichenkette in Display-Variable Zeile 3 an
      strcat( display2, "% " );               // Hänge "% " an Zeichenkette in Display-Variable Zeile 3 an
      strcat( display2, ct2 );                // Hänge Temperatur-Wert Aussen an Zeichenkette in Display-Variable Zeile 3 an
      strcat( display2, "C" );                // Hänge "C" an Zeichenkette in Display-Variable Zeile 3 an
      strcpy( display3, "Taupunkt  " );       // Setze Inhalt von Display-Variable Zeile 4
      strcat( display3, cdp2 );               // Hänge Taupunkt-Wert Aussen an Zeichenkette in Display-Variable Zeile 4 an
      strcat( display3, "C     " );           // Hänge "C     " an Zeichenkette in Display-Variable Zeile 4 an
      if (timepassed > 7000)                  // WENN State Machine länger als 7 Sekunden im Status "DATEN"
      {                                       // >>
        if (digitalRead(MAN_AUS) == LOW)      // WENN Modus-Schalter auf "manuell Aus"
        {                                     // >>>
          state=AUS;                          // State Machine auf Status "AUS" setzen
        }                                     // <<<
        else if (digitalRead(MAN_EIN) == LOW) // WENN SONST Modus-Schalter auf "manuell Ein"
        {                                     // >>>
          state=EIN;                          // State Machine auf Status "EIN" setzen
        }                                     // <<<
        else                                  // SONST
        {                                     // >>>
          state=AUTO;                         // State Machine auf Status "AUTO" setzen
        }                                     // <<<
        lastexec = millis();                  // Setze letzte Ausführung über Variable "lastexec" auf aktuelle Laufzeit
      }                                       // <<
      wdt_reset();                            // Watchdog zurücksetzen
      break;                                  // <<<< Switch case DATEN Ende
    case MENU:                                                    // >>>> State Maschine IST im Status "MENU"
      EEPROM.get(10, SCHALTmin);                                  // lese EEPROM Speicheradresse 20 und schreibe den Wert in die Variable HYSTERESE
      EEPROM.get(20, HYSTERESE);                                  // lese EEPROM Speicheradresse 20 und schreibe den Wert in die Variable HYSTERESE
      EEPROM.get(30, TEMP1_min);                                  // lese EEPROM Speicheradresse 30 und schreibe den Wert in die Variable TEMP1min
      EEPROM.get(40, TEMP2_min);                                  // lese EEPROM Speicheradresse 30 und schreibe den Wert in die Variable TEMP2min
      if (old_position < Encoder_Position)                        // WENN gespeicherte Encoder-Position kleiner als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        counter = counter+1;                                      // erhöhe counter um 1
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      else if (old_position > Encoder_Position)                   // WENN SONST gespeicherte Encoder-Position größer als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        counter = counter-1;                                      // verringere counter um 1
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      if (counter == 0)                                           // WENN counter gleich 0, DANN
      {                                                           // >>
        strcpy( display0, "  Menu  Position 1  " );               // Setze Inhalt von Display-Variable Zeile 1
        strcpy( display1, "   Schaltschwelle   " );               // Setze Inhalt von Display-Variable Zeile 2
        strcpy( display2, "DeltaTaupunkt setzen" );               // Setze Inhalt von Display-Variable Zeile 3
        strcpy( display3, "                    " );               // Setze Inhalt von Display-Variable Zeile 4
        if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU))  // WENN Encoder-Button gedrückt, DANN
        {                                                         // >>>
          state=MENU1;                                            // Setze State Machine Status "MENU1"
          counter = 0;                                            // setze Menu Zaehler auf den Wert 0
          delay(500);                                             // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
        }                                                         // <<<
      }                                                           // <<
      else if (counter == 1 || counter == -4)                     // WENN SONST counter gleich 1 ODER counter gleich -4, DANN
      {                                                           // >>
        strcpy( display0, "  Menu  Position 2  " );               // Setze Inhalt von Display-Variable Zeile 1
        strcpy( display1, "   Hysterese fuer   " );               // Setze Inhalt von Display-Variable Zeile 2
        strcpy( display3, "DeltaTaupunkt setzen" );               // Setze Inhalt von Display-Variable Zeile 3
        strcpy( display2, "   Schaltschwelle   " );               // Setze Inhalt von Display-Variable Zeile 4
        if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU))  // WENN Encoder-Button gedrückt, DANN
        {                                                         // >>>
          state=MENU2;                                            // Setze State Machine Status "MENU2"
          counter = 0;                                            // setze Menu Zaehler auf den Wert 0
          delay(500);                                             // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
        }                                                         // <<<
      }                                                           // <<
      else if (counter == 2 || counter == -3)                     // WENN SONST counter gleich 2 ODER counter gleich -3, DANN
      {                                                           // >>
        strcpy( display0, "  Menu  Position 3  " );               // Setze Inhalt von Display-Variable Zeile 1
        strcpy( display1, "   Minimale Innen-  " );               // Setze Inhalt von Display-Variable Zeile 2
        strcpy( display2, " Temperatur setzen  " );               // Setze Inhalt von Display-Variable Zeile 3
        strcpy( display3, "                    " );               // Setze Inhalt von Display-Variable Zeile 4
        if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU))  // WENN Encoder-Button gedrückt, DANN
        {                                                         // >>>
          state=MENU3;                                            // Setze State Machine Status "MENU3"
          counter = 0;                                            // setze Menu Zaehler auf den Wert 0
          delay(500);                                             // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
        }                                                         // <<<
      }                                                           // <<
      else if (counter == 3 || counter == -2)                     // WENN SONST counter gleich 3 ODER counter gleich -2, DANN
      {                                                           // >>
        strcpy( display0, "  Menu  Position 4  " );               // Setze Inhalt von Display-Variable Zeile 1
        strcpy( display1, "  Minimale Aussen-  " );               // Setze Inhalt von Display-Variable Zeile 2
        strcpy( display2, " Temperatur setzen  " );               // Setze Inhalt von Display-Variable Zeile 3
        strcpy( display3, "                    " );               // Setze Inhalt von Display-Variable Zeile 4
        if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU))  // WENN Encoder-Button gedrückt, DANN
        {                                                         // >>>
          state=MENU4;                                            // Setze State Machine Status "MENU4"
          counter = 0;                                            // setze Menu Zaehler auf den Wert 0
          delay(500);                                             // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
        }                                                         // <<<
      }                                                           // <<
      else if (counter == 4 || counter == -1)                     // WENN SONST counter gleich 4 ODER counter gleich -1, DANN
      {                                                           // >>
        strcpy( display0, "  Menu  Position 5  " );               // Setze Inhalt von Display-Variable Zeile 1
        strcpy( display1, "                    " );               // Setze Inhalt von Display-Variable Zeile 2
        strcpy( display2, "    Menu beenden    " );               // Setze Inhalt von Display-Variable Zeile 3
        strcpy( display3, "                    " );               // Setze Inhalt von Display-Variable Zeile 4
        if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU))  // WENN Encoder-Button gedrückt, DANN
        {                                                         // >>>
          state=DATEN;                                            // Setze State Machine Status "DATEN"
          counter = 0;                                            // setze Menu Zaehler auf den Wert 0
          delay(500);                                             // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
        }                                                         // <<<
      }                                                           // <<
      else                                                        // SONST
      {                                                           // >>
        counter = 0;                                              // setze Menu Zaehler auf den Wert 0 (Sprung an Menu-Anfang)
      }                                                           // <<
      wdt_reset();                                                // Watchdog zurücksetzen
      break;                                                      // <<<< Switch case MENU Ende
    case MENU1:                                                   // >>>> State Maschine IST im Status "MENU1" (Einstellung minimaler Delta-Taupunkt-Wert für Lüfter ein)
      if (old_position < Encoder_Position)                        // WENN gespeicherte Encoder-Position kleiner als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        if ((SCHALTmin) < 10.0 )                                  // WENN SCHALTmin kleiner 10.0
        {                                                         // >>>
          SCHALTmin = SCHALTmin+0.1;                              // erhöhe SCHALTmin um 0.1
        }                                                         // <<<
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      else if (old_position > Encoder_Position)                   // WENN SONST gespeicherte Encoder-Position größer als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        if ((SCHALTmin-HYSTERESE) > 0.1 )                         // WENN SCHALTmin größer 0.1
        {                                                         // >>>
          SCHALTmin = SCHALTmin-0.1;                              // verringere SCHALTmin um 0.1
        }                                                         // <<<
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      dtostrf(SCHALTmin, 5, 1, cSCHALTmin);                       // Wandle SCHALTmin-Wert in Zeichenkette und speichere in Variable "cSCHALTmin"
      strcpy( display0, "   Schaltschwelle   " );                 // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, "DeltaTaupunkt setzen" );                 // Setze Inhalt von Display-Variable Zeile 2
      strcpy( display2, "                    " );                 // Setze Inhalt von Display-Variable Zeile 3
      strcpy( display3, "      " );                               // Setze Inhalt von Display-Variable Zeile 4
      strcat( display3, cSCHALTmin );                             // Hänge aktuellen SCHALTmin-Wert an Display-Variable Zeile 4 an
      strcat( display3, " C       ");                             // Hänge " C       " an Display-Variable Zeile 4 an
      if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU1))   // WENN Encoder-Button gedrückt, DANN
      {                                                           // >>
        counter = 0;                                              // setze Menu Zaehler auf den Wert 0
        EEPROM.put(10, SCHALTmin);                                // schreibe SCHALTmin ins EEPROM (Speicheradresse 10, minmale Delta-Taupunkt-Temperatur Lüfter ein)
        state=MENU;                                               // Setze State Machine Status "MENU"
        delay(500);                                               // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
      }                                                           // <<
      wdt_reset();                                                // Watchdog zurücksetzen
      break;                                                      // <<<< Switch case MENU1 Ende
    case MENU2:                                                   // >>>> State Maschine IST im Status "MENU2" (Einstellung Hysterese Delta-Taupunkt-Wert für Lüfter aus)
       if (old_position < Encoder_Position)                       // WENN gespeicherte Encoder-Position kleiner als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        if ((HYSTERESE) < 4.9 )                                   // WENN HYSTERESE kleiner 4.9, DANN
        {                                                         // >>>
          HYSTERESE = HYSTERESE+0.1;                              // erhöhe HYSTERESE um 0.1
        }                                                         // <<<
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      else if (old_position > Encoder_Position)                   // WENN SONST gespeicherte Encoder-Position größer als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        if ((HYSTERESE) > 1.0 )                                   // WENN HYSTERESE größer 1.0, DANN
        {                                                         // >>>
          HYSTERESE = HYSTERESE-0.1;                              // verringere HYSTERES um 0.1
        }                                                         // <<<
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      dtostrf(HYSTERESE, 5, 1, cHYSTERESE);                       // Wandle HYSTERESE-Wert in Zeichenkette und speichere in Variable "cHYSTERESE"
      strcpy( display0, "   Hysterese fuer   " );                 // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, "DeltaTaupunkt setzen" );                 // Setze Inhalt von Display-Variable Zeile 2
      strcpy( display2, "                    " );                 // Setze Inhalt von Display-Variable Zeile 3
      strcpy( display3, "      " );                               // Setze Inhalt von Display-Variable Zeile 4
      strcat( display3, cHYSTERESE );                             // Hänge aktuellen HYSTERESE-Wert an Display-Variable Zeile 4 an
      strcat( display3, " C       ");                             // Hänge " C       " an Display-Variable Zeile 4 an
      if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU2))   // WENN Encoder-Button gedrückt, DANN
      {                                                           // >>
        counter = 0;                                              // setze Menu Zaehler auf den Wert 0
        EEPROM.put(20, HYSTERESE);                                // schreibe Standard-SCHALTmin ins EEPROM (Speicheradresse 20)
        state=MENU;                                               // Setze State Machine Status "MENU"
        delay(500);                                               // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
      }                                                           // <<
      wdt_reset();                                                // Watchdog zurücksetzen
      break;                                                      // <<<< Switch case MENU2 Ende
    case MENU3:                                                   // >>>> State Maschine IST im Status "MENU3" (Einstellung minimaler Innen-Temperatur)
      if (old_position < Encoder_Position)                        // WENN gespeicherte Encoder-Position kleiner als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        TEMP1_min = TEMP1_min+1;                                  // erhöhe TEMP1_min um 1
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      else if (old_position > Encoder_Position)                   // WENN SONST gespeicherte Encoder-Position größer als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        TEMP1_min = TEMP1_min-1;                                  // verringere TEMP1_min um 1
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      dtostrf(TEMP1_min, 5, 1, cTEMP1min);                        // Wandle TEMP1_min-Wert (minimale Innen-Temperatur in Zeichenkette und speichere in Variable "cTEMP1min"
      strcpy( display0, "   Minimale Innen-  " );                 // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, " Temperatur setzen  " );                 // Setze Inhalt von Display-Variable Zeile 2
      strcpy( display2, "                    " );                 // Setze Inhalt von Display-Variable Zeile 3
      strcpy( display3, "      " );                               // Setze Inhalt von Display-Variable Zeile 4
      strcat( display3, cTEMP1min );                              // Hänge aktuellen TEMP1_min-Wert an Display-Variable Zeile 4 an
      strcat( display3, " C       ");                             // Hänge " C       " an Display-Variable Zeile 4 an
      if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU3))   // WENN Encoder-Button gedrückt, DANN
      {                                                           // >>
        counter = 0;                                              // setze Menu Zaehler auf den Wert 0
        EEPROM.put(30, TEMP1_min);                                // schreibe Standard-SCHALTmin ins EEPROM (Speicheradresse 30)
        state=MENU;                                               // Setze State Machine Status "MENU"
        delay(500);                                               // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
      }                                                           // <<
      wdt_reset();                                                // Watchdog zurücksetzen
      break;                                                      // <<<< Switch case MENU3 Ende
    case MENU4:                                                   // >>>> State Maschine IST im Status "MENU4" (Einstellung minimaler Aussen-Temperatur)
      if (old_position < Encoder_Position)                        // WENN gespeicherte Encoder-Position kleiner als die aktuelle Encoder-Position, DANN
      {                                                           // >>
        TEMP2_min = TEMP2_min+1;                                  // erhöhe TEMP2_min um 1
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      else if (old_position > Encoder_Position)                   // WENN SONST gespeicherte Encoder-Position größer als die aktuelle Encoder-Position, DANN
      {                                                           // <<
        TEMP2_min = TEMP2_min-1;                                  // verringere TEMP2_min um 1
        old_position = Encoder_Position;                          // setze gespeicherte Encoder-Position auf die aktuelle Encoder-Position
      }                                                           // <<
      dtostrf(TEMP2_min, 5, 1, cTEMP2min);                        // Wandle TEMP2_min-Wert (minimale Aussen-Temperatur in Zeichenkette und speichere in Variable "cTEMP2min"
      strcpy( display0, "  Minimale Aussen-  " );                 // Setze Inhalt von Display-Variable Zeile 1
      strcpy( display1, " Temperatur setzen  " );                 // Setze Inhalt von Display-Variable Zeile 2
      strcpy( display2, "                    " );                 // Setze Inhalt von Display-Variable Zeile 3
      strcpy( display3, "      " );                               // Setze Inhalt von Display-Variable Zeile 4
      strcat( display3, cTEMP2min );                              // Hänge aktuellen TEMP2_min-Wert an Display-Variable Zeile 4 an
      strcat( display3, " C       ");                             // Hänge " C       " an Display-Variable Zeile 4 an
      if ((digitalRead(MENUBUTTON) == LOW) && (state == MENU4))   // WENN Encoder-Button gedrückt, DANN
      {                                                           // >>
        counter = 0;                                              // setze Menu Zaehler auf den Wert 0
        EEPROM.put(40, TEMP2_min);                                // schreibe TEMP2_min ins EEPROM (Speicheradresse 40, minimale Aussen-Temperatur)
        state=MENU;                                               // setze State Machine Status "MENU"
        delay(500);                                               // Verzögerungszeit für Tasten-Druck Encoder-Taste (kein versehentliches "Springen" im Menu)
      }                                                           // <<
      wdt_reset();                                                // Watchdog zurücksetzen
      break;                                                      // <<<< Switch case MENU4 Ende
  }
}