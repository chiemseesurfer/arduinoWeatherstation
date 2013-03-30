/* Weatherstation

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License 
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Max Oberberger
  arduinoWeather@oberbergers.de
  Alexander Zenger
  weather@zengers.de
  
  date: 12-12-02
  
  Get pressure and temperature from the BMP085 and calculate altitude.
  Serial.print it out at 9600 baud to serial monitor.

  hih4030 calculation information:
  http://wiki.bildr.org/index.php/Humidity_Sensor_-_HIH-4030_Breakout
*/

#include <Wire.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <bmp085.h>
#include <hih4030.h>

#include <WebServer.h>
#include <Time.h>
#include <avr/pgmspace.h> 
#include <EEany.h>
#include <Messages.h>
#include <EEPROM.h>

#include <SD.h>


#define PRESSURE_FACTOR 100.0         /**< divide pressure with this factor */
#define HTTP_PORT 80                  /**< arduino listens on this port for http connections */
#define SERVER_PORT 80                /**< http port on server for sending datas */
#define SERIAL_SPEED 9600             /**< speed of the serial console */
#define UPDATE_INTVAL_MILSEC 300000   /**< time after which new datas are gathered, 5 minutes */
#define NTP_UPDATE_INTVAL_SEC 183     /**< time in seconds after which ntp update is requested, 3 minutes 3 seconds */
#define UDP_NTP_PORT 8888             /**< local udp port for ntp requests */
#define NTP_PACKET_SIZE 48            /**< NTP time stamp is in the first 48 bytes of the message */
#define NTP_PORT 123                  /**< NTP PORT */

/**
 * no-cost stream operator as described at 
 * http://sundial.org/arduino/?page_id=119
 */
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ obj.print(arg); return obj; }

// global vars 
bmp085 bmp;                         /**< global object of the bmp085 sensor */
hih4030 humidSensor;                /**< global object of the hih4030 sensor */
float temperature = 0.0;            /**< global var to save temperature */
float pressure = 0.0;               /**< global var to save pressure */
float humidity = 0.0;               /**< global var to save humidity */
time_t lastUpdate = 0;              /**< global var to save when the last update was as time_t */
byte packetBuffer[NTP_PACKET_SIZE]; /**< buffer to hold incoming and outgoing packets */
EthernetUDP udp;                    /**< A UDP instance to let us send and receive packets over UDP */
unsigned long last = 0;             /**< save time when last update was */
/**
 * global var indicates current dbversion. 
 * if this is newer than the one in memory,
 * the db will be reinitialized.
 */
int dbversion = 1003;               
Database db;                        /**< global var to hold current db */
/**
 * On the Ethernet Shield, CS is pin 4. Note that even if it's not
 * used as the CS pin, the hardware CS pin (10 on most Arduino boards,
 * 53 on the Mega) must be left as an output or the SD library
 * functions will not work.
 */
#define chipSelect 4 
/**
 * This creates an instance of the webserver.  By specifying a prefix
 * of "", all pages will be at the root of the server. 
 */
#define PREFIX ""
WebServer webserver(PREFIX, HTTP_PORT);

/*
 * calculate remaining RAM
 */
int freeRam () 
{
	extern int __heap_start, *__brkval; 
	int v; 
  	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

/**
 * startPage for webduino. prints index.html
 */
void startPage(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  // this line sends the standard "we're all OK" headers back 
  // to the browser 
  server.httpSuccess();

  // if we're handling a GET or POST, we can output our data here.
  // For a HEAD request, we just stop after outputting headers. 
  if (type != WebServer::HEAD)
  {
    // this is a special form of print that outputs from PROGMEM
    server.printP(htmlHead);
    server.printP(centerStart);
    server.printP(MainStart);
	 
	 server.printP(pStart);
	 server.printP(htmlTemperature);
	 server << temperature;
   server.print(" C");
	 server.printP(pEnd);

	 server.printP(pStart);
	 server.printP(htmlPressure);
	 server << pressure;
   server.print(" hPa");
	 server.printP(pEnd);

	 server.printP(pStart);
	 server.printP(htmlHumidity);
	 server << humidity;
   server.print(" %");
	 server.printP(pEnd);

	 server.printP(pStart);
	 server.printP(LastUpdate);
	 server << year(lastUpdate) << "-" << month(lastUpdate) << "-" << day(lastUpdate) << "  " << hour(lastUpdate) << ":" << minute(lastUpdate) << ":" << second(lastUpdate);
	 server.printP(pEnd);

	 server.printP(pStart);
	 server.printP(Now);
	 server << year() << "-" << month() << "-" << day() << "  " << hour() << ":" << minute() << ":" << second();
	 server.printP(pEnd);

	 server.printP(centerEnd);
	 server.printP(htmlFoot);
  }
}


/**
 * function for webduino under /json which returns sensor datas in json format
 */
void jsonData(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  if (type == WebServer::POST)
  {
    server.httpFail();
    return;
  }

  server.httpSuccess("application/json");
  
  if (type == WebServer::HEAD)
  {
    return;
  }

  server.printP(jsonStart);
  server.printP(jsonTemperature);
  server << temperature;
  server.printP(jsonSeperater);
  server.printP(jsonPressure);
  server << pressure;
  server.printP(jsonSeperater);
  server.printP(jsonHumidity);
  server << humidity;
  server.printP(jsonEnd);
}

/**
 * function for webduino under /setip.html which allows to make the ip related
 * settings
 */
void setipPage(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  // this line sends the standard "we're all OK" headers back 
  // to the browser 
  server.httpSuccess();

  if (type == WebServer::POST)  {
    Settings S = db.settings;
    bool repeat = true;
	 char parsename[32], parsevalue[32];

    while (repeat) {
      repeat = server.readPOSTparam(parsename, 32, parsevalue, 32);
      if (strcmp(parsename, "ip1") == 0){S.ip_oct1 = atoi(parsevalue);}
      if (strcmp(parsename, "ip2") == 0){S.ip_oct2 = atoi(parsevalue);}
      if (strcmp(parsename, "ip3") == 0){S.ip_oct3 = atoi(parsevalue);}
      if (strcmp(parsename, "ip4") == 0){S.ip_oct4 = atoi(parsevalue);}
      if (strcmp(parsename, "gw1") == 0){S.gw_oct1 = atoi(parsevalue);}
      if (strcmp(parsename, "gw2") == 0){S.gw_oct2 = atoi(parsevalue);}
      if (strcmp(parsename, "gw3") == 0){S.gw_oct3 = atoi(parsevalue);}
      if (strcmp(parsename, "gw4") == 0){S.gw_oct4 = atoi(parsevalue);}
      if (strcmp(parsename, "ntp1") == 0){S.ntp_oct1 = atoi(parsevalue);}
      if (strcmp(parsename, "ntp2") == 0){S.ntp_oct2 = atoi(parsevalue);}
      if (strcmp(parsename, "ntp3") == 0){S.ntp_oct3 = atoi(parsevalue);}
      if (strcmp(parsename, "ntp4") == 0){S.ntp_oct4 = atoi(parsevalue);}
      if (strcmp(parsename, "srv1") == 0){S.srv_oct1 = atoi(parsevalue);}
      if (strcmp(parsename, "srv2") == 0){S.srv_oct2 = atoi(parsevalue);}
      if (strcmp(parsename, "srv3") == 0){S.srv_oct3 = atoi(parsevalue);}
      if (strcmp(parsename, "srv4") == 0){S.srv_oct4 = atoi(parsevalue);}
    }

	 bool error = false;
	 // check ip
    if(S.ip_oct1 < 1 || S.ip_oct1 > 255 || S.ip_oct2 > 255 || S.ip_oct3 > 255 || S.ip_oct4 > 255) 
	 {
	   error = true;
    }
	 else if(S.gw_oct1 < 1 || S.gw_oct1 > 255 || S.gw_oct2 > 255 || S.gw_oct3 > 255 || S.gw_oct4 > 255)
	 {
		error = true;
	 }
	 else if(S.ntp_oct1 < 1 || S.ntp_oct1 > 255 || S.ntp_oct2 > 255 || S.ntp_oct3 > 255 || S.ntp_oct4 > 255)
	 {
		error = true;
	 }
	 else if(S.srv_oct1 < 1 || S.srv_oct1 > 255 || S.srv_oct2 > 255 || S.srv_oct3 > 255 || S.srv_oct4 > 255)
	 {
		error = true;
	 }

	 if(error == true)
	 {
  		server.printP(htmlHead);
		server.printP(centerStart);
		server.printP(ErrorStart);
		server.printP(ErrorIP);
		server.printP(centerEnd);
		server.printP(htmlFoot);
    	db = ee_getdb();
      return;
	 }

    ee_wr_settings(S);
    db = ee_getdb();
  }

  // if we're handling a GET or POST, we can output our data here.
  // For a HEAD request, we just stop after outputting headers. 
  if (type != WebServer::HEAD)
  {
    server.printP(htmlHead);
    server.printP(centerStart);
    server.printP(SetIpStart);
	  server.printP(SetIpBeginForm);

	  server.printP(SetIpIpCur);
	  server << db.settings.ip_oct1 << "." << db.settings.ip_oct2 << "." << db.settings.ip_oct3 << "." << db.settings.ip_oct4;
	  server.printP(pEnd);
	  server.printP(SetIpIpForm);
	  server.printP(SetIpGwCur);
	  server << db.settings.gw_oct1 << "." << db.settings.gw_oct2 << "." << db.settings.gw_oct3 << "." << db.settings.gw_oct4;
	  server.printP(pEnd);
	  server.printP(SetIpGwForm);

	  server.printP(SetIpNtpCur);
	  server << db.settings.ntp_oct1 << "." << db.settings.ntp_oct2 << "." << db.settings.ntp_oct3 << "." << db.settings.ntp_oct4;
	  server.printP(pEnd);
	  server.printP(SetIpNtpForm);

	  server.printP(SetIpSrvCur);
	  server << db.settings.srv_oct1 << "." << db.settings.srv_oct2 << "." << db.settings.srv_oct3 << "." << db.settings.srv_oct4;
	  server.printP(pEnd);
	  server.printP(SetIpSrvForm);

	  server.printP(SetIpEndForm);
	  server.printP(centerEnd);
	  server.printP(htmlFoot);
  }
}

/**
 * function for webduino under /setsup.html which allows to make the data
 * related settings
 */
void setSupPage(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  // this line sends the standard "we're all OK" headers back 
  // to the browser 
  server.httpSuccess();

  if (type == WebServer::POST)  
  {
    Settings S = db.settings;
    bool repeat = true;
	  char parsename[16], parsevalue[16];

	  S.sdCard = false;
	  S.sendToSrv = false;
	  S.sendToSerial = false;

    while (repeat) 
	  {
      repeat = server.readPOSTparam(parsename, 16, parsevalue, 16);
      if (strcmp(parsename, "sdcard") == 0){S.sdCard = parsevalue;}
      if (strcmp(parsename, "server") == 0){S.sendToSrv = parsevalue;}
      if (strcmp(parsename, "serial") == 0){S.sendToSerial = parsevalue;}
    }

    ee_wr_settings(S);
    db = ee_getdb();

  }

  // if we're handling a GET or POST, we can output our data here.
  // For a HEAD request, we just stop after outputting headers. 
  if (type != WebServer::HEAD)
  {
    server.printP(htmlHead);
    server.printP(centerStart);
    server.printP(SetSupStart);
	  server.printP(SetSupBeginForm);

	  server.checkBox("sdcard", "sdcard", "enable sdcard", db.settings.sdCard);
	  server.printP(br);
	  server.checkBox("server", "server", "enable server", db.settings.sendToSrv);
	  server.printP(br);
	  server.checkBox("serial", "serial", "enable serial", db.settings.sendToSerial);
	  server.printP(br);

	  server.printP(SetSupEndForm);
	  server.printP(centerEnd);
	  server.printP(htmlFoot);
  }
}


/**
 * prints data.csv for download
 */
void listCsv(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  if (type == WebServer::POST)
  {
    server.httpFail();
    return;
  }
  
  server.httpSuccess("text/plain");

  if (type == WebServer::HEAD)
  {
    return;
  }

  server.println("unixtime,temperature,pressure,humidity");
  File entry =  SD.open("data.csv");

  if(entry) 
  {
    int16_t c;
    while ((c = entry.read()) > 0) 
    {
      server.print((char)c);
    }
    entry.close();
  }
  else
  {
    server.println("Error opening data.csv");
  }
}

/**
 * send sensor data to the configured server
 */
void sendtoServer()
{
  // client which posts some data to a webserver
  EthernetClient client; 

  byte server[] = { db.settings.srv_oct1, db.settings.srv_oct2, db.settings.srv_oct3, db.settings.srv_oct4 };
  // connect to my server at port 80
  if(client.connect(server,SERVER_PORT)) 
  { 
	  client.print(F("POST /cgi-bin/arduino.pl?P="));
	  client.print(pressure);
	  client.print(F("&T="));
	  client.print(temperature);
    client.print(F("&H="));
    client.print(humidity);
	  client.println();
	  client.stop();
	} 
  else 
  {
	  Serial.println(F("can't connect server"));
  }
}

/**
 * send an NTP request to the time server at the given address 
 */
void sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp: 		   
  udp.beginPacket(address, NTP_PORT); //NTP requests are to port 123
  udp.write(packetBuffer,NTP_PACKET_SIZE);
  udp.endPacket(); 
}

/**
 * function called regulary to initate the ntp requests
 */
unsigned long getNTPTime()
{
  // ntp server time.nist.gov
  IPAddress timeServer(db.settings.ntp_oct1, db.settings.ntp_oct2, db.settings.ntp_oct3, db.settings.ntp_oct4 );

  // send an NTP packet to a time server
  sendNTPpacket(timeServer); 

  // wait to see if a reply is available
  delay(1000);

  if( udp.parsePacket() ) {

    // We've received a packet, read the data from it
	 // read the packet into the buffer
    udp.read(packetBuffer,NTP_PACKET_SIZE);  

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long unix_start = 2208988800UL;     
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - unix_start;  

	 return epoch;
  }
  else
  {
     return 0;
  }
}

/**
 * prints current data to the serial console
 */
void sendToSerial()
{
  Serial.print(F("Temperature = "));
  Serial.println(temperature);
  Serial.print(F("Pressure = "));
  Serial.println(pressure);
  Serial.print(F("Humidity = "));
  Serial.println(humidity);
  Serial.print(F("FreeRAM = "));
  Serial.println(freeRam());
  Serial.print(F("LasteUpdate = "));
  Serial.print(hour(lastUpdate));
  Serial.print(F(":"));
  Serial.print(minute(lastUpdate));
  Serial.print(F(":"));
  Serial.println(second(lastUpdate));
  Serial.print(F("Current IP: "));
  Serial << db.settings.ip_oct1 << "." << db.settings.ip_oct2 << "." << db.settings.ip_oct3 << ".";
	Serial.println(db.settings.ip_oct4);
}

/**
 * updates the sensor datas, and sends them to the targets which are configured (serial, server, sdcard)
 */
void updateData()
{
 	// update sensor datas in global vars
	temperature = bmp.getTemperature();
  pressure = bmp.getPressure()/PRESSURE_FACTOR;
	humidity = humidSensor.getHumidity(temperature);
	// remember when we updated
	lastUpdate = now();

	// check if enabled, and then send new data to server
	if(db.settings.sendToSrv == true)
	{
   	sendtoServer();
	}

	// check if enabled, and then save new data to sdcard
	if(db.settings.sdCard == true)
	{
		dataToFile();
	}

	// check if enabled, and then send new data to serial console
	if(db.settings.sendToSerial == true)
	{
		sendToSerial();
	}
}

/**
 * write data to file on sdcard
 */
void dataToFile()
{
	// buffer vor dtostrf
	char test[10];

	// String for data log line
	String dataString = "";

	dataString = String(lastUpdate);
	dataString += ",";
	dtostrf(temperature,5,2,test);
	dataString += test;
	dataString += ",";
	dtostrf(pressure,5,2,test);
	dataString += test;
	dataString += ",";
	dtostrf(humidity,5,2,test);
	dataString += test;

	// open the file. note that only one file can be open at a time,
 	// so you have to close this one before opening another.
	File dataFile = SD.open("data.csv", FILE_WRITE);

	if(dataFile)
	{
		dataFile.println(dataString);
	  dataFile.close();
	}
	else
	{
		Serial.println(F("error opening data.csv"));
	}

}

/**
 * initiale setup of everything we need, when the controller boots
 */
void setup()
{
	// set baudrate for Serial Port to 9600
	Serial.begin(SERIAL_SPEED); 
	Serial.println("Beginning");

	ee_init(false);
	db = ee_getdb();

  // init db, when dbversion in memory is different than in the data structure
	if(dbversion > db.header.version) 
	{
		ee_init(true);
		db = ee_getdb();    
	}

	// Arduino Mac-Adress
	byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; 
	// Arduino IP-Adress
  byte ip[] = { db.settings.ip_oct1, db.settings.ip_oct2, db.settings.ip_oct3, db.settings.ip_oct4 };
	// Network-Gateway in my local network (Router)
  byte gateway[] = { db.settings.gw_oct1, db.settings.gw_oct2, db.settings.gw_oct3, db.settings.gw_oct4 };

	Serial.println("Loaded settings from EEPROM");

	// "start" Ethernet-connection
	Ethernet.begin(mac, ip, gateway, gateway); 

	Serial.println("Ethernet configured");

	// start sensors
  humidSensor.begin();
	bmp.begin();
  
	// make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);                    // but turn off the W5100 chip!

	// see if the card is present and can be initialized:
  if(SD.begin(chipSelect)) 
	{
	  db.settings.sdCard == true;
		Serial.println("card initialized.");
	}
  else
  {
		Serial.println("error card not initialized.");
  }
  
  // setup our default command that will be run when the user accesses
  // the root page on the server 
  webserver.setDefaultCommand(&startPage);
  // run the same command if you try to load /index.html, a common
  // default page name 
  webserver.addCommand("index.html", &startPage);
	// function for path /json
	webserver.addCommand("json", &jsonData);
	// function for path /setip.html 
	webserver.addCommand("setip.html", &setipPage);
	// function for path /setsup.html
	webserver.addCommand("setsup.html", &setSupPage);
  // function for path /data.csv
  webserver.addCommand("data.csv", &listCsv);

  // start the webserver
  webserver.begin();

	Serial.println("Webserver configured");

	// listen on specific udp port for data
	udp.begin(UDP_NTP_PORT);
  // set function for time update
	setSyncProvider(getNTPTime);
  // set interval for time update in seconds
  setSyncInterval(NTP_UPDATE_INTVAL_SEC);

	Serial.println("NTP configured");

	// get first set of datas
	updateData();

	Serial.println("Setup finishied, begin collecting data...");
}

/*
 * main function which will run in loop all the time
 */
void loop()
{
	if (millis() - last > UPDATE_INTVAL_MILSEC) 
	{
   	last = millis();

    // get new sensor datas
		updateData();
 	}

 	// process incoming connections one at a time forever 
 	webserver.processConnection();
}
