#ifndef DisplayDriver_h
#define DisplayDriver_h

#include "Arduino.h"
#include "Adafruit_MCP23017.h"

class DisplayDriver
{
public:
    DisplayDriver();
    void setCharacter(int char_i, char c);
    void send();
    uint16_t resolveCharacter(char c);
private:
    char char1;
    char char2;
    Adafruit_MCP23017 mcp;
};

#endif
