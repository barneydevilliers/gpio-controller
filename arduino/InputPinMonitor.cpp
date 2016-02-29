#include "Arduino.h"
#include "InputPinMonitor"



InputPinMonitor::InputPinMonitor(byte pin)
{
  _pin = pin;

  pinMode(pin, INPUT);
  if (HIGH == digitalRead(_pin))
  {
    _state = PIN_STATE_HIGH;
  }
  else
  {
    _state = PIN_STATE_LOW;
  }
}
