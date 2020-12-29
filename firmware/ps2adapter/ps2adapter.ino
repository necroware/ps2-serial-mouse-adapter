#include "Ps2Mouse.h"

static const int PS2_CLOCK = 2;
static const int PS2_DATA  = 17;
static const int RS232_RTS = 3;
static const int RS232_TX  = 4;
static const int JP12 = 11;
static const int JP34 = 12;
static const int LED = 13;

static Ps2Mouse mouse(PS2_CLOCK, PS2_DATA);
static bool threeButtons = false;

static void sendSerialBit(int data) {
  // Delay between the signals to match 1200 baud
  static const auto usDelay = 1000000 / 1200;
  digitalWrite(RS232_TX, data);
  delayMicroseconds(usDelay);
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
  auto dx = constrain(data.xMovement, -127, 127);
  auto dy = constrain(-data.yMovement, -127, 127);
  byte lb = data.leftButton ? 0x20 : 0;
  byte rb = data.rightButton ? 0x10 : 0;
  sendSerialByte(0x40 | lb | rb | ((dy >> 4) & 0xC) | ((dx >> 6) & 0x3));
  sendSerialByte(dx & 0x3F);
  sendSerialByte(dy & 0x3F);
  if (threeButtons) {
    byte mb = data.middleButton ? 0x20 : 0;
    sendSerialByte(mb);
  }
}

static void initSerialPort() {
  Serial.println("Starting serial port");
  digitalWrite(RS232_TX, HIGH);
  delayMicroseconds(10000);
  sendSerialByte('M');
  if(threeButtons) {
    sendSerialByte('3');
    Serial.println("Init 3-buttons mode");
  }
  delayMicroseconds(10000);

  Serial.println("Listening on RTS");
  void (*resetHack)() = 0;
  attachInterrupt(digitalPinToInterrupt(RS232_RTS), resetHack, FALLING);
}

static void initPs2Port() {
  Serial.println("Reseting PS/2 mouse");
  mouse.reset();
  mouse.setSampleRate(20);

  Ps2Mouse::Settings settings;
  if (mouse.getSettings(settings)) {
    Serial.print("scaling = ");
    Serial.println(settings.scaling);
    Serial.print("resolution = ");
    Serial.println(settings.resolution);
    Serial.print("samplingRate = ");
    Serial.println(settings.sampleRate);
  }
}

void setup() {
  // PS/2 Data input must be initialized shortly after power on,
  // or the mouse will not initialize
  pinMode(PS2_DATA, INPUT_PULLUP);
  pinMode(RS232_TX, OUTPUT);
  pinMode(JP12, INPUT_PULLUP);
  pinMode(JP34, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  threeButtons = digitalRead(JP12);
  Serial.begin(115200);
  initSerialPort();
  initPs2Port();
  Serial.println("Setup done!");
  digitalWrite(LED, LOW);
  if (digitalRead(JP34) == LOW) {
    Serial.println("Enabling streaming mode");
    mouse.enableStreaming();
  }
}

void loop() {
  Ps2Mouse::Data data;
  if (mouse.readData(data)) {
    sendToSerial(data);
  }
}
