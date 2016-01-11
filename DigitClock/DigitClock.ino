#include <DS3232RTC.h>
#include <Time.h> 
#include <Wire.h> 
#include "FastLED.h"
#include <BH1750.h>
#define NUM_LEDS 30 // Number of LED controles (remember I have 3 leds / controller)
#define COLOR_ORDER RGB  // Define color order for your strip
#define DATA_PIN 6  // Data pin for led comunication
#define BRIGHTNESS_THRESHOLD 150
#define BLACK 0x000000
#define FRAMES_PER_SECOND  24
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

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
int digitOffset[4] = { 23, 16, 7, 0 };
bool dotIsVisible = true;
int timeChanged = 0;
uint16_t brightness = 0;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
BH1750 lightMeter;

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void(*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow };
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current

void setup(){
	Serial.begin(9600);
	Wire.begin();
	lightMeter.begin();
	delay(3000); // 3 second delay for recovery
	LEDS.addLeds<WS2811, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);; // Set LED strip type
	LEDS.setBrightness(255); // Set initial brightness
	pinMode(2, INPUT_PULLUP); // Define Color Mode
	pinMode(4, INPUT_PULLUP); // Define Hours adjust button pin
	pinMode(5, INPUT_PULLUP); // Define Minutes adjust button pin
	pinMode(3, INPUT_PULLUP); // Defines Minutes adjust button pin
}

void nextPattern()
{
	// add one to the current pattern number, and wrap around at the end
	Serial.println("Next pattern");
	gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

void rainbow()
{
	// FastLED's built-in rainbow generator
	fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

int GetTime() {
	tmElements_t Now;
	RTC.read(Now);
	printTime(Now);
	int hour = Now.Hour;
	int minutes = Now.Minute;
	int second = Now.Second;

	(second % 2 == 0) ? dotIsVisible = false : true;
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

// Convert time to array needed for display 
void TimeToArray() {
	int Now = GetTime();
	int cursor = NUM_LEDS;
	CHSV color = CHSV(gHue, 255, 255);

	dotIsVisible ? (leds[14] = color) : (leds[14] = BLACK);
	dotIsVisible ? (leds[15] = color) : (leds[15] = BLACK);

	for (int i = 0; i < 4; i++)
	{
		int digit = Now % 10; // get last digit in time
		int cursor = digitOffset[i];
		for (int k = 0; k <= 6; k++) {
			if (digits[digit][k] == 1) { leds[cursor] = color; }
			else if (digits[digit][k] == 0) { leds[cursor] = BLACK; };
			cursor++;
		};
		Now /= 10;
	}
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
	int buttonColor = digitalRead(2);
	if (buttonH == LOW || buttonM == LOW || buttonColor == LOW)
	{
		delay(500);
		tmElements_t Now;
		RTC.read(Now);
		int hour = Now.Hour;
		int minutes = Now.Minute;
		int second = Now.Second;
		if (buttonH == LOW) {
			if (Now.Hour == 23) 
			{ 
				Now.Hour = 0; 
			}
			else 
			{ 
				Now.Hour += 1; 
			};
		}
		else if(buttonM == LOW) {
			if (Now.Minute == 59)
			{
				Now.Minute = 0;
			}
			else {
				Now.Minute += 1;
			};
		}
		else if (buttonColor == LOW)
		{
			nextPattern();
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
	// send the 'leds' array out to the actual LED strip
	// Call the current pattern function once, updating the 'leds' array
	//gPatterns[gCurrentPatternNumber]();
	FastLED.show();
	gHue++;
	// insert a delay to keep the framerate modest
	FastLED.delay(1000 / FRAMES_PER_SECOND);
	//EVERY_N_MILLISECONDS(20) { gHue++; } // slowly cycle the "base color" through the rainbow
}