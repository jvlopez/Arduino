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
#define BTN_ADD_HOUR_PRESSED(x) (x >> 3 & 1) > 0
#define BTN_ADD_MINUTE_PRESSED(x) (x >> 2 & 1) > 0
#define BTN_CHANGE_LUMINOSITY_PRESSED(x) (x >> 1 & 1) > 0
#define BTN_CHANGE_COLOR_PRESSED(x) (x & 1) > 0
#define BTN_HOURS_PIN 2
#define BTN_MINUTES_PIN 3
#define BTN_BRIGHTNESS_PIN 4
#define BTN_COLOR_PIN 5
#define LED_DATA_PIN 6
#define EEPROM_SETTINGS_ADDR 0
#define AUTO_ADJUST 0
#define LIGHTSENSOR_HIGH_LUX 20000
#define LED_LOWEST_LUMINOSITY 10
#define LED_HIGHEST_LUMINOSITY 255
#define BLACK CHSV(0, 255, 0)
#define FRAMES_PER_SECOND  24
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define INITIAL_SETTINGS { LED_HIGHEST_LUMINOSITY, 0, getColor(RED) }

struct Settings {
	uint8_t luminosityMode;
	uint8_t colorPattern;
	CHSV color;
};

struct SymbolSegments {
	byte segments[7];
	char representation;
};

struct LuminositySettings {
	byte luminosityValue;
	byte representation[4];
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

enum Letters { A = 10, F, G, H, I, L, O, T, U, _ };

SymbolSegments symbols[] = { { { 0,1,1,1,1,1,1 }, '0'},
							 { { 0,1,0,0,0,0,1 }, '1'},
							 { { 1,1,1,0,1,1,0 }, '2'},
							 { { 1,1,1,0,0,1,1 }, '3'},
							 { { 1,1,0,1,0,0,1 }, '4'},
							 { { 1,0,1,1,0,1,1 }, '5'},
							 { { 1,0,1,1,1,1,1 }, '6'},
							 { { 0,1,1,0,0,0,1 }, '7'},
							 { { 1,1,1,1,1,1,1 }, '8'},
							 { { 1,1,1,1,0,1,1 }, '9'},
							 { { 1,1,1,1,1,0,1 }, 'A'},
							 { { 1,0,1,1,1,0,0 }, 'F'},
							 { { 1,0,1,1,1,1,1 }, 'G'},
							 { { 1,1,0,1,1,0,1 }, 'H'},
							 { { 0,0,0,1,1,0,0 }, 'I'},
							 { { 0,0,0,1,1,1,0 }, 'L'},
							 { { 0,1,1,1,1,1,1 }, 'O'},
							 { { 1,0,0,1,1,1,0 }, 't'},
							 { { 0,1,0,1,1,1,1 }, 'U'},
							 { { 0,0,0,0,0,0,0 }, ' '} };

LuminositySettings luminositySetting[] =  { { 0,	{ A,U,T,O } },
											{ 64,	{ L,O,_,_ } }, 
											{ 128,	{ H,A,L,F } }, 
											{ 192,	{ H,I,G,H } }, 
											{ 255,	{ F,U,L,L } },
											{ 0,	{ O,F,F,_ } } };

ColorPatterns colorPatterns[] = { DYNAMIC, HOLD, RED, GREEN, BLUE, WHITE };
uint8_t digitOffset[] = { 0, 7, 16, 23 };
CRGB leds[NUM_LEDS]; // Define the color array of the LED strip
Settings settings = INITIAL_SETTINGS;
tmElements_t previousTime, currentTime;
uint8_t currentLuminosity = 0;
BH1750 lightMeter;

void setup(){
	DEBUG_OPEN_SERIAL(9600);
	Wire.begin();
	lightMeter.begin(BH1750_CONTINUOUS_LOW_RES_MODE);
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

void changeLuminosityMode()
{
	settings.luminosityMode = (settings.luminosityMode + 1) % ARRAY_SIZE(luminositySetting);
	LEDS.setBrightness(LED_HIGHEST_LUMINOSITY);
	updateLedDigits(luminositySetting[settings.luminosityMode].representation, false);
	delay(1000);
}

void updateLedLuminosity()
{
	uint8_t luminosity;
	settings.luminosityMode == AUTO_ADJUST ? luminosity = luminosityValueBySensor() : luminosity = luminositySetting[settings.luminosityMode].luminosityValue;
	if (currentLuminosity != luminosity)
	{
		DEBUG_PRINT_FORMATTED_LINE("Luminosity changed %d --> %d", currentLuminosity, luminosity);
		currentLuminosity = luminosity;
		LEDS.setBrightness(currentLuminosity);
	}
}

void handleButtonInteraction()
{
	byte buttonsPressed = (~digitalRead(BTN_HOURS_PIN) << 3 | ~digitalRead(BTN_MINUTES_PIN) << 2 | ~digitalRead(BTN_BRIGHTNESS_PIN) << 1 | ~digitalRead(BTN_COLOR_PIN));
	if(buttonsPressed > 0)
	{
		if (BTN_ADD_HOUR_PRESSED(buttonsPressed))
		{
			currentTime.Hour = (++currentTime.Hour % 24);
			RTC.write(currentTime);
		}
		else if (BTN_ADD_MINUTE_PRESSED(buttonsPressed))
		{
			currentTime.Minute = (++currentTime.Minute % 60);
			RTC.write(currentTime);
		}
		else if (BTN_CHANGE_LUMINOSITY_PRESSED(buttonsPressed))
		{
			changeLuminosityMode();
		}
		else if (BTN_CHANGE_COLOR_PRESSED(buttonsPressed))
		{
			settings.colorPattern = (settings.colorPattern + 1) % ARRAY_SIZE(colorPatterns);
		}
		persistSettings();
		delay(400);
	}
}

uint8_t luminosityValueBySensor()
{
	uint16_t lightSensor = lightMeter.readLightLevel();
	lightSensor = min(lightSensor, LIGHTSENSOR_HIGH_LUX);
	return map(lightSensor, LIGHTSENSOR_HIGH_LUX, 0, LED_LOWEST_LUMINOSITY, LED_HIGHEST_LUMINOSITY);
}

void showTime() 
{
	settings.color = getColor(colorPatterns[settings.colorPattern]);
	int Now = (currentTime.Hour * 100 + currentTime.Minute);
	byte timeDigits[4];
	timeDigits[3] = Now % 10;
	timeDigits[2] = (Now /= 10) % 10;
	timeDigits[1] = (Now /= 10) % 10;
	timeDigits[0] = (Now /= 10) % 10;
	updateLedDigits(timeDigits, (currentTime.Second % 2 == 0));
};

void updateLedDigits(byte symbolsToShow[4], bool showDots)
{
	int cursor = NUM_LEDS;
	leds[14] = leds[15] = showDots ? settings.color : BLACK;

	for (int i = 0; i < 4; i++)
	{
		int cursor = digitOffset[i];
		for (int k = 0; k <= 6; k++) {
			bool segmentActive = (symbols[symbolsToShow[i]].segments[k] == 1);
			leds[cursor] = segmentActive ? settings.color : BLACK;
			cursor++;
		};
	}
	FastLED.show();
}

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
	showTime();
	FastLED.delay(1000 / FRAMES_PER_SECOND);
}