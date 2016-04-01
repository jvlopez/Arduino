#include <IRLib.h>

#define IR_BTN_HOURS -2122231633
#define IR_BTN_MINUTES -2122207153
#define IR_BTN_BRIGHTNESS -2122198993
#define IR_BTN_COLOR -2122223473
#define IR_BTN_SWITCH_DISPLAYDATA -2122239793
//Create a receiver object to listen on pin 11
#define IR_RECV_PIN 11
IRrecv My_Receiver(IR_RECV_PIN);
boolean knownCommand = false;
char command;
//decode_results irResults;
IRdecode My_Decoder;
void setup()
{
	Serial.begin(9600);
	My_Receiver.enableIRIn(); // Start the receiver
}

void loop() {
//  if (irrecv.decode(&results)) {
//    understood = true;
//    switch(results.value) {
//      case 16752735:
//        command = 'A';
//        break;
//      case 16720095:
//        command = 'B';
//        break;
//      case 16736415:
//        command = 'C';
//        break;
//      case 16769055:
//        command = 'D';
//        break;
//      default:
//        understood = false;
//    }
//    if (understood) Serial.println(command);
//    irrecv.resume(); // Receive the next value
  
	//Continuously look for results. When you have them pass them to the decoder
	if (My_Receiver.GetResults(&My_Decoder)) {
		My_Decoder.decode();    //Decode the data
		My_Decoder.DumpResults(); //Show the results on serial monitor
		Serial.println(My_Decoder.value);
		My_Receiver.resume();     //Restart the receiver
	}
	delay(100);
}
