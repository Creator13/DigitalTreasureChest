#include <Arduino.h>
#include "HDSPA22C_Driver.h"

HDSPA22C_Driver* driver;

const int numReadings = 100;
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;
uint32_t lastReading = millis();
uint32_t readTimeout = 10;

void readPressure() {
    int pressure = map(analogRead(A0), 275, 1023, 99, 0);

    if (pressure > 99) {
        pressure = 99;
    }
    else if (pressure < 0) {
        pressure = 0;
    }

    // subtract the last reading:
    total = total - readings[readIndex];
    // read from the sensor:
    readings[readIndex] = pressure;
    // add the reading to the total:
    total = total + readings[readIndex];
    // advance to the next position in the array:
    readIndex = readIndex + 1;

    // if we're at the end of the array...
    if (readIndex >= numReadings) {
        // ...wrap around to the beginning:
        readIndex = 0;
    }

    // calculate the average:
    average = total / numReadings;
}

void setup() {
    Serial.begin(9600);

    driver = new HDSPA22C_Driver();
}

void loop() {
    driver->send();

    if (millis() > lastReading + readTimeout) {
        readPressure();
        lastReading = millis();
    }

    driver->setCharacter(0, average / 10 + '0');
    driver->setCharacter(1, average % 10 + '0');
}
