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
 */

#include "hih4030.h"

hih4030::hih4030(){
}

/*
 * initialize Analog Input pin
 */
void
hih4030::begin(void)
{
	pinMode(HIH4030_PIN, INPUT); // analog input
}

/*
 * read analog input and calculate humidity according to the data sheet
 */
float
hih4030::getHumidity(float temp)
{
	hih4030_value = analogRead(HIH4030_PIN);
	voltage = hih4030_value / 1023.0 * SUPPLYVOLTAGE;        // 1023 (10 Bit)=Volt; http://arduino.cc/playground/CourseWare/AnalogInput
    sensorRH = voltage - ZERO_OFFSET / SLOPE;
	trueRH = sensorRH / (1.0546 - 0.00216 * temp);        // according to Datasheet
	
	return trueRH;
}
