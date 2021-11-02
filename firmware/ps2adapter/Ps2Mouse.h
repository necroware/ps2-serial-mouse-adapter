#pragma once

#include <Arduino.h>

class Ps2Mouse {
public:

  struct Data {
    bool leftButton;
    bool middleButton;
    bool rightButton;
    int  xMovement;
    int  yMovement;
  };

  struct Settings {
    bool scaling;
    byte resolution;
    byte sampleRate;
  };
  
  Ps2Mouse(int clockPin, int dataPin);

  bool reset() const;

  bool setScaling(bool flag) const;
  bool setResolution(byte resolution) const;
  bool setSampleRate(byte sampleRate) const;

  bool getSettings(Settings& settings) const;

  bool enableStreaming() const;
  bool disableStreaming() const;
  bool readData(Data& data) const;

private:
  struct Impl;

  int m_clockPin;
  int m_dataPin;
  bool m_stream;
};
