#include "Ps2Mouse.h"

static const int PS2_CLOCK = 2;
static const int PS2_DATA  = 17;
static const int RS232_RTS = 3;
static const int RS232_TX  = 4;

static Ps2Mouse mouse(PS2_CLOCK, PS2_DATA, Ps2Mouse::Mode::remote);
static bool reset = false;

static void sendSerialBit(int data) {
  // Delay between the signals to match 1200 baud
  static const auto usDelay = 1000000 / 1200;
  if (!reset) {
      digitalWrite(RS232_TX, data);
      delayMicroseconds(usDelay);
  }
}

static void sendSerialByte(byte data) {

  // Start bit
  sendSerialBit(0);

  // Data bits
  for (int i = 0; i < 7; i++) {
    sendSerialBit((data >> i) & 0x01);
  }

  // Stop bit
  sendSerialBit(1);

  // 7+1 bits is normal mouse protocol, but some serial controllers
  // expect 8+1 bits format. We send additional stop bit to stay
  // compatible to that kind of controllers.
  sendSerialBit(1);
}

static void sendToSerial(const Ps2Mouse::Data& data) {
  byte lb = data.leftButton << 5;
  byte rb = data.rightButton << 4;
  byte dx = static_cast<byte>(constrain(data.xMovement, -127, 127));
  byte dy = static_cast<byte>(constrain(-data.yMovement, -127, 127));
  sendSerialByte(0x40 | lb | rb | ((dy >> 4) & 0xC) | (dx >> 6));
  sendSerialByte(dx & 0x3F);
  sendSerialByte(dy & 0x3F);
  byte mb = data.middleButton ? 0x20 : 0x00;
  sendSerialByte(mb);
}

static void initPs2Mouse() {
  Serial.print("Activating PS/2 mouse... ");
  if (!mouse.reset()) {
    Serial.println("failed");
  } else {
    Serial.println("ok");
  }
}

static void initSerialMouse() {
  Serial.print("Activating Serial Port... ");
  pinMode(RS232_TX, OUTPUT);
  reset = false;
  sendSerialByte('M');
  sendSerialByte('3');
  Serial.println("ok");
}

void setup() {
  // PS/2 Data input must be initialized shortly after power on,
  // or the mouse will not initialize
  pinMode(PS2_DATA, INPUT_PULLUP);
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  initPs2Mouse();
  auto handler = []() { reset = true; digitalWrite(RS232_TX, HIGH); };
  attachInterrupt(digitalPinToInterrupt(RS232_RTS), handler, FALLING);
}

void loop() {
  if (reset) {
      digitalWrite(13, HIGH);
      initSerialMouse();
      digitalWrite(13, LOW);
  } else {
    Ps2Mouse::Data data;
    if (mouse.readData(data)) {
      sendToSerial(data);
    }
  }
}
