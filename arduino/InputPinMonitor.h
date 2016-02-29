#ifndef InputPin_h
#define InputPin_h

#include "Arduino.h"

class InputPinMonitor
{
  public:
    InputPinMonitor(byte pin);
  private:
    PIN_STATE _state;
    byte      _pin;
};

#endif
