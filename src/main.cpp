#include "Arduino.h"
#include "HDSPA22C_Driver.h"

HDSPA22C_Driver* driver;

// ## Pin configuration ## //
const int pressurePin = A0;     //
const int buttonPin = 8;
const int powerLed = 7;
const int activityLed = 5;

// ## Buffer for pressure sensor smoothing ## //
const int numReadings = 100;        // Size of buffer (WARNING: increasing this drastically increases program size)
int readings[numReadings];          // Array to store readings
int readIndex = 0;                  // Index pointer
int total = 0;                      // Sum of buffer values
int average = 0;                    // Average of buffer values
uint32_t lastReading = millis();    // Timestamp for each reading
uint32_t readTimeout = 12;          // Timeout between each reading (lower numbers increase responsiveness)

void readPressure() {
    int pressure = map(analogRead(pressurePin), 275, 1023, 99, 0);

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
        // Read new value
        readPressure();
        // Update reading timestamp
        lastReading = millis();
    }

    driver->setCharacter(0, average / 10 + '0');
    driver->setCharacter(1, average % 10 + '0');
}
