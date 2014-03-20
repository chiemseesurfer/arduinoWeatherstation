Copyright &copy; 2012-2014 Max Oberberger, Alexander Zenger

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/.

* * *

# weatherstation
This is a the arduino weatherstation without its own webserver.

Supported sensors:
 * Temperature
 * Humidity
 * Pressure
 * Windspeed
 * Winddirection
 * rainfall

# weatherstation-mega
Arduino Weatherstation with own webserver and SD-Card support.

Supported sensors:
 * Temperature
 * Humidity
 * Pressure

# Dokumentation.pdf
Final Documentation of the project in German.

# arduino.pl
Perl cgi-file for webserver support.
 * Called from arduino with a HTTP-POST Request.
 * Saves all sensor values in seperate rrd-simple databases
