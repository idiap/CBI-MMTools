/**
* Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/
* Written by François Marelli <francois.marelli@idiap.ch>
* 
* This file is part of CBI-MMTools.
* 
* CBI-MMTools is free software: you can redistribute it and/or modify
* it under the terms of the 3-Clause BSD License.
* 
* CBI-MMTools is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* 3-Clause BSD License for more details.
* 
* You should have received a copy of the 3-Clause BSD License along
* with CBI-MMTools. If not, see https://opensource.org/licenses/BSD-3-Clause.
* 
* SPDX-License-Identifier: BSD-3-Clause 
*/

#include <Arduino.h>
#define BAUDRATE 9600

const unsigned short RESET_CODE = 0;
const unsigned short FIRMWARE_CODE = 1;
const unsigned short VERSION_CODE = 2;
const unsigned short TRIG_CODE = 3;
const unsigned short MOD_CODE = 4;
const unsigned short EXT_CODE = 5;

const unsigned char ACK = 0;
const unsigned char ERR = 1;
const unsigned char WARN = 2;

const unsigned long timeOut = 500;

const unsigned int VERSION = 1;
const char FIRMWARE_ID[] = "MM-ArdIllu";

const unsigned int switchPin = 9;
const unsigned int switchPinB = 10;
const unsigned int buttonPin = 11;

// Software control pins
const unsigned int trigPin = 5;
const unsigned int modPin = 3;

// Multiplexer control
const unsigned int multPinA = 7;
const unsigned int multPinB = 8;

bool manualMode = false;
bool isOn = false;
bool lastButtonState = true;

bool externalMode = false;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup()
{
	pinMode(switchPin, INPUT_PULLUP);
	pinMode(switchPinB, INPUT_PULLUP);
	pinMode(buttonPin, INPUT_PULLUP);

	pinMode(trigPin, OUTPUT);
	pinMode(modPin, OUTPUT);

	pinMode(multPinA, OUTPUT);
	pinMode(multPinB, OUTPUT);

	reset();

	if (digitalRead(switchPin))
	{
		externalMode = true;
		digitalWrite(multPinA, HIGH);
		digitalWrite(multPinB, LOW);
	}
	else
	{
		externalMode = false;
		digitalWrite(multPinA, LOW);
		digitalWrite(multPinB, LOW);
	}

	Serial.begin(BAUDRATE);
}

void loop()
{
	bool switchReading = digitalRead(switchPin);

	if (switchReading && !externalMode)
	{
		externalMode = true;
		digitalWrite(multPinA, HIGH);
		digitalWrite(multPinB, LOW);
	}

	else if (!switchReading && externalMode)
	{
		externalMode = false;
		reset();
		digitalWrite(multPinA, LOW);
		digitalWrite(multPinB, LOW);
	}

	if (Serial.available() > 0)
	{
		int inByte = Serial.read();

		switch (inByte)
		{

		// Returns firmware name
		case FIRMWARE_CODE:
			Serial.println(FIRMWARE_ID);
			break;

		// Returns version number
		case VERSION_CODE:
			Serial.println(VERSION);
			break;

		// Returns 1 if external mode is on
		case EXT_CODE:
			Serial.println(externalMode);

		// Shut down the light
		case RESET_CODE:
			if (externalMode)
			{
				Serial.write(WARN);
				break;
			}
			reset();
			break;

		// Set the trigger state
		case TRIG_CODE:
			if (externalMode)
			{
				Serial.write(WARN);
				break;
			}
			if (waitForSerial(timeOut))
			{
				if (manualMode)
				{
					manualMode = false;
					digitalWrite(multPinA, LOW);
					digitalWrite(multPinB, LOW);
				}
				int value = Serial.read();
				digitalWrite(trigPin, value <= 0);
				isOn = value <= 0;
				Serial.write(ACK);
			}
			break;

		// Set the illumination power
		case MOD_CODE:
			if (externalMode)
			{
				Serial.write(WARN);
				break;
			}
			if (waitForSerial(timeOut))
			{
				if (manualMode)
				{
					manualMode = false;
					digitalWrite(multPinA, LOW);
					digitalWrite(multPinB, LOW);
				}
				int value = Serial.read();
				analogWrite(modPin, 255 - value);
				Serial.write(ACK);
			}
			break;

		default:
			Serial.write(ERR);
		}
	}

	else if (!externalMode && !isOn)
	{
		bool reading = digitalRead(buttonPin);
		if (reading != lastButtonState && (millis() - lastDebounceTime) > debounceDelay)
		{
			lastDebounceTime = millis();
			lastButtonState = reading;
			if (!reading)
			{
				manualMode = !manualMode;
				if (manualMode)
				{
					digitalWrite(multPinA, LOW);
					digitalWrite(multPinB, HIGH);
				}
				else
				{
					digitalWrite(multPinA, LOW);
					digitalWrite(multPinB, LOW);
				}
				digitalWrite(trigPin, !manualMode);
			}
		}
	}
}

void reset()
{
	// Trigger is inverted logic!
	digitalWrite(trigPin, HIGH);
	analogWrite(modPin, 255);
	manualMode = false;
	isOn = false;
	if (!externalMode)
	{
		digitalWrite(multPinA, LOW);
		digitalWrite(multPinB, LOW);
	}
}

bool waitForSerial(unsigned long timeOut)
{
	unsigned long startTime = millis();

	while (!Serial.available())
	{
		if (millis() - startTime > timeOut)
		{
			return false;
		}
	}

	return true;
}
