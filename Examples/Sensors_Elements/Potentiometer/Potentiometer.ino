int potPin = 2;    // select the input pin for the potentiometer
int potiValue = 13;   // select the pin for the LED

void setup() {
  Serial.begin(9600);
}

void loop() {
  potiValue = analogRead(potPin);    // read the value from the sensor
  digitalWrite(potiValue, HIGH);  // turn the ledPin on
  Serial.print("Poti: ");
  Serial.println(potiValue);
  delay(250);                  // stop the program for some time
}
