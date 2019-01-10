#include "Arduino.h"
#include "DisplayDriver.h"
#include "Adafruit_MCP23017.h"

const uint16_t characterTable[36][2] = {
    {48, 0b1010000110000110}, //0
    {49, 0b1000000000010010}, //1
    {50, 0b0011000100001110}, //2
    {51, 0b1011000000000110}, //3
    {52, 0b1001000010001010}, //4
    {53, 0b1011000010001100}, //5
    {54, 0b1011000110001100}, //6
    {55, 0b1000000000000110}, //7
    {56, 0b1011000110001110}, //8
    {57, 0b1011000010001110}, //9
    {65, 0b1001000110001110}, //A
    {66, 0b1011010000100110}, //B
    {67, 0b0010000110000100}, //C
    {68, 0b0010010000100110}, //D
    {69, 0b0010000110001100}, //E
    {70, 0b0000000110001100}, //F
    {71, 0b1011000110000100}, //G
    {72, 0b1001000110001010}, //H
    {73, 0b0010010000100100}, //I
    {74, 0b1010000100000010}, //J
    {75, 0b0000100110011000}, //K
    {76, 0b0010000110000000}, //L
    {77, 0b1000000111010010}, //M
    {78, 0b1000100111000010}, //N
    {80, 0b0001000110001110}, //P
    {81, 0b1010100110000110}, //Q
    {82, 0b0001100110001110}, //R
    {84, 0b0000001000100100}, //T
    {85, 0b1010000110000010}, //U
    {86, 0b0000001110010000}, //V
    {87, 0b1000101110000010}, //W
    {88, 0b0000101001010000}, //X
    {89, 0b0000001001010000}, //Y
    {90, 0b0010001000010100}, //Z
    {45, 0b0001000000001000}  //-
};

DisplayDriver::DisplayDriver() {
    mcp.begin(0);
    Wire.beginTransmission(0x20);
    Wire.write(MCP23017_IODIRA);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.beginTransmission(0x20);
    Wire.write(MCP23017_IODIRB);
    Wire.write(0x00);
    Wire.endTransmission();

    Wire.setClock(400000);

    pinMode(3, OUTPUT);
}

void DisplayDriver::setCharacter(int char_i, char c) {
    if (char_i == 0) {
        char1 = c;
    }
    else if (char_i == 1) {
        char2 = c;
    }
}

void DisplayDriver::send() {
    mcp.writeGPIOAB(~0);
    mcp.writeGPIOAB(~resolveCharacter(char1) - 1);

    //delay(10);
    mcp.writeGPIOAB(~1);
    mcp.writeGPIOAB(~resolveCharacter(char2));
}

uint16_t DisplayDriver::resolveCharacter(char c) {
    if (c == 'O') {
        c = '0';
    }
    else if (c == 'S') {
        c = '5';
    }

    //Cast character to int
    uint16_t intval = c;

    //Find the value by looping over the ascii values in the first col of the character table and return the associated output value.
    for (int i = 0; i < 36; i++) {
        if (characterTable[i][0] == intval) {
            return characterTable[i][1];
        }
    }

    // Return 0 if no match was found.
    return 0;
}
