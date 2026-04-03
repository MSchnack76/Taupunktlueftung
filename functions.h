#include <Arduino.h>              // Arduino Standard Bibliothek
#include <LiquidCrystal_I2C.h>    // LCD Display mit I2C Anbindung Bibliothek
#include <Wire.h>                 // Arduino I2C Bibliothek
#include <EEPROM.h>               // Arduino EEPROM Bibliothek
#include <avr/wdt.h>              // AVR Bibliothek für Watchdog

/*** Globale Variablen setzen und einmalig in main.cpp übernehmen ***/
#ifndef GLOBALS_
#define GLOBALS_

/* Definition der verwendeten Microcontroller Pins */
#define RELAISPIN 8                         // PB0, 328P Pin 14 - Luefter schalten
#define MAN_EIN 15                          // PC1, 328P Pin 24 - Schalter Ein - Auto - Aus
#define MAN_AUS 16                          // PC2, 328P Pin 25 - Schalter Ein - Auto - Aus
#define LICHT 17                            // PC3, 328P Pin 26 - Taster für LCD-Display Belechtung
#define ENCODER_A 2                         // PD2, 328P Pin 04 - Dreh-Encoder Pin A
#define ENCODER_B 3                         // PD3, 328P Pin 05 - Dreh-Encoder Pin B
#define MENUBUTTON 4                        // PD4, 328P Pin 06 - Dreh-Encoder Taster

/* Schalten des Relais über N-Feld-Effekt-Transistor, Vorwiderstand und die Masse | VCC liegt fix an der Relais-Spule an */
/* Definition der Relais-Zustände für Programmierung                                                                     */
#define RELAIS_EIN HIGH                     // Relais ein
#define RELAIS_AUS LOW                      // Relais aus

/* Definition LCD-Display */
LiquidCrystal_I2C lcd(0x27,20,4);           // LCD: I2C-Addresse und Displaygroesse setzen
char display0[21] = " Taupunkt--Lueftung "; // Speichervariable für Display Zeile 1 und Initial-Wert
char display1[21] = "    Version  2.0    "; // Speichervariable für Display Zeile 2 und Initial-Wert
char display2[21] = "    Luefter Test    "; // Speichervariable für Display Zeile 3 und Initial-Wert
char display3[21] = "Einstellungen folgen"; // Speichervariable für Display Zeile 4 und Initial-Wert

/* Definition der Konstanten für Programm-Ablauf */
double SCHALTmin = 5.0;   // minimaler Taupunktunterschied, bei dem das Relais schaltet
double HYSTERESE = 1.0;   // Abstand von Ein- und Ausschaltpunkt (Hysterese)
double TEMP1_min = 3.0;   // Minimale Innentemperatur, bei der die Lueftung aktiviert wird
double TEMP2_min = -10.0; // Minimale Außentemperatur, bei der die Lueftung aktiviert wird

/* Definition der State Machine Status */
enum { MENU, MENU1, MENU2, MENU3, MENU4, EIN, AUTO, AUS, SENSOR_FEHLER, DATEN, INIT } state;

/* Definitionen für Dreh-Encoder */
long Encoder_Position = 0;           // Variable für aktuelle Dreh-Encoder Position
long old_position = 0;               // alte Position Encoder für Menu auf/ab
int counter = 0;                     // Menu-Punkt Identifizierung

#define MULTIPLEXER_ADDR 0x70        // I2C-Adresse für PCA9548A I2C Multiplexer setzen (A0 & A1 & A2 auf Masse --> 0x70)
#define DISP_LIGHT_TIME 60000        // Dauer der Display-Hintergrund-Beleuchtung in ms

#endif


/*** FUNKTIONS-DEFINITIONEN ***/

// *** Interrupt-Routine für Dreh-Encoder ***
void encoder_interrupt()
{
  if (digitalRead (ENCODER_A) != digitalRead (ENCODER_B))
  {
    Encoder_Position++;
  }
  else
  {
    Encoder_Position--;
  }
}

// Startet das Programm neu, nicht aber die Sensoren oder das LCD
void software_Reset() 
{
  asm volatile ("  jmp 0");  
}

// *** Funktions-Definition zur Berechnung einer Taupunkt-Temperatur ***
float calculate_dewpoint(double temperature, double humidity)
{
  // Parameter für Magnus-Formel ueber/gleich und unter 0°C definieren
  float a, b;
  if (temperature >= 0)
  {
    a = 7.5;
    b = 237.3;
  }
  else if (temperature < 0)
  {
    a = 7.6;
    b = 240.7;
  }
  
  // Berechnung des Taupunktes an Hand Teamperatur und relativer Luftfeuchte
  // Sättigungsdampfdruck in hPa (Magnus-Formel)
  float sdd = 6.1078 * pow(10, (a*temperature)/(b+temperature));
  
  // Dampfdruck in hPa
  float dd = sdd * (humidity/100);
  
  // v-Parameter
  float v = log10(dd/6.1078);
  
  // Taupunkttemperatur (°C)
  float dewpoint_temperature = (b*v) / (a-v);
  return { dewpoint_temperature };  
}

// *** Definierte Zeilen auf Display ausgeben ***
void write_to_display(char *line0, char *line1, char *line2, char *line3)
{
  static unsigned long last_executed = 0;               // Einmalige Initialisierung der Variable "last_executed" für Zeitstempel letzte Ausführung
  unsigned long time_passed = millis() - last_executed; // Vergangene Zeit seit letzer Ausführung errechnen und in "time_passed" speichern

  if (time_passed > 500)          // WENN vergangene Zeit seit letzter Ausführung größer 0,5 Sekunden ist
  {                               // >>
    lcd.setCursor(0,0);           // Ausgabe-Cursor des LCD auf Position 1, Zeile 1 setzen
    for (int i = 0; i < 20; i++)  // Cursor Positionen von Anfang bis Ende durchlaufen
    {                             // >>>
      lcd.write(line0[i]);        // Zeichen je angwählte Position Zeile 1 auf LCD ausgeben
    }                             // <<<
    lcd.setCursor(0,1);           // Ausgabe-Cursor des LCD auf Position 1, Zeile 2 setzen
    for (int i = 0; i < 20; i++)  // Cursor Positionen von Anfang bis Ende durchlaufen
    {                             // >>>
      lcd.write(line1[i]);        // Zeichen je angwählte Position Zeile 2 auf LCD ausgeben
    }                             // <<<
    lcd.setCursor(0,2);           // Ausgabe-Cursor des LCD auf Position 1, Zeile 3 setzen
    for (int i = 0; i < 20; i++)  // Cursor Positionen von Anfang bis Ende durchlaufen
    {                             // >>Y
      lcd.write(line2[i]);        // Zeichen je angwählte Position Zeile 3 auf LCD ausgeben
    }                             // <<<
    lcd.setCursor(0,3);           // Ausgabe-Cursor des LCD auf Position 1, Zeile 4 setzen
    for (int i = 0; i < 20; i++)  // Cursor Positionen von Anfang bis Ende durchlaufen
    {                             // >>>
      lcd.write(line3[i]);        // Zeichen je angwählte Position Zeile 4 auf LCD ausgeben
    }                             // <<<
    last_executed = millis();     // Setze letzte Ausführung über Variable "last_executed" auf aktuelle Laufzeit
  }                               // <<
}

// *** Display Beleuchtungs-Routine ***
void display_light(int light_button)
{
  static unsigned long last_exec = 0;             // Einmalige Initialisierung der Variable "last_exec" für Zeitstempel letzte Ausführung
  unsigned long time_pass = millis() - last_exec; // Vergangene Zeit seit letzer Ausführung errechnen und in "time_pass" speichern

  if (digitalRead(light_button) == LOW) // WENN Taster für Display-Beleuchtung gedrückt
  {                                     // >>
    last_exec = millis();               // Setze letzte Ausführung über Variable "last_exec" auf aktuelle Laufzeit
    lcd.backlight();                    // Schalte Display-Beleuchtung ein
  }                                     // <<
  if (time_pass > DISP_LIGHT_TIME)      // WENN vergangene Zeit seit letzter Ausführung größer als definierte Display-Beleuchtungs-Dauer
  {                                     // >>
    last_exec = millis();               // Setze letzte Ausführung über Variable "last_exec" auf aktuelle Laufzeit         
    lcd.noBacklight();                  // Schalte Display-Beleuchtung aus
  }                                     // <<
  if (state == MENU)                    // WENN state machine im Status MENU
  {                                     // >>
    lcd.backlight();                    // Schalte Display-Beleuchtung ein
  }                                     // <<
  
}


/* I2C-Sensoren Funktionen für Sensoren Typ GY-21 */

// *** Initialisierungs-Funktion für Sensor mit Adresse 0x40 ***
void GY21_sensor_init(const int addr)
{
    Wire.beginTransmission(addr);
    Wire.endTransmission();
}

// *** Temperatur aus Sensor mit spezifizierter I2C-Adresse auslesen ***
double GY21_read_temperature(const int addr)
{
    double temperature;
    int low_byte, high_byte, raw_data;
    
    /**Send command of initiating temperature measurement**/
    Wire.beginTransmission(addr);
    Wire.write(0xE3);
    Wire.endTransmission();

    /**Read data of temperature**/
    Wire.requestFrom(addr, 2);
    if (Wire.available() <= 2)
    {
        high_byte = Wire.read();
        low_byte = Wire.read();
        high_byte = high_byte << 8;
        raw_data = high_byte + low_byte;
    }

    temperature = (175.72 * raw_data) / 65536;
    temperature = temperature - 46.85;
    return temperature;
}

// *** Luftfeuchtigkeit aus Sensor mit spezifizierter I2C-Adresse auslesen ***
double GY21_read_humidity(const int addr)
{
    double humidity, raw_data_1, raw_data_2;
    int low_byte, high_byte, container;

    /**Send command of initiating relative humidity measurement**/
    Wire.beginTransmission(addr);
    Wire.write(0xE5);
    Wire.endTransmission();

    /**Read data of relative humidity**/
    Wire.requestFrom(addr, 2);
    if(Wire .available() <= 2)
    {
        high_byte = Wire.read();
        container = high_byte / 100;
        high_byte = high_byte % 100;
        low_byte = Wire.read();
        raw_data_1 = container * 25600;
        raw_data_2 = high_byte * 256 + low_byte;
    }

    raw_data_1 = (125 * raw_data_1) / 65536;
    raw_data_2 = (125 * raw_data_2) / 65536;
    humidity = raw_data_1 + raw_data_2;
    humidity = humidity - 6;
    return humidity;
}

/* I2C-Multiplexer Funktionen TCA9548A oder PCA9548A */

// *** Auswahlfunktion für Multiplexer-Kanal ***
void I2C_multiplexer_channel(uint8_t bus)
{
    Wire.beginTransmission(MULTIPLEXER_ADDR); // Übertragung beginnen
    Wire.write(1 << bus);                     // Daten übertragen
    Wire.endTransmission();                   // Übertragung beenden
}
