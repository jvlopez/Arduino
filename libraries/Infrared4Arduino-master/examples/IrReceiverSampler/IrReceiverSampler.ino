#include <Arduino.h>
#include <IrReceiverSampler.h>

IrReceiver *receiver;

void setup() {
    Serial.begin(115200);
    receiver = IrReceiverSampler::newIrReceiverSampler(200, 5);
}

void loop() {
    receiver->enable();
    while (!receiver->isReady())
        ;
    receiver->disable();
    if (receiver->getDataLength() == 0)
        Serial.println("timeout");
    else
        receiver->dump(Serial);
}
