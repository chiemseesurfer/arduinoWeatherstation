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

#ifndef HIH4030_H_
#define HIH4030_H_

#include <Wire.h>
#include <SPI.h>
#include <math.h>

#define HIH4030_PIN 3
#define SUPPLYVOLTAGE 5.1       // Arduino PowerSupply with 5 Volt
#define SLOPE 0.0307
#define ZERO_OFFSET 0.958


class hih4030{
	public:
		hih4030();

        // initialize Analog Input pin
		void begin(void);

		// read analog input and calculate humidity according to the data sheet
		float getHumidity(float temp);
	
	private:
		int hih4030_value;
		float voltage, sensorRH, trueRH; // RH = Real Humidity
	
};
#endif
