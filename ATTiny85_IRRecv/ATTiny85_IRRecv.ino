/*
  IR remote control (Sony) detection for Arduino, M. Burnette
  Binary sketch size: 2,794 bytes (of a 8,192 byte maximum)

    20130103 MRB Modified for interface to Mega2560
      Europa codebase for menu system
      
    20121230 MRB Modified for Tiny85 Google Tiny library
      Tiny85 Internal RC 16MHz

    20121230 MRB modifications to adapt to numeric input, avoid dupes,
      and to generally "behave" consistently
    Used with Electronic Goldmine IR detector MIM 5383H4
      http://www.goldmine-elec-products.com/prodinfo.asp?number=G16737
    IR detector:
      Pin 1: To pin D4 on Arduino ATtiny85
      Pin 2: GND
      Pin 3: 5V through 33 Ohm resistor
  
    This is based on pmalmsten's code found on the Arduino forum from 2007:
      http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1176098434/0

*/
// Pins 2/3 used for Software serial
int irPin     = 4;       //Sensor pin 1 wired to Arduino's pin D4
int statLED   = 0;       //Toggle the status LED every time Power is pressed
int start_bit = 2200;    //Start bit threshold (Microseconds)
int bin_1     = 1000;    //Binary 1 threshold (Microseconds)
int bin_0     = 400;     //Binary 0 threshold (Microseconds)

void setup() {
  pinMode(statLED, OUTPUT);
  digitalWrite(statLED, LOW);

  pinMode(irPin, INPUT);

  Serial.begin(9600);
  Serial.println("IR/Serial Initialized: ");
}

void loop() {
  int key = getIRKey();   //Fetch the key
  
  if(key != 0)            //Ignore keys that are zero
  {
    switch(key)
    {
      case 128: Serial.print("1"); break;
      case 129: Serial.print("2"); break;
      case 130: Serial.print("3"); break;
      case 131: Serial.print("4"); break;
      case 132: Serial.print("5"); break;
      case 133: Serial.print("6"); break;
      case 134: Serial.print("7"); break;
      case 135: Serial.print("8"); break;
      case 136: Serial.print("9"); break;
      case 137: Serial.print("0"); break;
      
      case 144: Serial.print("A"); break;  // CH Up
      case 145: Serial.print("B"); break;  // CH Down
      case 146: Serial.print("C"); break;  // VOL Right
      case 147: Serial.print("D"); break;  // VOL Left
      case 148: Serial.print("E"); break;  // Mute
      case 165: Serial.print("F"); break;  // AV/TV
      case 149: Serial.print("P");         // Power == MENU ACTIVE
        //This toggles the statLED every time power button is hit
        if(digitalRead(statLED) != 1)
          digitalWrite(statLED, HIGH);
        else
          digitalWrite(statLED, LOW);
        break;

      //default: Serial.println(key); // for inspection of keycode
    }

    delay(400);    // avoid double key logging (adjustable)
  }
}

int getIRKey() {
  int data[12];
  int i;

  while(pulseIn(irPin, LOW) < start_bit); //Wait for a start bit
  
  for(i = 0 ; i < 11 ; i++)
    data[i] = pulseIn(irPin, LOW);      //Start measuring bits, I only want low pulses
  
  for(i = 0 ; i < 11 ; i++)             //Parse them
  {      
    if(data[i] > bin_1)                 //is it a 1?
      data[i] = 1;
    else if(data[i] > bin_0)            //is it a 0?
      data[i] = 0;
    else
      return -1;                        //Flag the data as invalid; I don't know what it is! Return -1 on invalid data
  }

  int result = 0;
  for(i = 0 ; i < 11 ; i++)             //Convert data bits to integer
    if(data[i] == 1) result |= (1<<i);

  return result;                        //Return key number
} 
