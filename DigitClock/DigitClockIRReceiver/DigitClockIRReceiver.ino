/*
* One single arduino can’t reliably receive commands from an IR module and 
* control a demanding animation on an LED strand at the same time.
* The trouble is that both operations are highly time dependent. 
* In simple terms, the IR module needs to be able to interrupt execution the moment 
* it receives a command or the command will be lost or garbled.
* here is how it works: The arduino listening to the IR receiver gets a command, 
* validates it, and then sends an appropriate command out to the LED arduino.
* http://industriumvita.com/arduino-ir-remote-controled-ws2811-addressable-leds/
*/
#include <IRLib.h>

// DVD Player remote buttons
#define IR_CMD_LIGHTS_POWER_ON 1223			// Power
#define IR_CMD_LIGHTS_POWER_OFF 66759		// Power
#define IR_CMD_BRIGHTNESS 1039				// Display
#define IR_CMD_BRIGHTNESS_ALT 66575			// Display
#define IR_CMD_COLOR 1233					// Menu
#define IR_CMD_COLOR_ALT 66769				// Menu
#define IR_CMD_INCREASE_SPEED 1112			// Up
#define IR_CMD_INCREASE_SPEED_ALT 66648		// Up
#define IR_CMD_DECREASE_SPEED 1113			// Down
#define IR_CMD_DECREASE_SPEED_ALT 66649		// Down
#define IR_CMD_SWITCH_DISPLAYDATA 1155		// Title
#define IR_CMD_SWITCH_DISPLAYDATA_ALT 66691	// Title
#define IR_CMD_HOURS 1114					// Left
#define IR_CMD_HOURS_ALT 66650				// Left
#define IR_CMD_MINUTES 1115					// Down
#define IR_CMD_MINUTES_ALT 66651			// Down

#define IR_RECV_PIN 11 // Create a receiver object to listen on pin 11
IRrecv irReceiver(IR_RECV_PIN);
boolean knownCommand = false;
char command;
IRdecode irDecoder;
void setup()
{
	Serial.begin(9600);
	irReceiver.enableIRIn(); // Start the receiver
}

void loop() {
	if (irReceiver.GetResults(&irDecoder)) {
		irDecoder.decode();
		// Serial.println(irDecoder.value);		// For analysis: Shows the received IR value.
		knownCommand = true;
		switch (irDecoder.value) {
		case IR_CMD_LIGHTS_POWER_ON:
			command = '1';
			break;
		case IR_CMD_LIGHTS_POWER_OFF:
			command = '0';
			break;
		case IR_CMD_BRIGHTNESS:
		case IR_CMD_BRIGHTNESS_ALT:
			command = 'B';
			break;
		case IR_CMD_COLOR:
		case IR_CMD_COLOR_ALT:
			command = 'C';
			break;
		case IR_CMD_INCREASE_SPEED:
		case IR_CMD_INCREASE_SPEED_ALT:
			command = '+';
			break;
		case IR_CMD_DECREASE_SPEED:
		case IR_CMD_DECREASE_SPEED_ALT:
			command = '-';
			break;
		case IR_CMD_SWITCH_DISPLAYDATA:
		case IR_CMD_SWITCH_DISPLAYDATA_ALT:
			command = 'D';
			break;
		case IR_CMD_HOURS:
		case IR_CMD_HOURS_ALT:
			command = 'H';
			break;
		case IR_CMD_MINUTES:
		case IR_CMD_MINUTES_ALT:
			command = 'M';
			break;
		default:
			knownCommand = false;
		}
		if (knownCommand)
		{
			Serial.print(command);
		}

		delay(300);
		irReceiver.resume(); // Receive the next value
	}
}