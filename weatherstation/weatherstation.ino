/*
 * Copyright (c) 2013-2014 Max Oberberger (max@oberbergers.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Get pressure and temperature from the BMP085 and calculate altitude.
 * Serial.print it out at 9600 baud to serial monitor.
 *
 * hih4030 calculation information:
 * http://wiki.bildr.org/index.php/Humidity_Sensor_-_HIH-4030_Breakout
 */

#include <Wire.h>
#include <Arduino.h>
#include <Ethernet.h>
#include <math.h>
#include <SPI.h>
#include <bmp085.h>
#include <hih4030.h>

#define uint8_t byte

#define TIMER_INTERVAL 54000 	  // 54000 * 5 = 270 000 ms = 4 min 30 sec

bmp085 bmp;
hih4030 humidSensor;

float temperature;
long timer = 599888;

// Internet values
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // Arduino Mac-Adress
byte ip[] = { 192, 168, 3, 130}; // Arduino IP-Adress
byte server[]={0, 0, 0, 0}; // IP-Adress of server TODO: replace 0 with IP
byte gateway[]={192, 168, 3, 1}; // Network-Gateway in my local network (Router)
EthernetClient client; // the arduino is a client, which posts some data to a webserver
char *postRequestPressure="POST /cgi-bin/arduino.pl?P=";
char *postRequestTemp="&T=";
char *postRequestHumidity="&H=";

/*********** Windspeed *******************/
char *postRequestWindSpeed="&W=";
char *postRequestWindDirection="&D=";
char *postRequestRain="&R=";
const int  windGeschwPin = 6; // contact on pin 6 digital
int windGeschwPinZaehler = 0; // impuls counter
int windGeschwPinStatus = 0; // actual impuls
int windGeschwPinStatusAlt = 0; // alter Impuls-Status
unsigned long windGeschwMessStart;
unsigned long windGeschwMessStartAlt = 0;
int windGeschw; // Variable fuer Windgeschwindigkeit
int beaufort = 0; // Variable Windstaerke in Beaufort
float knoten = 0.0;
float wind = 0.0;

/*********** Windrichtung *******************/
int windrichtungsPin = A2; // Windrichtung an Pin 3 analog
int sensorWertNeu = 0; // speichert aktuellen Wert vom Windrichtungs-Sensor
int sensorVolt1 = 0; // speichert berechneten Wert vom Windrichtungs-Sensor in Volt

// Werte laut Datenblatt, aufsteigend zur Suche geordnet
int windrichtungVolt[] = {320, 410, 450, 620, 900, 1190, 1400, 1980, 2250, 2930, 3080, 3430, 3840, 4040, 4620, 4780};

// Werte in Grad laut Datenblatt
//int windRichtungGrad[] = {113,  68,  90, 158, 135, 203, 180,  23,  45, 248, 225, 338,   0, 292, 270, 315};

// Werte in Grad passend zur Ausrichtung der Sensoren
int windRichtungGrad[] = {203, 158, 180, 248, 225, 292, 270, 113, 135, 338, 315, 68, 90, 23, 0, 45};

unsigned int windrichtungVoltSumme = 0;
// start with SE
int winddirection = 8;
long zeit1Neu;
long zeit1Alt;
int zaehler1 = 0;
long intervall = 60000; // eine Messung pro Minute (60.000 ms)

/*********** Niederschlag *******************/
int zaehlerRegen = 0;
int mengeRegen = 0;

void zaehlerRegenMess()
{
    zaehlerRegen = zaehlerRegen++; // Impuls erkannt, Zaehler erhoehen
    {;} // verlassen Unterroutine
}

void setup()
{
	Serial.begin(9600); // set baudrate for Serial Port to 9600
	Ethernet.begin(mac, ip, gateway); // "start" Ethernet-connection
        
    humidSensor.begin();
	bmp.begin();

    attachInterrupt(0, zaehlerRegenMess, CHANGE); // an Pin 2 digital, Dignal bei Aenderung
}

void sendData(int32_t pres, float temp, float humidity, float wind, int winddirection, int rain)
{
    if(client.connect(server,80)){ // connect to my server at port 80
        client.print(postRequestPressure);
        client.print(pres);
        client.print(postRequestTemp);
        client.print(temp);
        client.print(postRequestHumidity);
        client.print(humidity);
        client.print(postRequestWindSpeed);
        client.print(wind);
        client.print(postRequestWindDirection);
        client.print(winddirection);
        client.print(postRequestRain);
        client.print(rain);
        client.println();
        client.stop();
    }else{
          Serial.println("can't connect server");
    }
}

/*
 * catch server-answer like ERROR 505 or something else
 * if everything works correct, the server is not answering
 */
void fetchData()
{
	if(client.available())
	{
    		char c=client.read();
    		Serial.print(c);
  	}
  
  	if(!client.connected())
  	{
    		client.stop();
                Serial.println("client not connected");
   	}
}

float windgeschwmess()
{
    windGeschwMessStart = millis(); // aktualieren Startzeit fuer Messung
    windGeschwMessStartAlt = windGeschwMessStart; // merken uns Startzeit

    windGeschwPinZaehler = 0; // setzen Pulszaehler auf 0
    windGeschwPinStatusAlt = HIGH; // setzen PulsStatus auf HIGH

    while ((windGeschwMessStart - windGeschwMessStartAlt) <= windGeschwMessZeit) // solange die 10000 ms (10 Sekunden) noch nicht durch sind ..
    {
        windGeschwPinStatus = digitalRead(windGeschwPin); // lesen wir Eingang Pin 6 digital und
        if (windGeschwPinStatus != windGeschwPinStatusAlt) // wenn sich Pin-Status geändert hat ...
        {
            if (windGeschwPinStatus == HIGH) // und wenn Status = HIGH
            {
                windGeschwPinZaehler++; // erhoehenn wir den Zaehler um 1
            }
        }
        windGeschwPinStatusAlt = windGeschwPinStatus; // merken uns Pin-Status fuer naechsten Durchlauf
        windGeschwMessStart = millis(); // aktualisieren Zeit
    }

    windGeschw =  ((windGeschwPinZaehler * 24) / 10) + 0.5; //  Windgeschwindigkeit - ein Impuls = 2,4 km/h, aufgerundet -
    windGeschw = (windGeschw / (windGeschwMessZeit / 1000)); // geteilt durch Messzeit in Sekunden

    knoten = windGeschw / 1.852;

    return knoten;
}

/*
 * #############################################################
 * ##################        MAIN       ########################
 * #############################################################
 */
void loop()
{
    zeit1Neu = millis(); // aktualisieren Variable
    if ((zeit1Neu - zeit1Alt) >= intervall) // Abfrage-Intervall ueberschritten?, wenn ja ...
    {
        sensorWertNeu = analogRead(windrichtungsPin); // lesen Sensor an Pin 3 analog
        sensorVolt1 = map(sensorWertNeu, 1, 1024, 1, 5000); // wandeln Werte aus den Bereich 0 - 1024 in 0 bis 5000 um
        windrichtungVoltSumme = windrichtungVoltSumme + sensorVolt1; // bilden Gesamtsumme

        if (zaehler1 == 14) // wenn (0 bis 14 =) 15 Werte gelesen ...
        {
            windrichtungVoltSumme = windrichtungVoltSumme / 15; // bilden Durschnitt
            for (int zaehler3 = 0; zaehler3 <= 15; zaehler3++) // durchlaufen Array fuer Werte in Volt ...bis Bedingung erfüllt
            {
                if (windrichtungVoltSumme <= windrichtungVolt[zaehler3]) // bis richtiger Wert gefunden
                {
                    winddirection = zaehler3;

                    break;  // verlassen Subroutine
                }
            }
            windrichtungVoltSumme = 0; // setzen Variable fuer naechsten Durchlauf zurueck
            zaehler1 = -1; // setzen Variable fuer naechsten Durchlauf zurueck
        }
        zaehler1 = zaehler1++; // erhoehen Schleifenzaehler
        zeit1Alt = zeit1Neu; // setzten verstrichenen Zeitraum auf 0
    }

	// easiest way to get a "timeout" of 5 min
	if (timer >= TIMER_INTERVAL ) {
    		timer = 0;
    
    		temperature = bmp.getTemperature();
            wind = windgeschwmess();
            mengeRegen = ((zaehlerRegen) * 0.28) + 0.5; // 1 Impuls = 0,2794 mm
                
            sendData(bmp.getPressure(), temperature, humidSensor.getHumidity(temperature), wind, windRichtungGrad[winddirection], mengeRegen);
            mengeRegen = 0;
            zaehlerRegen = 0;
  	}
  	timer = timer +1;
  
    delay(5);
}
