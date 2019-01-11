#ifndef HDSPA22C_Driver_h
#define HDSPA22C_Driver_h

#include "Arduino.h"
#include "Adafruit_MCP23017.h"

class HDSPA22C_Driver
{
public:
    HDSPA22C_Driver();
    void setCharacter(byte char_i, char c);
    void send();
    uint16_t resolveCharacter(char c);
    void clear();
private:
    char char1;
    char char2;
    Adafruit_MCP23017 mcp;
};

#endif
