/*
 * Copyright (c) 2013 Max Oberberger (max@oberbergers.de)
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

#define TIMER_INTERVAL 50000 	       // to get a timeout of 5 min

bmp085 bmp;
hih4030 humidSensor;

float temperature;
long timer = 599900;

// Internet values
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // Arduino Mac-Adress
byte ip[] = { 192, 168, 3, 130}; // Arduino IP-Adress
byte server[]={0, 0, 0, 0}; // IP-Adress of server TODO: replace 0 with IP
byte gateway[]={192, 168, 3, 1}; // Network-Gateway in my local network (Router)
EthernetClient client; // the arduino is a client, which posts some data to a webserver
char *postRequestPressure="POST /cgi-bin/arduino.pl?P=";
char *postRequestTemp="&T=";
char *postRequestHumidity="&H=";

void setup()
{
	Serial.begin(9600); // set baudrate for Serial Port to 9600
	Ethernet.begin(mac, ip, gateway); // "start" Ethernet-connection
        
    humidSensor.begin();
	bmp.begin();
}

void sendData(int32_t pres, float temp, float humidity)
{
        
	Serial.print("pressure: ");
	Serial.println(pres/100.0);
	Serial.print("temp: ");
	Serial.println(temp);
    Serial.print("humidity: ");
    Serial.println(humidity);
	
	if(client.connect(server,80)){ // connect to server at port 80
		client.print(postRequestPressure);
		client.print(pres);
		client.print(postRequestTemp);
		client.print(temp);
        client.print(postRequestHumidity);
        client.print(humidity);
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

/*
 * #############################################################
 * ##################        MAIN       ########################
 * #############################################################
 */
void loop()
{
	// easiest way to get a "timeout" of 5 min
	if (timer >= TIMER_INTERVAL ) {
    		timer = 0;
    
    		temperature = bmp.getTemperature();
                
            sendData(bmp.getPressure(), temperature, humidSensor.getHumidity(temperature));
  	}
  	timer = timer +1;
  
  	delay(6);
}
