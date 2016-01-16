#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <EEPROM.h>
#include <BH1750.h>
#include <FastLED.h>
#include "ardprintf.h"

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
#define DATA_PIN 6  // Data pin for led comunication
#define EEPROM_SETTINGS_ADDR 0
#define BRIGHTNESS_MAX_LUX 20000
#define BRIGHTNESS_THRESHOLD 10
#define BRIGHTNESS_OFF 0
#define BRIGHTNESS_LOWEST 10
#define BRIGHTNESS_DIMMED 64
#define BRIGHTNESS_MEDIUM 128
#define BRIGHTNESS_BRIGHT 192
#define BRIGHTNESS_FULL 255
#define BLACK CHSV(0, 255, 0)
#define FRAMES_PER_SECOND  24
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define INITIAL_SETTINGS { FULL, 0, CHSV(0, 255, 255)}

CRGB leds[NUM_LEDS]; // Define the array of the LEDs strip
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
uint8_t digitOffset[4] = { 23, 16, 7, 0 };

struct Settings {
	uint8_t brightnessMode;
	uint8_t colorPattern;
	CHSV color;
};

enum BrightnessMode {
	SENSOR_DRIVEN,
	DIMMED,
	MEDIUM,
	BRIGHT,
	FULL,
	OFF,
	SIZE
};

Settings settings = INITIAL_SETTINGS;
bool dotIsVisible = true;
int timeChanged = 0;
uint8_t brightness = 0;
CHSV color = settings.color;
BH1750 lightMeter;

// List of patterns to cycle through. Each is defined as a separate function below.
typedef void(*SimplePatternList[])();
SimplePatternList colorPatternList = { colorWheel, keepColor, red, green, blue, white };

void setup(){
	DEBUG_OPEN_SERIAL(9600);
	Wire.begin();
	lightMeter.begin(BH1750_CONTINUOUS_LOW_RES_MODE);
	getOrInitializeSettings();
	color = settings.color;
	setBrightness(BrightnessModeToValue(settings.brightnessMode));
	LEDS.addLeds<WS2811, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);; // Set LED strip type
	pinMode(BTN_BRIGHTNESS_PIN, INPUT_PULLUP);
	pinMode(BTN_COLOR_PIN, INPUT_PULLUP);
	pinMode(BTN_HOURS_PIN, INPUT_PULLUP);
	pinMode(BTN_MINUTES_PIN, INPUT_PULLUP);
}

void changeBrightnessMode()
{
	settings.brightnessMode = (settings.brightnessMode + 1) % SIZE;
	persistSettings();
	setBrightness(BrightnessModeToValue(settings.brightnessMode));
}

void changeColorMode()
{
	settings.colorPattern = (settings.colorPattern + 1) % ARRAY_SIZE(colorPatternList);
	colorPatternList[settings.colorPattern]();
	settings.color = color;
	persistSettings();
}

uint8_t BrightnessModeToValue(uint8_t brightnessMode)
{
	switch (brightnessMode)
	{
		case SENSOR_DRIVEN:
			return BrightnessValueBySensor();
		case DIMMED:
			return BRIGHTNESS_DIMMED;
		case MEDIUM:
			return BRIGHTNESS_MEDIUM;
		case BRIGHT:
			return BRIGHTNESS_BRIGHT;
		case OFF:
			return BRIGHTNESS_OFF;
		default:
			return BRIGHTNESS_FULL;
	}
}

void AdjustBrightnessToLightIntensity()
{
	uint8_t sensorValue = BrightnessValueBySensor();
	if (brightness != sensorValue)
	{
		DEBUG_PRINT_FORMATTED_LINE("Brightness changed %d --> %d", brightness, sensorValue);
		brightness = sensorValue;
		setBrightness(brightness);
	}
}

void HandleButtons()
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
			changeBrightnessMode();
		};

		delay(400);
	}
}

void keepColor() { }

void colorWheel()
{
	color = CHSV(++color.hue, 255, 255);
}

void red()
{
	color = CHSV(0, 255, 255);
}

void green()
{
	color = CHSV(96, 255, 255);
}

void blue()
{
	color = CHSV(160, 255, 255);
}
void white()
{
	color = CHSV(color.hue, 0, 255);
}


int GetTime() {
	tmElements_t Now;
	RTC.read(Now);
	printTime(Now);
	dotIsVisible = (Now.Second % 2 == 0);
	return (Now.Hour * 100 + Now.Minute);
};

uint8_t BrightnessValueBySensor()
{
	uint16_t lightSensor = lightMeter.readLightLevel();
	if (lightSensor > BRIGHTNESS_MAX_LUX)
	{
		lightSensor = BRIGHTNESS_MAX_LUX;
	}
	return map(lightSensor, BRIGHTNESS_MAX_LUX, 0, BRIGHTNESS_LOWEST, BRIGHTNESS_FULL);
}

void setBrightness(uint8_t value)
{
	LEDS.setBrightness(value);
}

void TimeToLEDArray() {
	int Now = GetTime();
	int cursor = NUM_LEDS;

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
		DEBUG_PRINT_FORMATTED_LINE("Time: %d:%d:%d", time.Hour, time.Minute, time.Second);
	}
}

void getOrInitializeSettings()
{
	EEPROM.get(EEPROM_SETTINGS_ADDR, settings);
	if (settings.colorPattern >= ARRAY_SIZE(colorPatternList))
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
	DEBUG_PRINT_FORMATTED_LINE("[Settings] Color Mode: %d HSV: [%d,%d,%d], Brightness Mode: %d", settings.colorPattern, settings.color.hue, settings.color.saturation, settings.color.value, settings.brightnessMode);
}

void loop()
{
	EVERY_N_MILLISECONDS(200)
	{
		if (settings.brightnessMode == SENSOR_DRIVEN)
		{
			AdjustBrightnessToLightIntensity();
		}
	}
	
	HandleButtons();
	// Call the current pattern function once, updating the 'leds' array
	colorPatternList[settings.colorPattern]();
	TimeToLEDArray();
	// send the 'leds' array out to the actual LED strip
	FastLED.show();
	// insert a delay to keep the framerate modest
	FastLED.delay(1000 / FRAMES_PER_SECOND);
}