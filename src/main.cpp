#include "Arduino.h"
#include "HDSPA22C_Driver.h"

// Display driver reference
HDSPA22C_Driver* driver;

// ## Pin configuration ## //
const int pressurePin = A0;             // Pressure sensor
const int buttonPin = 8;                // Action button
const int powerPin = 7;                 // Power led (always on when active == true)
const int activityPin = 6;              // Multipurpose information led
const int solenoidPin = 10;             // Controls solenoid lock

// ## Program configuration ## //
const int startupDelay = 1500;          // Time 'HI' is shown on the display in milliseconds
const int awakeTime = 30;               // Time in seconds the device will stay awake
const int unlockTime = 5;               // Time in seconds the lock stays open after being unlocked

// # Activity LED config
const int shortFlash = 30;              // Time in milliseconds a short flash takes (time LED is on)
const int longFlash = 500;              // Time in milliseconds a long flash takes

const short comboLength = 3;            // Length of the lock combination
const int combination[comboLength] = {43, 50, 99};  // Lock combination

// ## Buffer for pressure sensor smoothing ## //
const int numReadings = 150;            // Size of buffer (WARNING: increasing this drastically increases program size)
int readings[numReadings];              // Array to store readings
int readIndex = 0;                      // Index pointer
int total = 0;                          // Sum of buffer values
int average = 0;                        // Average of buffer values
uint32_t lastReading = millis();        // Timestamp for each reading
uint32_t readTimeout = 12;              // Timeout between each reading (lower numbers increase responsiveness)

// ## State variables ## //
bool active = false;                        // Defines if the device is active (power led on and ready to receive input)
unsigned long wakeStamp = millis();         // Timestamp of the last time the device was woken up.

bool correct[] = {false, false, false};     // Keep track of valid inputs

bool unlocked = false;                      // Defines if the device is unlocked
unsigned long unlockStamp = millis();       // Timestamp of when the device was last unlocked.

// # Activity led state variables
int activityFlashes = 1;                    // Currently implemented as a boolean, if the value is 1 the activity led will flash on the next loop
unsigned long activityStamp = millis();     // Timestamp of the first loop when the led was turned on by activityFlashes
int flashTime = 0;                          // Current flash time, can be defined by constants longFlash and shortFlash

// # Button press state variables
bool buttonPressed = false;                     // Defines if the button was pressed in the last loop
unsigned long pressTimeout = 10;                // timeout for new buttonpress in ms
unsigned long pressTimeoutStamp = millis();     // Timestamp of the moment the button was last pressed
unsigned long releaseTimeoutStamp = millis();   // Timestamp of the moment the button was last released

// ## Function declarations ## //
// For function description, see implementation
int readPressure();
void detectButtonPress(void (*callback)());
void keepAwake();
void wake();
void sleep();
void checkCode();
void lock();
void unlock();
void shortActFlash();
void unlockCountdown();

void setup() {
    // Open a serial connection
    Serial.begin(9600);

    // Create driver
    driver = new HDSPA22C_Driver();
    driver->clear();

    // Setup IO pins
    pinMode(buttonPin, INPUT);
    pinMode(powerPin, OUTPUT);
    pinMode(activityPin, OUTPUT);
    pinMode(solenoidPin, OUTPUT);

    // Turn off the builtin led
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 0);

    // Wake the device
    wake();
}

void loop() {
    // When the device is not active, wait for button press
    if (!active) {
        void (*_wake)() = &wake;
        detectButtonPress(_wake);
        return;
    }

    // If activityFlashes is a positive number, it means a flash has been requested
    if (activityFlashes > 0) {
        // Turn the pin on (yeah, each cycle but really it just works)
        digitalWrite(activityPin, 1);

        // If the time has passed, turn the led off again and remove the request.
        if (millis() > activityStamp + flashTime) {
            digitalWrite(activityPin, 0);
            activityFlashes--;
        }
    }

    // Countdown until the timeout has passed and lock the device again (isolated state)
    if (unlocked) {
        unlockCountdown();
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

    // Check for a button press
    void (*_checkCode)() = &checkCode;
    detectButtonPress(_checkCode);

    // Show current pressure level on the display.
    driver->setCharacter(0, average / 10 + '0');
    driver->setCharacter(1, average % 10 + '0');

    driver->send();

    // Check if the device should go to bed again (see config)
    if (active) {
        if (millis() > wakeStamp + 1000 * awakeTime) {
            sleep();
        }
    }
}

// Keeps track of when the unlocked device should lock again.
void unlockCountdown() {
    // Calculate time passed since unlock
    int timePassed = (millis() - unlockStamp) / 1000;

    // Show the time left (in seconds) on the display
    driver->setCharacter(0, (unlockTime - timePassed) / 10 + '0');
    driver->setCharacter(1, (unlockTime - timePassed) % 10 + '0');

    driver->send();

    // Lock the device again if the specified time has passed (see configuration)
    if (millis() > unlockTime * 1000 + unlockStamp) {
        lock();
    }
}

// Request a short flash
void shortActFlash() {
    activityFlashes = 1;
    activityStamp = millis();
    flashTime = shortFlash;
}

// Request a long flash
void longActFlash() {
    activityFlashes = 1;
    activityStamp = millis();
    flashTime = longFlash;
}

// Unlock the device
void unlock() {
    unlocked = true;
    unlockStamp = millis();

    digitalWrite(activityPin, 1);
    digitalWrite(solenoidPin, 1);
}

// Lock the device
void lock() {
    unlocked = false;
    digitalWrite(activityPin, 0);
    digitalWrite(solenoidPin, 0);

    sleep();
}

// Check if the current pressure level is the right code
void checkCode() {
    // Loop over the stored combination
    for (int i = 0; i < comboLength; i++) {
        // Find the first value that is false
        if (!correct[i]) {
            // Provided code is correct
            if (average == combination[i]) {
                // Save this unlock
                correct[i] = true;
                // Request an activity flash
                shortActFlash();
            }
            else {
                // If code is wrong, user must start over. Reset all values in the array
                correct[0] = false;
                correct[1] = false;
                correct[2] = false;
                // Request an activity flash
                longActFlash();
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

// Keeps the device awake by updating the wakeup stamp to the current time
void keepAwake() {
    wakeStamp = millis();
}

// Wake the device (set state to active)
void wake() {
    // Turn power led on
    digitalWrite(powerPin, 1);

    // Show "HI" on the display
    driver->setCharacter(0, 'H');
    driver->setCharacter(1, 'I');

    // Save wake up timestamp
    wakeStamp = millis();

    // Show "HI" for the amount of time defined in the configuration (startupDelay)
    while (millis() < wakeStamp + startupDelay) {
        driver->send();
    }

    // Set state variable
    active = true;
    // Update wake up timestamp with the startupDelay
    wakeStamp = millis();
}

// Enter sleep mode (state is not active)
void sleep() {
    // Turn power led off
    digitalWrite(powerPin, 0);

    // Change state variable
    active = false;

    // Clear display
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

// Read the pressure from the pressure sensor and store it in the average varaible
int readPressure() {
    // Map input to a range of 0-99
    int pressure = map(analogRead(pressurePin), 0, 750, 0, 99);

    // Clamp pressure to extreme values (in case it overshoots or I don't know, power fluctuations)
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
