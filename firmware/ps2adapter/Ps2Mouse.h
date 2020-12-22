#pragma once

#include <Arduino.h>

class Ps2Mouse {
public:

  enum Mode {
    remote,
    stream, 
  };

  struct Data {
    bool leftButton;
    bool middleButton;
    bool rightButton;
    short xMovement;
    short yMovement; 
  };
  
  Ps2Mouse(int clockPin, int dataPin, Mode mode);

  bool reset() const;

  bool setDataReporting(bool flag) const;
  bool getDataReporting(bool& flag) const;

  bool setScaling(bool flag) const;
  bool getScaling(bool& flag) const;

  bool setResolution(byte resolution) const;
  bool getResolution(byte& resolution) const;

  bool setSampleRate(byte sampleRate) const;
  bool getSampleRate(byte& sampleRate) const;

  bool hasData() const;

  bool readData(Data& data) const;

private:
  int m_clockPin;
  int m_dataPin;
  Mode m_mode;
};
