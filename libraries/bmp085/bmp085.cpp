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
#include "bmp085.h"
#include <util/delay.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino.h>

bmp085::bmp085(){
}

/*
 * start to initialize needed values
 */
void
bmp085::begin(void)
{
	Wire.begin();
	
	ac1 = read16Bit(BMP085_CAL_AC1);
  	ac2 = read16Bit(BMP085_CAL_AC2);
  	ac3 = read16Bit(BMP085_CAL_AC3);
  	ac4 = read16Bit(BMP085_CAL_AC4);
  	ac5 = read16Bit(BMP085_CAL_AC5);
  	ac6 = read16Bit(BMP085_CAL_AC6);
  	b1 = read16Bit(BMP085_CAL_B1);
  	b2 = read16Bit(BMP085_CAL_B2);
  	mb = read16Bit(BMP085_CAL_MB);
  	mc = read16Bit(BMP085_CAL_MC);
  	md = read16Bit(BMP085_CAL_MD);
	OSS = 0;
}

/* 
 * @return temperature-value in degrees
 */
float
bmp085::getTemperature(void)
{
	int32_t ut, x1, x2, b5;
	float temp;
	
	ut = readUT();
	x1 = ((ut - (int32_t)ac6) * (int32_t)ac5) >> 15;
  	x2 = ((int32_t)mc << 11) - (x1 + md)/2;     // round up
  	x2 /= (x1 + md);
  	b5 = x1 + x2;
  	
  	temp = (b5 + 8) >> 4;
  	temp /=10;

  	return temp;
}

  
/* 
 * @return pressure-value in Pascal
 */
int32_t
bmp085::getPressure(void)
{	
	int32_t x1, x2, x3, b3, b6, p, b5, ut, up;
	uint32_t b4, b7;
	
	ut = readUT();
	up = readUP();
	
	// some temperature calculation
	x1 = ((ut - (int32_t)ac6) * (int32_t)ac5) >> 15;
  	x2 = ((int32_t)mc << 11) - (x1 + md)/2;     // round up
  	x2 /= (x1 + md);
  	b5 = x1 + x2;
	
	b6 = b5 - 4000;
	// Calculate B3
	x1 = ((int32_t)b2 * (b6 * b6)>>12)>>11;
	x2 = ((int32_t)ac2 * b6)>>11;
	x3 = x1 + x2;
	b3 = (((((int32_t)ac1)*4 + x3)<<OSS) + 2) / 4;
	
	// Calculate B4
	x1 = ((int32_t)ac3 * b6)>>13;
	x2 = ((int32_t)b1 * ((b6 * b6)>>12))>>16;
	x3 = ((x1 + x2) + 2)>>2;
	b4 = ((uint32_t)ac4 * (uint32_t)(x3 + 32768))>>15;
	b7 = ((uint32_t)(up - b3) * (uint32_t)(50000>>OSS));
	
	if (b7 < 0x80000000)
		p = (b7 * 2) / b4;
	else
		p = (b7/b4) * 2;
	  
	x1 = (p>>8) * (p>>8);
	x1 = (x1 * 3038)>>16;
	x2 = (-7357 * p)>>16;
	p += (x1 + x2 + (int32_t)3791)>>4;
	
	return p;
}

uint8_t
bmp085::read8Bit(uint8_t address)
{
	uint8_t returnValue;
	unsigned char data;
	
	Wire.beginTransmission(BMP085_ADDRESS);
	Wire.write(address);
	Wire.endTransmission();
	
	Wire.beginTransmission(BMP085_ADDRESS);
	Wire.requestFrom(BMP085_ADDRESS, 1);
	returnValue = Wire.read();
	  
	return returnValue;
}

uint16_t
bmp085::read16Bit(uint8_t address)
{
	uint16_t returnValue;
	
	Wire.beginTransmission(BMP085_ADDRESS);
	Wire.write(address);
	Wire.endTransmission();
	
	Wire.beginTransmission(BMP085_ADDRESS);
	Wire.requestFrom(BMP085_ADDRESS, 2);
	while(Wire.available()<2)
	  ;
	returnValue = Wire.read();
	returnValue <<= 8;
	returnValue |= Wire.read();
	
	Wire.endTransmission();
	
	return returnValue;
}

uint32_t
bmp085::readUT(void)
{
	uint32_t ut;
	
	// Write 0x2E into Register 0xF4
	// This requests a temperature reading
	write8Bit(BMP085_CONTROL, BMP085_READTEMPCMD);
	_delay_ms(5);
	
	// Read two bytes from registers 0xF6 and 0xF7
	ut = read16Bit(BMP085_TEMPDATA);
	return ut;
}

void
bmp085::write8Bit(uint8_t address, uint8_t data)
{
	Wire.beginTransmission(BMP085_ADDRESS);
	Wire.write(address);
	Wire.write(data);
	Wire.endTransmission();
}

uint32_t
bmp085::readUP(void)
{
	uint32_t raw;
	unsigned long up = 0;
	
	// Write 0x34+(OSS<<6) into register 0xF4
	// Request a pressure reading w/ oversampling setting
	write8Bit(BMP085_CONTROL, BMP085_READPRESSURECMD + (OSS<<6));
	
	// Wait for conversion, delay time dependent on OSS
	_delay_ms(2 + (3<<OSS));
	
	// Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
	raw = read16Bit(BMP085_PRESSUREDATA);//Wire.beginTransmission(BMP085_ADDRESS);
	
	raw <<= 8;
	raw |= read8Bit(BMP085_PRESSUREDATA+2);
	raw >>= (8 - OSS);
	
	return raw;
}
