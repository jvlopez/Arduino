#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <EEPROM.h>
#include <BH1750.h>
#include <FastLED.h>

#define DEBUG_OVER_SERIAL
#ifdef DEBUG_OVER_SERIAL
	#include "ardprintf.h"
	#define DEBUG_OPEN_SERIAL(x) Serial.begin(x)
	#define DEBUG_PRINT_LINE(x) Serial.println(x)
	#define DEBUG_PRINT_FORMATTED_LINE(x, ...) ardprintf(x, __VA_ARGS__)
#else
	#define DEBUG_OPEN_SERIAL(x)
	#define DEBUG_PRINT_LINE(x)
	#define DEBUG_PRINT_FORMATTED_LINE(x, ...)
#endif

#define NUM_LEDS 30 // Number of LED controllers (3 LEDS per controller)
#define COLOR_ORDER RGB  // Define LED strip color order
#define BTN_HOURS_PIN 2
#define BTN_MINUTES_PIN 3
#define BTN_BRIGHTNESS_PIN 4
#define BTN_COLOR_PIN 5
#define LED_DATA_PIN 6
#define EEPROM_SETTINGS_ADDR 0
#define LIGHTSENSOR_HIGH_LUX 20000
#define LED_LOWEST_LUMINOSITY 10
#define LED_HIGHEST_LUMINOSITY 255
#define BLACK CHSV(0, 255, 0)
#define FRAMES_PER_SECOND  24
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define INITIAL_SETTINGS { FULL, 0, getColor(RED) }

struct Settings {
	uint8_t luminosityMode;
	uint8_t colorPattern;
	CHSV color;
};

enum LuminosityMode {
	SENSOR_DRIVEN,
	DIMMED,
	MEDIUM,
	BRIGHT,
	FULL,
	OFF,
	SIZE
};

enum ColorPatterns {
	DYNAMIC,
	HOLD,
	RED,
	GREEN,
	BLUE,
	WHITE,
	PATTERNS_SIZE
};

enum Letters { A, F, G, H, I, L, O, T, U, _, LETTERS_SIZE };

Settings settings = INITIAL_SETTINGS;
bool dotIsVisible = true;
tmElements_t previousTime, currentTime;
uint8_t luminosityValues[SIZE];
uint8_t currentLuminosity = 0;
BH1750 lightMeter;

ColorPatterns colorPatterns[] = { DYNAMIC, HOLD, RED, GREEN, BLUE, WHITE };
CRGB leds[NUM_LEDS]; // Define the color array of the LED strip
byte digits[10][7] = {	{0,1,1,1,1,1,1},   // Digit 0
						{0,1,0,0,0,0,1},   // Digit 1
						{1,1,1,0,1,1,0},   // Digit 2
						{1,1,1,0,0,1,1},   // Digit 3
						{1,1,0,1,0,0,1},   // Digit 4
						{1,0,1,1,0,1,1},   // Digit 5
						{1,0,1,1,1,1,1},   // Digit 6
						{0,1,1,0,0,0,1},   // Digit 7
						{1,1,1,1,1,1,1},   // Digit 8
						{1,1,1,1,0,1,1} }; // Digit 9 | 2D Array for numbers on 7 segment
byte letters[LETTERS_SIZE][7] = {{1,1,1,1,1,0,1},   // Digit A
						{1,0,1,1,1,0,0},   // Digit F
						{1,0,1,1,1,1,1},   // Digit G
						{1,1,0,1,1,0,1},   // Digit H
						{0,0,0,1,1,0,0},   // Digit I
						{0,0,0,1,1,1,0},   // Digit L
						{0,1,1,1,1,1,1},   // Digit O
						{1,0,0,1,1,1,0},   // Digit t
						{0,1,0,1,1,1,1},   // Digit U 
						{0,0,0,0,0,0,0} }; // Digit shows nothing | 2D Array for letters on 7 segment

byte luminosityText[SIZE][4] = {{A,U,T,O},
								{L,O,_,_},
								{H,A,L,F},
								{H,I,G,H},
								{F,U,L,L},
								{O,F,F,_}};

uint8_t digitOffset[] = { 23, 16, 7, 0 };

void setup(){
	DEBUG_OPEN_SERIAL(9600);
	Wire.begin();
	lightMeter.begin(BH1750_CONTINUOUS_LOW_RES_MODE);
	createLuminosityDefinition();
	getSettings();
	LEDS.addLeds<WS2811, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	pinMode(BTN_BRIGHTNESS_PIN, INPUT_PULLUP);
	pinMode(BTN_COLOR_PIN, INPUT_PULLUP);
	pinMode(BTN_HOURS_PIN, INPUT_PULLUP);
	pinMode(BTN_MINUTES_PIN, INPUT_PULLUP);
}

CHSV getColor(uint8_t forPattern)
{
	switch (forPattern)
	{
		case DYNAMIC:
			settings.color.hue += 1;
		case HOLD:
			return CHSV(settings.color.hue, 255, 255);
		case RED:
			return CHSV(0, 255, 255);
		case GREEN:
			return CHSV(96, 255, 255);
		case BLUE:
			return CHSV(160, 255, 255);
		case WHITE: 
		default:
			return CHSV(settings.color.hue, 0, 255);
	}
}

void createLuminosityDefinition()
{
	luminosityValues[SENSOR_DRIVEN] = 0;
	luminosityValues[DIMMED] = 64;
	luminosityValues[MEDIUM] = 128;
	luminosityValues[BRIGHT] = 192;
	luminosityValues[FULL] = 255;
	luminosityValues[OFF] = 0;
}  

void changeLuminosityMode()
{
	settings.luminosityMode = (settings.luminosityMode + 1) % SIZE;
	showLuminosityModeText();
	persistSettings();
}

void changeColorMode()
{
	settings.colorPattern = (settings.colorPattern + 1) % ARRAY_SIZE(colorPatterns);
	updateLedDigits();
	persistSettings();
}

void updateLedLuminosity()
{
	uint8_t luminosity;
	settings.luminosityMode == SENSOR_DRIVEN ? luminosity = luminosityValueBySensor() : luminosity = luminosityValues[settings.luminosityMode];
	if (currentLuminosity != luminosity)
	{
		DEBUG_PRINT_FORMATTED_LINE("Luminosity changed %d --> %d", currentLuminosity, luminosity);
		currentLuminosity = luminosity;
		LEDS.setBrightness(currentLuminosity);
	}
}

uint8_t * showLuminosityModeText()
{
	leds[14] = leds[15] = BLACK;
	
	for (int i = 0; i < 4; i++)
	{
		byte letter = luminosityText[settings.luminosityMode][3-i];
		int cursor = digitOffset[i];
		for (int k = 0; k <= 6; k++) {
			if (letters[letter][k] == 1) { leds[cursor] = settings.color; }
			else if (letters[letter][k] == 0) { leds[cursor] = BLACK; };
			cursor++;
		};
	}
	LEDS.setBrightness(LED_HIGHEST_LUMINOSITY);
	FastLED.show();
	delay(1000);
}

void handleButtonInteraction()
{
	int btnHours = digitalRead(BTN_HOURS_PIN);
	int btnMinutes = digitalRead(BTN_MINUTES_PIN);
	int btnColorMode = digitalRead(BTN_COLOR_PIN);
	int btnBrightness = digitalRead(BTN_BRIGHTNESS_PIN);

	if (btnHours == LOW || btnMinutes == LOW || btnColorMode == LOW || btnBrightness == LOW)
	{
		tmElements_t Now;
		RTC.read(Now);
		if (btnHours == LOW) {
			Now.Hour == 23 ? Now.Hour = 0 : Now.Hour++;
			RTC.write(Now);
		}
		else if (btnMinutes == LOW) {
			Now.Minute == 59 ? Now.Minute = 0 : Now.Minute++;
			RTC.write(Now);
		}
		else if (btnColorMode == LOW)
		{
			changeColorMode();
		}
		else if (btnBrightness == LOW)
		{
			changeLuminosityMode();
		};
		delay(400);
	}
}

uint8_t luminosityValueBySensor()
{
	uint16_t lightSensor = lightMeter.readLightLevel();
	lightSensor = min(lightSensor, LIGHTSENSOR_HIGH_LUX);
	return map(lightSensor, LIGHTSENSOR_HIGH_LUX, 0, LED_LOWEST_LUMINOSITY, LED_HIGHEST_LUMINOSITY);
}

void updateLedDigits() 
{
	settings.color = getColor(colorPatterns[settings.colorPattern]);
	int Now = (currentTime.Hour * 100 + currentTime.Minute);
	int cursor = NUM_LEDS;
	dotIsVisible = (currentTime.Second % 2 == 0);
	dotIsVisible ? (leds[14] = leds[15] = settings.color) : (leds[14] = leds[15] = BLACK);

	for (int i = 0; i < 4; i++)
	{
		int digit = Now % 10; // get last digit in time
		int cursor = digitOffset[i];
		for (int k = 0; k <= 6; k++) {
			if (digits[digit][k] == 1) { leds[cursor] = settings.color; }
			else if (digits[digit][k] == 0) { leds[cursor] = BLACK; };
			cursor++;
		};
		Now /= 10;
	}
};

void printTime()
{
	if (abs(currentTime.Second - previousTime.Second) > 0)
	{
		previousTime = currentTime;
		DEBUG_PRINT_FORMATTED_LINE("Time: %d:%d:%d", currentTime.Hour, currentTime.Minute, currentTime.Second);
	}
}

void getSettings()
{
	EEPROM.get(EEPROM_SETTINGS_ADDR, settings);
	if (settings.colorPattern >= ARRAY_SIZE(colorPatterns))
	{
		DEBUG_PRINT_LINE("First run. Set initial settings.");
		settings = INITIAL_SETTINGS;
		persistSettings();
	}
	printSettings();
}

void persistSettings()
{
	EEPROM.put(EEPROM_SETTINGS_ADDR, settings);
	printSettings();
}

void printSettings()
{
	DEBUG_PRINT_FORMATTED_LINE("[Settings] Color Mode: %d HSV: [%d,%d,%d], Brightness Mode: %d", settings.colorPattern, settings.color.hue, settings.color.saturation, settings.color.value, settings.luminosityMode);
}

void loop()
{
	EVERY_N_MILLISECONDS(200)
	{
		updateLedLuminosity();
	}
	handleButtonInteraction();
	RTC.read(currentTime);
	printTime();
	// Update the color array of the LEDs to represent the current time & color pattern
	updateLedDigits();
	FastLED.show();
	FastLED.delay(1000 / FRAMES_PER_SECOND);
}