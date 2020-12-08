#include "Ps2Mouse.h"
#include <Arduino.h>

namespace {

struct Status {

  uint8_t rightButton   : 1;
  uint8_t middleButton  : 1;
  uint8_t leftButton    : 1;
  uint8_t na2           : 1;
  uint8_t scaling       : 1;
  uint8_t dataReporting : 1;
  uint8_t remoteMode    : 1;
  uint8_t na1           : 1;

  uint8_t resolution;
  uint8_t sampleRate;
};

struct Packet {

  uint8_t leftButton    : 1;
  uint8_t rightButton   : 1;
  uint8_t middleButton  : 1;
  uint8_t na            : 1;
  uint8_t xSign         : 1;
  uint8_t ySign         : 1;
  uint8_t xOverflow     : 1;
  uint8_t yOverflow     : 1;

  uint8_t xMovement;
  uint8_t yMovement;
};

enum class Command {
  disableScaling        = 0xE6,
  enableScaling         = 0xE7,
  setResolution         = 0xE8,
  statusRequest         = 0xE9,
  setStreamMode         = 0xEA,
  readData              = 0xEB,
  resetWrapMode         = 0xEC, // Not implemented
  setWrapMode           = 0xEE, // Not implemented
  reset                 = 0xFF,
  setRemoteMode         = 0xF0,
  getDeviceId           = 0xF2, // Not implemented
  setSampleRate         = 0xF3,
  enableDataReporting   = 0xF4,
  disableDataReporting  = 0xF5,
  setDefaults           = 0xF6, // Not implemented
};

enum class Response {
  isMouse        = 0x00,
  selfTestPassed = 0xAA,
  ack            = 0xFA,
  error          = 0xFC,
  resend         = 0xFE,
};

void sendBit(uint8_t clockPin, uint8_t dataPin, uint8_t value) {
  Serial.println(__LINE__);
  while (digitalRead(clockPin) != LOW) {}
  Serial.println(__LINE__);
  digitalWrite(dataPin, value ? 1 : 0);
  while (digitalRead(clockPin) != HIGH) {}
  Serial.println(__LINE__);
}

uint8_t recvBit(uint8_t clockPin, uint8_t dataPin) {
  Serial.println(__LINE__);
  while (digitalRead(clockPin) != LOW) {}
  Serial.println(__LINE__);
  uint8_t result = digitalRead(dataPin);
  while (digitalRead(clockPin) != HIGH) {}
  Serial.println(__LINE__);
  return result;
}

bool sendByte(uint8_t clockPin, uint8_t dataPin, uint8_t value) {

  // Inhibit communication
  pinMode(clockPin, OUTPUT);
  digitalWrite(clockPin, LOW);
  delayMicroseconds(150);

  // Set start bit and release the clock
  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  pinMode(clockPin, INPUT_PULLUP);
  
  // Send data bits
  int parity = 1;
  for (int i = 0; i < 8; i++) {
    int nextBit = (value >> i) & 0x01;
    parity ^= nextBit;
    Serial.println(__LINE__);
    sendBit(clockPin, dataPin, nextBit);
    Serial.println(__LINE__);
  }

  // Send parity bit
  sendBit(clockPin, dataPin, parity);

  // Send stop bit
  sendBit(clockPin, dataPin, 1);

  // Enter receive mode and wait for ACK bit
  pinMode(dataPin, INPUT);
  return recvBit(clockPin, dataPin) == 0;
}

bool recvByte(uint8_t clockPin, uint8_t dataPin, uint8_t* value) {

  if (!value) {
    return false;
  }

  // Enter receive mode
  pinMode(clockPin, INPUT);
  pinMode(dataPin, INPUT);

  // Receive start bit
  if (recvBit(clockPin, dataPin) != 0) {
    return false;
  }

  // Receive data bits
  *value = 0;
  int parity = 1;
  for (int i = 0; i < 8; i++) {
    int nextBit = recvBit(clockPin, dataPin);
    *value |= nextBit << i;
    parity ^= nextBit;
  }

  // Receive and check parity bit
  recvBit(clockPin, dataPin); // TODO check parity

  // Receive stop bit
  recvBit(clockPin, dataPin);

  return true;
}

bool sendByteWithAck(uint8_t clockPin, uint8_t dataPin, uint8_t value) {
  while (true) {
    if (sendByte(clockPin, dataPin, value)) {
      uint8_t response;
      if (recvByte(clockPin, dataPin, &response)) {
        if (response == static_cast<uint8_t>(Response::resend)) {
          continue;
        }
        return response == static_cast<uint8_t>(Response::ack);
      }
    }
    return false;
  }
}

bool sendCommand(uint8_t clockPin, uint8_t dataPin, Command command) {
  return sendByteWithAck(clockPin, dataPin, static_cast<uint8_t>(command));
}

bool sendSetting(uint8_t clockPin, uint8_t dataPin, Command command, uint8_t setting) {
  if (sendCommand(clockPin, dataPin, command)) {
    return sendByteWithAck(clockPin, dataPin, setting);
  }
  return false;
}

bool getStatus(uint8_t clockPin, uint8_t dataPin, Status& status) {
  if (sendCommand(clockPin, dataPin, Command::statusRequest)) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&status);
    for (int i = 0; i < 3; i++) {
      if (!recvByte(clockPin, dataPin, ptr + i)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool setMode(uint8_t clockPin, uint8_t dataPin, Ps2Mouse::Mode mode) {
  switch (mode) {
    case Ps2Mouse::Mode::remote:
      return sendCommand(clockPin, dataPin, Command::setRemoteMode);
    case Ps2Mouse::Mode::stream:
      return sendCommand(clockPin, dataPin, Command::setStreamMode);
  }
  return false;
}

} // namespace

Ps2Mouse::Ps2Mouse(uint8_t clockPin, uint8_t dataPin, Mode mode)
  : m_clockPin(clockPin), m_dataPin(dataPin), m_mode(mode)
{}

bool Ps2Mouse::reset() const {
  Serial.println(__LINE__);
  if (sendCommand(m_clockPin, m_dataPin, Command::reset)) {
    uint8_t reply;
    recvByte(m_clockPin, m_dataPin, &reply);
    if (reply == static_cast<uint8_t>(Response::selfTestPassed)) {
      recvByte(m_clockPin, m_dataPin, &reply);
      if (reply == static_cast<uint8_t>(Response::isMouse)) {
        return setMode(m_clockPin, m_dataPin, m_mode);
      }
    }
  }
  return false;
}

bool Ps2Mouse::setDataReporting(bool flag) const {
  Command command = flag ? Command::enableDataReporting : Command::disableDataReporting;
  return sendCommand(m_clockPin, m_dataPin, command);
}

bool Ps2Mouse::getDataReporting(bool& flag) const {
  Status status;
  if (getStatus(m_clockPin, m_dataPin, status)) {
    flag = status.dataReporting;
    return true;
  }
  return false;
}

bool Ps2Mouse::setScaling(bool flag) const {
  Command command = flag ? Command::enableScaling : Command::disableScaling;
  return sendCommand(m_clockPin, m_dataPin, command);
}

bool Ps2Mouse::getScaling(bool& flag) const {
  Status status;
  if (getStatus(m_clockPin, m_dataPin, status)) {
    flag = status.scaling;
    return true;
  }
  return false;
}

bool Ps2Mouse::setResolution(uint8_t resolution) const {
  return sendSetting(m_clockPin, m_dataPin, Command::setResolution, resolution);
}

bool Ps2Mouse::getResolution(uint8_t& resolution) const {
  Status status;
  if (getStatus(m_clockPin, m_dataPin, status)) {
    resolution = status.resolution;
    return true;
  }
  return false;
}

bool Ps2Mouse::setSampleRate(uint8_t sampleRate) const {
  return sendSetting(m_clockPin, m_dataPin, Command::setSampleRate, sampleRate);
}

bool Ps2Mouse::getSampleRate(uint8_t& sampleRate) const {
  Status status;
  if (getStatus(m_clockPin, m_dataPin, status)) {
    sampleRate = status.sampleRate;
    return true;
  }
  return false;
}

bool Ps2Mouse::hasData() const {
  return (m_mode == Mode::remote) || (digitalRead(m_clockPin) == LOW);
}

bool Ps2Mouse::readData(Data& data) const {

  if (m_mode == Mode::remote && !sendCommand(m_clockPin, m_dataPin, Command::readData)) {
    return false;
  }

  Packet packet;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&packet);
  for (size_t i = 0; i < sizeof(packet); i++) {
    if (!recvByte(m_clockPin, m_dataPin, ptr + i)) {
      return false;
    }
  }

  data.leftButton = packet.leftButton;
  data.middleButton = packet.middleButton;
  data.rightButton = packet.rightButton;
  data.xMovement = packet.xSign ? (0xFF00 | packet.xMovement) : packet.xMovement;
  data.yMovement = packet.ySign ? (0xFF00 | packet.yMovement) : packet.yMovement;
  return true;
}
