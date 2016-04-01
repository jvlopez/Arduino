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

#define IR_CMD_HOURS 2172739743
#define IR_CMD_MINUTES 2172772383
#define IR_CMD_BRIGHTNESS 2172768303
#define IR_CMD_COLOR 2172743823
#define IR_CMD_SWITCH_DISPLAYDATA 2172735663

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
		knownCommand = true;
		switch (irDecoder.value) {
		case IR_CMD_HOURS:
			command = 'H';
			break;
		case IR_CMD_MINUTES:
			command = 'M';
			break;
		case IR_CMD_BRIGHTNESS:
			command = 'B';
			break;
		case IR_CMD_COLOR:
			command = 'C';
			break;
		case IR_CMD_SWITCH_DISPLAYDATA:
			command = 'D';
			break;
		default:
			knownCommand = false;
		}

		if (knownCommand)
		{
			Serial.println(command);
		}

		delay(300);
		irReceiver.resume(); // Receive the next value
	}
}