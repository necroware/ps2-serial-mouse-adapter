#include "Ps2Mouse.h"

namespace {

struct Status {

  byte rightButton   : 1;
  byte middleButton  : 1;
  byte leftButton    : 1;
  byte na2           : 1;
  byte scaling       : 1;
  byte dataReporting : 1;
  byte remoteMode    : 1;
  byte na1           : 1;

  byte resolution;
  byte sampleRate;
};

struct Packet {

  byte leftButton    : 1;
  byte rightButton   : 1;
  byte middleButton  : 1;
  byte na            : 1;
  byte xSign         : 1;
  byte ySign         : 1;
  byte xOverflow     : 1;
  byte yOverflow     : 1;

  byte xMovement;
  byte yMovement;
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

void sendBit(int clockPin, int dataPin, int value) {
  while (digitalRead(clockPin) != LOW) {}
  digitalWrite(dataPin, value);
  while (digitalRead(clockPin) != HIGH) {}
}

int recvBit(int clockPin, int dataPin) {
  while (digitalRead(clockPin) != LOW) {}
  auto result = digitalRead(dataPin);
  while (digitalRead(clockPin) != HIGH) {}
  return result;
}

bool sendByte(int clockPin, int dataPin, byte value) {

  // Inhibit communication
  pinMode(clockPin, OUTPUT);
  digitalWrite(clockPin, LOW);
  delayMicroseconds(100);

  // Set start bit and release the clock
  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  pinMode(clockPin, INPUT_PULLUP);
  
  // Send data bits
  byte parity = 1;
  for (auto i = 0; i < 8; i++) {
    byte nextBit = (value >> i) & 0x01;
    parity ^= nextBit;
    sendBit(clockPin, dataPin, nextBit);
  }

  // Send parity bit
  sendBit(clockPin, dataPin, parity);

  // Send stop bit
  sendBit(clockPin, dataPin, 1);

  // Enter receive mode and wait for ACK bit
  pinMode(dataPin, INPUT);
  return recvBit(clockPin, dataPin) == 0;
}

bool recvByte(int clockPin, int dataPin, byte& value) {

  // Enter receive mode
  pinMode(clockPin, INPUT);
  pinMode(dataPin, INPUT);

  // Receive start bit
  if (recvBit(clockPin, dataPin) != 0) {
    return false;
  }

  // Receive data bits
  value = 0;
  byte parity = 1;
  for (int i = 0; i < 8; i++) {
    byte nextBit = recvBit(clockPin, dataPin);
    value |= nextBit << i;
    parity ^= nextBit;
  }

  // Receive and check parity bit
  recvBit(clockPin, dataPin); // TODO check parity

  // Receive stop bit
  recvBit(clockPin, dataPin);

  return true;
}

template <typename T>
bool sendData(int clockPin, int dataPin, const T& data) {
  auto ptr = reinterpret_cast<const byte*>(&data);
  for (auto i = 0; i < sizeof(data); i++) {
    if (!sendByte(clockPin, dataPin, ptr[i])) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool recvData(int clockPin, int dataPin, T& data) {
  auto ptr = reinterpret_cast<byte*>(&data);
  for (auto i = 0; i < sizeof(data); i++) {
    if (!recvByte(clockPin, dataPin, ptr[i])) {
      return false;
    }
  }
  return true;
}

bool sendByteWithAck(int clockPin, int dataPin, byte value) {
  while (true) {
    if (sendByte(clockPin, dataPin, value)) {
      byte response;
      if (recvByte(clockPin, dataPin, response)) {
        if (response == static_cast<byte>(Response::resend)) {
          continue;
        }
        return response == static_cast<byte>(Response::ack);
      }
    }
    return false;
  }
}

bool sendCommand(int clockPin, int dataPin, Command command) {
  return sendByteWithAck(clockPin, dataPin, static_cast<byte>(command));
}

bool sendSetting(int clockPin, int dataPin, Command command, byte setting) {
  if (sendCommand(clockPin, dataPin, command)) {
    return sendByteWithAck(clockPin, dataPin, setting);
  }
  return false;
}

bool getStatus(int clockPin, int dataPin, Status& status) {
  if (sendCommand(clockPin, dataPin, Command::statusRequest)) {
    return recvData(clockPin, dataPin, status);
  }
  return false;
}

bool setMode(int clockPin, int dataPin, Ps2Mouse::Mode mode) {
  switch (mode) {
    case Ps2Mouse::Mode::remote:
      return sendCommand(clockPin, dataPin, Command::setRemoteMode);
    case Ps2Mouse::Mode::stream:
      return sendCommand(clockPin, dataPin, Command::setStreamMode);
  }
  return false;
}

} // namespace

Ps2Mouse::Ps2Mouse(int clockPin, int dataPin, Mode mode)
  : m_clockPin(clockPin), m_dataPin(dataPin), m_mode(mode)
{}

bool Ps2Mouse::reset() const {
  if (sendCommand(m_clockPin, m_dataPin, Command::reset)) {
    byte reply;
    recvByte(m_clockPin, m_dataPin, reply);
    if (reply == static_cast<byte>(Response::selfTestPassed)) {
      recvByte(m_clockPin, m_dataPin, reply);
      if (reply == static_cast<byte>(Response::isMouse)) {
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

bool Ps2Mouse::setResolution(byte resolution) const {
  return sendSetting(m_clockPin, m_dataPin, Command::setResolution, resolution);
}

bool Ps2Mouse::getResolution(byte& resolution) const {
  Status status;
  if (getStatus(m_clockPin, m_dataPin, status)) {
    resolution = status.resolution;
    return true;
  }
  return false;
}

bool Ps2Mouse::setSampleRate(byte sampleRate) const {
  return sendSetting(m_clockPin, m_dataPin, Command::setSampleRate, sampleRate);
}

bool Ps2Mouse::getSampleRate(byte& sampleRate) const {
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
  if (!recvData(m_clockPin, m_dataPin, packet)) {
    return false;
  }

  data.leftButton = packet.leftButton;
  data.middleButton = packet.middleButton;
  data.rightButton = packet.rightButton;
  data.xMovement = packet.xSign ? (0xFF00 | packet.xMovement) : packet.xMovement;
  data.yMovement = packet.ySign ? (0xFF00 | packet.yMovement) : packet.yMovement;
  return true;
}
