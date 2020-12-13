#pragma once

#include <stdint.h>

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
  
  Ps2Mouse(uint8_t clockPin, uint8_t dataPin, Mode mode);

  bool reset() const;

  bool setDataReporting(bool flag) const;
  bool getDataReporting(bool& flag) const;

  bool setScaling(bool flag) const;
  bool getScaling(bool& flag) const;

  bool setResolution(uint8_t resolution) const;
  bool getResolution(uint8_t& resolution) const;

  bool setSampleRate(uint8_t sampleRate) const;
  bool getSampleRate(uint8_t& sampleRate) const;

  bool hasData() const;

  bool readData(Data& data) const;

private:
  uint8_t m_clockPin;
  uint8_t m_dataPin;
  Mode m_mode;
};
