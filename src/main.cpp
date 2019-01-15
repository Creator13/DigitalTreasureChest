#include "Arduino.h"
#include "HDSPA22C_Driver.h"

// Display driver reference
HDSPA22C_Driver* driver;

// ## Pin configuration ## //
const int pressurePin = A0;     // Pressure sensor
const int buttonPin = 8;        // Action button
const int powerPin = 7;         // Power led (always on when active == true)
const int activityPin = 6;      // Multipurpose information led
const int solenoidPin = 10;     // Controls solenoid lock

// ## Program configuration ## //
const int startupDelay = 1500;        // Time 'HI' is shown on the display in milliseconds
const int awakeTime = 30;             // Time in seconds the device will stay awake
const int unlockTime = 5;

const int shortFlash = 30;
const int longFlash = 250;

// ## Buffer for pressure sensor smoothing ## //
const int numReadings = 150;        // Size of buffer (WARNING: increasing this drastically increases program size)
int readings[numReadings];          // Array to store readings
int readIndex = 0;                  // Index pointer
int total = 0;                      // Sum of buffer values
int average = 0;                    // Average of buffer values
uint32_t lastReading = millis();    // Timestamp for each reading
uint32_t readTimeout = 12;          // Timeout between each reading (lower numbers increase responsiveness)

// ## State variables ## //
bool active = false;
unsigned long wakeStamp = millis();         // Timestamp of the last time the device was woken up.

short comboLength = 3;
const int code[] = {43, 50, 99};            // Lock code
bool correct[] = {false, false, false};     // Keep track of valid inputs

bool unlocked = false;
unsigned long unlockStamp = millis();

int activityFlashes = 1;
unsigned long activityStamp = millis();
int flashTime = 0;

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
void checkCode();
void lock();
void unlock();
void shortActFlash();

void setup() {
    Serial.begin(9600);

    driver = new HDSPA22C_Driver();
    driver->clear();

    pinMode(buttonPin, INPUT);
    pinMode(powerPin, OUTPUT);
    pinMode(activityPin, OUTPUT);

    // Turn off the builtin led
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 0);

    wake();
}

void loop() {
    if (!active) {
        void (*_wake)() = &wake;
        detectButtonPress(_wake);
        return;
    }

    if (activityFlashes > 0) {
        digitalWrite(activityPin, 1);

        if (millis() > activityStamp + flashTime) {
            digitalWrite(activityPin, 0);
            activityFlashes--;
        }
    }

    if (unlocked) {
        digitalWrite(activityPin, 1);
        digitalWrite(solenoidPin, 1);

        int timePassed = (millis() - unlockStamp) / 1000;

        driver->setCharacter(0, (unlockTime - timePassed) / 10 + '0');
        driver->setCharacter(1, (unlockTime - timePassed) % 10 + '0');

        driver->send();

        if (millis() > unlockTime * 1000 + unlockStamp) {
            lock();
        }

        return;
    }


    if (millis() > lastReading + readTimeout) {
        // Read new value
        if (readPressure() > 0) {
            keepAwake();
        }
        // Update reading timestamp
        lastReading = millis();
    }

    void (*_checkCode)() = &checkCode;
    detectButtonPress(_checkCode);

    driver->setCharacter(0, average / 10 + '0');
    driver->setCharacter(1, average % 10 + '0');

    driver->send();

    if (active) {
        if (millis() > wakeStamp + 1000 * awakeTime) {
            sleep();
        }
    }
}

void shortActFlash() {
    activityFlashes = 1;
    activityStamp = millis();
    flashTime = shortFlash;
}

void longActFlash() {
    activityFlashes = 1;
    activityStamp = millis();
    flashTime = longFlash;
}

void unlock() {
    unlocked = true;
    unlockStamp = millis();

    digitalWrite(activityPin, 1);
    digitalWrite(solenoidPin, 1);
}

void lock() {
    unlocked = false;
    digitalWrite(activityPin, 0);
    digitalWrite(solenoidPin, 0);

    sleep();
}

void checkCode() {
    // TODO fix this fucking lock because its broke af
    for (int i = 0; i < comboLength; i++) {
        if (!correct[i]) {
            if (average == code[i]) {
                correct[i] = true;
                shortActFlash();
            }
            else {
                // if code is wrong, user must start over.
                correct[0] = false;
                correct[1] = false;
                correct[2] = false;
            }

            // Loop stops after the first empty combination position was found.
            break;
        }
    }

    // If last value is correct, the chest is unlocked.
    if (correct[2]) {
        unlock();
    }

    keepAwake();
}

void keepAwake() {
    wakeStamp = millis();
}

void wake() {
    digitalWrite(powerPin, 1);

    driver->setCharacter(0, 'H');
    driver->setCharacter(1, 'I');

    wakeStamp = millis();

    while (millis() < wakeStamp + startupDelay) {
        driver->send();
    }

    active = true;
    wakeStamp = millis();
}

void sleep() {
    digitalWrite(powerPin, 0);

    active = false;
    driver->clear();

    // Reset lock
    for (int i = 0; i < comboLength; i++) {
        correct[i] = false;
    }

    // Reset array of readings to preserve data integrity (fancy words for 'restarting the device')
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }
    total = 0;
}

int readPressure() {
    int pressure = map(analogRead(pressurePin), 0, 750, 0, 99);

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
