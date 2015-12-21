#include <DS3232RTC.h>
#include <Time.h> 
#include <Wire.h> 
#include "FastLED.h"
#include <BH1750.h>
#define NUM_LEDS 29 // Number of LED controles (remember I have 3 leds / controler)
#define COLOR_ORDER RGB  // Define color order for your strip
#define DATA_PIN 6  // Data pin for led comunication
#define BRIGHTNESS_THRESHOLD 150
#define BLACK 0x000000

CRGB leds[NUM_LEDS]; // Define LEDs strip
byte digits[10][7] = { {0,1,1,1,1,1,1},  // Digit 0
					 {0,1,0,0,0,0,1},   // Digit 1
					 {1,1,1,0,1,1,0},   // Digit 2
					 {1,1,1,0,0,1,1},   // Digit 3
					 {1,1,0,1,0,0,1},   // Digit 4
					 {1,0,1,1,0,1,1},   // Digit 5
					 {1,0,1,1,1,1,1},   // Digit 6
					 {0,1,1,0,0,0,1},   // Digit 7
					 {1,1,1,1,1,1,1},   // Digit 8
					 {1,1,1,1,0,1,1} };  // Digit 9 | 2D Array for numbers on 7 segment
bool Dot = true;  //Dot state
int timeChanged = 0;
int ledColor = 0xFF9933; // Color used (in hex)
uint16_t brightness = 0;
BH1750 lightMeter;

void setup(){
	Serial.begin(9600);
	Wire.begin();
	lightMeter.begin();
	LEDS.addLeds<WS2811, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS); // Set LED strip type
	LEDS.setBrightness(255); // Set initial brightness
	pinMode(2, INPUT_PULLUP); // Define ???
	pinMode(4, INPUT_PULLUP); // Define Hours adjust button pin
	pinMode(5, INPUT_PULLUP); // Define Minutes adjust button pin
	pinMode(3, INPUT_PULLUP); // Defines Minutes adjust button pin
}
// Get time in a single number, if hours will be a single digit then time will be displayed 155 instead of 0155
int GetTime() {
	tmElements_t Now;
	RTC.read(Now);
	printTime(Now);
	int hour = Now.Hour;
	int minutes = Now.Minute;
	int second = Now.Second;
	if (second % 2 == 0) 
	{ 
		Dot = false; 
	}
	else 
	{ 
		Dot = true; 
	};
	return (hour * 100 + minutes);
};

// Check Light sensor and set brightness accordingly
void BrightnessCheck() {
	const byte brightnessLow = 5; // Low brightness value
	const byte brightnessHigh = 255; // High brightness value
	uint16_t sensorValue = lightMeter.readLightLevel();
	uint16_t difference = sensorValue - brightness;
	difference = abs(difference);
	if (difference > 10)
	{
		Serial.print("Brightness Changed from "); Serial.print(brightness); Serial.print(" to "); Serial.println(sensorValue);
		brightness = sensorValue;
		if (brightness < BRIGHTNESS_THRESHOLD)
		{
			LEDS.setBrightness(brightnessHigh);
		}
		else
		{
			LEDS.setBrightness(brightnessLow);
		}
	}
};

// Convert time to array needet for display 
void TimeToArray() {
	int Now = GetTime();  // Get time
	int cursor = 30;

	if (Dot) 
	{ 
		leds[14] = ledColor;
		leds[15] = ledColor;
	}
	else 
	{
		leds[14] = BLACK;
		leds[15] = BLACK;
	};
	for (int i = 1; i <= 4; i++) {
		int digit = Now % 10; // get last digit in time
		if (i == 1) {
			cursor = 23;
			for (int k = 0; k <= 6; k++) {
				if (digits[digit][k] == 1) { leds[cursor] = ledColor; }
				else if (digits[digit][k] == 0) { leds[cursor] = BLACK; };
				cursor++;
			};
		}
		else if (i == 2) {
			cursor -= 14;
			for (int k = 0; k <= 6; k++) {
				if (digits[digit][k] == 1) { leds[cursor] = ledColor; }
				else if (digits[digit][k] == 0) { leds[cursor] = BLACK; };
				cursor++;
			};
		}
		else if (i == 3) {
			cursor = 7;
			for (int k = 0; k <= 6; k++) {
				if (digits[digit][k] == 1) { leds[cursor] = ledColor; }
				else if (digits[digit][k] == 0) { leds[cursor] = BLACK; };
				cursor++;
			};
		}
		else if (i == 4) {
			cursor = 0;
			for (int k = 0; k <= 6; k++) {
				if (digits[digit][k] == 1) { leds[cursor] = ledColor; }
				else if (digits[digit][k] == 0) { leds[cursor] = BLACK; };
				cursor++;
			};
		}
		Now /= 10;
	};
};

void printTime(tmElements_t time)
{
	if (time.Second != timeChanged)
	{
		timeChanged = time.Second;
		Serial.print("Time: ");
		Serial.print(time.Hour);
		Serial.print(":");
		Serial.print(time.Minute);
		Serial.print(":");
		Serial.print(time.Second);
		Serial.println("");
	}
}

void TimeAdjust() {
	int buttonH = digitalRead(4);
	int buttonM = digitalRead(5);
	if (buttonH == LOW || buttonM == LOW)
	{
		delay(500);
		tmElements_t Now;
		RTC.read(Now);
		int hour = Now.Hour;
		int minutes = Now.Minute;
		int second = Now.Second;
		if (buttonH == LOW) {
			if (Now.Hour == 23) { Now.Hour = 0; }
			else { Now.Hour += 1; };
		}
		else {
			if (Now.Minute == 59)
			{
				Now.Minute = 0;
			}
			else {
				Now.Minute += 1;
			};
		};
		printTime(Now);
		RTC.write(Now);
	}
}
void loop()  // Main loop
{
	BrightnessCheck(); // Check brightness
	TimeAdjust(); // Check to se if time is geting modified
	TimeToArray(); // Get leds array with required configuration
	FastLED.show(); // Display leds array
}