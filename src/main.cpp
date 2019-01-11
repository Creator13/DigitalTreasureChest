#include "Arduino.h"
#include "HDSPA22C_Driver.h"

// Display driver reference
HDSPA22C_Driver* driver;

// ## Pin configuration ## //
const int pressurePin = A0;     // Pressure sensor
const int buttonPin = 8;        // Action button
const int powerLed = 7;         // Power led (always on when active == true)
const int activityLed = 5;      // Multipurpose information led

// ## Program configuration ## //
uint16_t startupDelay = 3;          // Time 'HI' is shown on the display in seconds
int awakeTime = 30;                 // Time in seconds the device will stay awake

// ## Buffer for pressure sensor smoothing ## //
const int numReadings = 100;        // Size of buffer (WARNING: increasing this drastically increases program size)
int readings[numReadings];          // Array to store readings
int readIndex = 0;                  // Index pointer
int total = 0;                      // Sum of buffer values
int average = 0;                    // Average of buffer values
uint32_t lastReading = millis();    // Timestamp for each reading
uint32_t readTimeout = 12;          // Timeout between each reading (lower numbers increase responsiveness)

// ## State variables ## //
bool active = false;
unsigned long wakeStamp = millis(); // Timestamp of the last time the device was woken up.

// # Button press state variables
bool buttonPressed = false;
unsigned long pressTimeout = 10; //timeout for new buttonpress in ms
unsigned long pressTimeoutStamp = millis();
unsigned long releaseTimeoutStamp = millis();

// ## Function declarations ## //
int readPressure();
void detectButtonPress(void (*callback)());
void keepAwake();
void wake();
void sleep();

void setup() {
    Serial.begin(9600);

    driver = new HDSPA22C_Driver();
    driver->clear();

    pinMode(buttonPin, INPUT);
    pinMode(powerLed, OUTPUT);
    pinMode(activityLed, OUTPUT);

    // Turn off the builtin led
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 0);
}

void loop() {
    if (!active) {
        void (*_wake)() = &wake;
        detectButtonPress(_wake);
        return;
    }

    driver->send();

    if (millis() > lastReading + readTimeout) {
        // Read new value
        if (readPressure() > 0) {
            keepAwake();
        }
        // Update reading timestamp
        lastReading = millis();
    }

    driver->setCharacter(0, average / 10 + '0');
    driver->setCharacter(1, average % 10 + '0');

    if (active) {
        if (millis() > wakeStamp + 1000 * awakeTime) {
            sleep();
        }
    }
}

void keepAwake() {
    wakeStamp = millis();
}

void wake() {
    digitalWrite(powerLed, 1);

    driver->setCharacter(0, 'H');
    driver->setCharacter(1, 'I');

    wakeStamp = millis();

    while (millis() < wakeStamp + 1000 * startupDelay) {
        driver->send();
    }

    active = true;
    wakeStamp = millis();
}

void sleep() {
    digitalWrite(powerLed, 0);

    active = false;
    driver->clear();

    // Reset array of readings to preserve data integrity (fancy words for 'restarting the device')
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }
}

int readPressure() {
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

    return pressure;
}

// Button press with dampening and single-click implementation
void detectButtonPress(void (*callback)()) {
    int buttonState = digitalRead(buttonPin);
    if (buttonPressed == 0 && buttonState == 1 && millis() > releaseTimeoutStamp + pressTimeout) {
        buttonPressed = 1;
        pressTimeoutStamp = millis();

        (*callback)();
    }

    if (buttonPressed == 1 && buttonState == 0 && millis() > pressTimeoutStamp + pressTimeout) {
        buttonPressed = 0;
        releaseTimeoutStamp = millis();
    }
}
