#include <Arduino.h>
#include "DisplayDriver.h"

DisplayDriver* driver;

void setup() {
    Serial.begin(9600);

    driver = new DisplayDriver();
    driver->setCharacter(0, 'I');
    driver->setCharacter(1, 'K');
}

void loop() {
    driver->send();
    // TODO buffer input and smooth/average
    map(analogRead(A0), 255, 1023, 99, 0);
}
