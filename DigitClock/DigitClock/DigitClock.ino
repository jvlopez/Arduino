#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <EEPROM.h>
#include <BH1750.h>
#include <FastLED.h>
#include <SI7021.h>
#include "ardprintf.h"

#define PRINTF(x, ...) ardprintf(x, __VA_ARGS__)
#define NUM_LEDS 30 // Number of LED controllers (3 LEDS per controller)
#define COLOR_ORDER RGB  // Define LED strip color order
#define PULLUP_PIN_ACTIVE(pin) ~digitalRead(pin) & 1
#define PULLUP_PIN_ACTIVE_ARRAY(pin) (PULLUP_PIN_ACTIVE(pin)) << pin
#define BTN_IS_PRESSED(pin, pinArray) (pinArray >> pin & 1) > 0
#define BTN_HOURS_PIN 2
#define BTN_MINUTES_PIN 3
#define BTN_BRIGHTNESS_PIN 4
#define BTN_COLOR_PIN 5
#define PIN_ACTIVE(x) PULLUP_PIN_ACTIVE_ARRAY(x)
#define PRESSED_BUTTONS_ARRAY (PIN_ACTIVE(BTN_HOURS_PIN) | PIN_ACTIVE(BTN_MINUTES_PIN) | PIN_ACTIVE(BTN_BRIGHTNESS_PIN) | PIN_ACTIVE(BTN_COLOR_PIN))
#define LED_DATA_PIN 6
#define EEPROM_SETTINGS_ADDR 0
#define AUTO_ADJUST 0
#define LIGHTSENSOR_HIGH_LUX 20000
#define MIN_VALID_TEMP -10
#define MAX_VALID_TEMP 60
#define LED_LOWEST_LUMINOSITY 10
#define LED_HIGHEST_LUMINOSITY 255
#define BLACK CHSV(0, 255, 0)
#define FRAMES_PER_SECOND 24
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define INITIAL_SETTINGS { LED_HIGHEST_LUMINOSITY, 0, getColor(RED), true, true }

struct Settings {
	uint8_t luminosity;
	uint8_t colorPattern;
	CHSV color;
	uint8_t showEnvironmentData;
	uint8_t displayIsOn;
};

struct DisplaySymbol {
	byte segments[7];
	char symbol;
};

struct DisplayContent {
	byte symbols[4];
	bool showDots;
};

struct LuminositySetting {
	byte value;
	byte text[4];
};

struct EnvironmentData {
	int celsius;
	int humidity;
};

enum ColorPatterns { DYNAMIC, HOLD, RED, GREEN,	BLUE, WHITE };
enum DisplayContentMode { TIME, TEMPERATURE, REL_HUMIDITY, CONTENTMODE_SIZE };

enum Letters { A = 10, F, G, H, I, L, O, o, T, U, R, C, DEGR, DASH, _ };

DisplaySymbol symbols[] = { { { 0,1,1,1,1,1,1 }, '0'},
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
							{ { 1,0,0,0,1,1,1 }, 'o'},
							{ { 1,0,0,1,1,1,0 }, 't'},
							{ { 0,1,0,1,1,1,1 }, 'U'},
							{ { 1,0,0,0,1,0,0 }, 'r'},
							{ { 0,0,1,1,1,1,0 }, 'C'},
							{ { 1,1,1,1,0,0,0 }, 176},	// ° char
							{ { 1,0,0,0,0,0,0 }, '-'},
							{ { 0,0,0,0,0,0,0 }, ' '} };

LuminositySetting luminositySetting[] =	  { { 0,	{ A,U,T,O } },
											{ 64,	{ L,O,_,_ } }, 
											{ 128,	{ H,A,L,F } }, 
											{ 192,	{ H,I,G,H } }, 
											{ 255,	{ F,U,L,L } } };

ColorPatterns colorPatterns[] = { DYNAMIC, HOLD, RED, GREEN, BLUE, WHITE };
uint8_t digitOffset[] = { 0, 7, 16, 23 };
CRGB leds[NUM_LEDS]; // Define the color array of the LED strip
DisplayContent content = { { _, _, _, _ }, 1 };
Settings settings = INITIAL_SETTINGS;
tmElements_t currentTime;
uint8_t currentLuminosity;
uint8_t currentDisplayContentMode;
EnvironmentData environmentData;
BH1750 lightMeter;
SI7021 environmentSensor;
char receivedIrCommand = '?';
bool displayIsOn = false;

void setup(){
	Wire.begin();
	lightMeter.begin(BH1750_CONTINUOUS_LOW_RES_MODE);
	environmentSensor.begin();
	getTempAndHumidity();
	LEDS.addLeds<WS2811, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	pinMode(BTN_BRIGHTNESS_PIN, INPUT_PULLUP);
	pinMode(BTN_COLOR_PIN, INPUT_PULLUP);
	pinMode(BTN_HOURS_PIN, INPUT_PULLUP);
	pinMode(BTN_MINUTES_PIN, INPUT_PULLUP);
	Serial.begin(9600);
	getSettings();
	PRINTF("Initialization completed! Nr. of LEDS configured: %d", NUM_LEDS);
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

void getTempAndHumidity()
{
	si7021_thc result = environmentSensor.getTempAndRH();
	environmentData.celsius = result.celsiusHundredths / 100;
	environmentData.humidity = result.humidityPercent;
	PRINTF("Temp (celsius): %d, Rel. Humidity (percent): %d", environmentData.celsius, environmentData.humidity);
}

void showTemperature()
{
	struct DisplayContent content;
	if (environmentData.celsius <= MIN_VALID_TEMP || environmentData.celsius > MAX_VALID_TEMP)
	{
		content.symbols[0] = DASH;
		content.symbols[1] = DASH;
	}
	else {
		content.symbols[0] = environmentData.celsius < 0 ? DASH : (environmentData.celsius / 10) % 10;
		content.symbols[1] = abs(environmentData.celsius % 10);
	}

	content.symbols[2] = DEGR;
	content.symbols[3] = C;
	content.showDots = false;
	updateDisplayContent(content);
}

void showHumidity()
{
	struct DisplayContent content;
	content.symbols[0] = (environmentData.humidity / 10) % 10;
	content.symbols[1] = environmentData.humidity % 10;
	content.symbols[2] = DEGR;
	content.symbols[3] = o;
	content.showDots = false;
	updateDisplayContent(content);
}

void changeLuminosityMode()
{
	if (settings.displayIsOn > 0)
	{
		settings.luminosity = (settings.luminosity + 1) % ARRAY_SIZE(luminositySetting);
	}
	else {
		settings.displayIsOn = 1;
	}
	LEDS.setBrightness(LED_HIGHEST_LUMINOSITY);
	struct DisplayContent content = { {}, false };
	memcpy(content.symbols, luminositySetting[settings.luminosity].text, sizeof(content.symbols));
	updateDisplayContent(content);
	delay(600);
}

void powerOff()
{
	if (settings.displayIsOn > 0)
	{
		settings.displayIsOn = 0;
		LEDS.setBrightness(LED_HIGHEST_LUMINOSITY);
		updateDisplayContent({ { O,F,F,_ }, false });
		delay(600);
	}
}

void adjustDisplayLuminosity()
{
	if (!settings.displayIsOn)
	{
		LEDS.setBrightness(0);
		return;
	}
	uint8_t luminosity;
	settings.luminosity == AUTO_ADJUST ? luminosity = luminosityValueBySensor() : luminosity = luminositySetting[settings.luminosity].value;
	if (currentLuminosity != luminosity)
	{
		PRINTF("Luminosity changed %d --> %d", currentLuminosity, luminosity);
		currentLuminosity = luminosity;
		LEDS.setBrightness(currentLuminosity);
	}
}

bool irCommandReceived() {
	if (Serial.available() > 0) {
		receivedIrCommand = Serial.read();
		PRINTF("IR Command received: HEX %c DEC %d", receivedIrCommand, receivedIrCommand);
		return true;
	}
	receivedIrCommand = '?';
	return false;
}

void handleButtonInteraction()
{
	if(PRESSED_BUTTONS_ARRAY || irCommandReceived())
	{
		delay(100);	// Wait for more buttons
		byte buttonsPressed = PRESSED_BUTTONS_ARRAY; // Fix the pressed buttons array
		if ((BTN_IS_PRESSED(BTN_HOURS_PIN, buttonsPressed) && BTN_IS_PRESSED(BTN_MINUTES_PIN, buttonsPressed) || receivedIrCommand == 'D'))
		{
			settings.showEnvironmentData = settings.showEnvironmentData ? 0 : 1;
			currentDisplayContentMode = settings.showEnvironmentData ? TEMPERATURE : TIME;
		}
		else if ((BTN_IS_PRESSED(BTN_BRIGHTNESS_PIN, buttonsPressed) && BTN_IS_PRESSED(BTN_COLOR_PIN, buttonsPressed) || receivedIrCommand == '0'))
		{
			powerOff();
		}
		else if (BTN_IS_PRESSED(BTN_HOURS_PIN, buttonsPressed) || receivedIrCommand == 'H')
		{
			currentTime.Hour = (++currentTime.Hour % 24);
			RTC.write(currentTime);
		}
		else if (BTN_IS_PRESSED(BTN_MINUTES_PIN, buttonsPressed) || receivedIrCommand == 'M')
		{
			currentTime.Minute = (++currentTime.Minute % 60);
			RTC.write(currentTime);
		}
		else if (BTN_IS_PRESSED(BTN_BRIGHTNESS_PIN, buttonsPressed) || receivedIrCommand == 'B' || receivedIrCommand == '1')
		{
			changeLuminosityMode();
		}
		else if (BTN_IS_PRESSED(BTN_COLOR_PIN, buttonsPressed) || receivedIrCommand == 'C')
		{
			settings.colorPattern = (settings.colorPattern + 1) % ARRAY_SIZE(colorPatterns);
		}
		
		persistSettings();
		delay(300);
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
	int Now = (currentTime.Hour * 100 + currentTime.Minute);
	struct DisplayContent content;
	content.symbols[3] = Now % 10;
	content.symbols[2] = (Now /= 10) % 10;
	content.symbols[1] = (Now /= 10) % 10;
	content.symbols[0] = (Now /= 10) % 10;
	content.showDots = (currentTime.Second % 2 == 0);
	updateDisplayContent(content);
};

void whatToShow(uint8_t contentMode)
{
	switch (contentMode)
	{
		case TEMPERATURE:
			showTemperature();
			break;
		case REL_HUMIDITY:
			showHumidity();
			break;
		case TIME:
		default:
			showTime();
	}
}

void updateDisplayContent(struct DisplayContent newContent)
{
	int cursor = NUM_LEDS;
	settings.color = getColor(colorPatterns[settings.colorPattern]);
	leds[14] = leds[15] = newContent.showDots ? settings.color : BLACK;
	
	for (int i = 0; i < 4; i++)
	{
		int cursor = digitOffset[i];
		for (int k = 0; k <= 6; k++) {
			bool segmentActive = (symbols[newContent.symbols[i]].segments[k] == 1);
			leds[cursor] = segmentActive ? settings.color : BLACK;
			cursor++;
		};
	}
	FastLED.show();

	bool contentUpdated = (content.showDots != newContent.showDots) | memcmp(content.symbols, newContent.symbols, sizeof(content.symbols));
	if (contentUpdated) {
		content = newContent;
		PRINTF("[Display Changed] %c%c%c%c%c", symbols[content.symbols[0]].symbol, symbols[content.symbols[1]].symbol,
			content.showDots ? ':' : ' ', symbols[content.symbols[2]].symbol, symbols[content.symbols[3]].symbol);
	}
}

void getSettings()
{
	EEPROM.get(EEPROM_SETTINGS_ADDR, settings);
	if (settings.colorPattern >= ARRAY_SIZE(colorPatterns))
	{
		PRINTF("No (or corrupt) settings found (EEPROM addr %d). Storing initial settings.", EEPROM_SETTINGS_ADDR);
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
	PRINTF("[Settings] Color Mode: %d HSV: [%d,%d,%d], Light are On: %d, Brightness Mode: %d, Show Environment Data: %d", 
		settings.colorPattern, settings.color.hue, settings.color.saturation, settings.color.value, settings.displayIsOn, settings.luminosity, settings.showEnvironmentData);
}

void loop()
{
	EVERY_N_MILLISECONDS(5000)
	{
		if (settings.showEnvironmentData)
		{
			getTempAndHumidity();
			currentDisplayContentMode = (currentDisplayContentMode + 1) % CONTENTMODE_SIZE;
		}
		else {
			currentDisplayContentMode = TIME;
		}
	}

	EVERY_N_MILLISECONDS(200)
	{
		adjustDisplayLuminosity();
	}

	handleButtonInteraction();
	RTC.read(currentTime);
	whatToShow(currentDisplayContentMode);
	FastLED.delay(1000 / FRAMES_PER_SECOND);
}