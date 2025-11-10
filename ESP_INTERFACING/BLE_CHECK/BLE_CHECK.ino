#include <SPI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// ADS1299 SPI Commands
#define _WAKEUP  0x02
#define _STANDBY 0x04
#define _RESET   0x06
#define _START   0x08
#define _STOP    0x0A
#define _RDATAC  0x10
#define _SDATAC  0x11
#define _RDATA   0x12
#define _RREG    0x20
#define _WREG    0x40

// Register Addresses
#define CONFIG1  0x01
#define CONFIG2  0x02
#define CONFIG3  0x03
#define CH1SET   0x04

// Pin Definitions
#define CS      5
#define PWDN    22
#define RESET   16
#define START   21
#define DRDY    17
#define CLKSEL  4

// SPI Settings
const uint32_t SPI_CLOCK = 1000000;
const uint8_t BIT_ORDER = MSBFIRST;
const uint8_t SPI_MODE = SPI_MODE1;

// BLE objects
BLECharacteristic *eegCharacteristic;

void printRegisterValue(uint8_t address, uint8_t value) {
  Serial.print("Reg 0x");
  if (address < 0x10) Serial.print("0");
  Serial.print(address, HEX);
  Serial.print(" = 0x");
  if (value < 0x10) Serial.print("0");
  Serial.println(value, HEX);
}

void WREG(uint8_t reg_address, uint8_t reg_num, uint8_t* reg_data) {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);

  SPI.transfer(_SDATAC);  // Stop continuous mode
  SPI.transfer(_WREG | reg_address);
  SPI.transfer(reg_num - 1);
  for (uint8_t i = 0; i < reg_num; i++) {
    SPI.transfer(reg_data[i]);
    printRegisterValue(reg_address + i, reg_data[i]);
  }

  digitalWrite(CS, HIGH);
  SPI.endTransaction();
  delayMicroseconds(10);
}

void RREG(uint8_t reg_address, uint8_t reg_num) {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);
  SPI.transfer(_SDATAC);
  SPI.transfer(_RREG | reg_address);
  SPI.transfer(reg_num - 1);

  Serial.println("\n--- Register Values ---");
  for (uint8_t i = 0; i < reg_num; i++) {
    uint8_t data = SPI.transfer(0x00);
    printRegisterValue(reg_address + i, data);
  }

  digitalWrite(CS, HIGH);
  SPI.endTransaction();
  delayMicroseconds(10);
}

void WAKEUP() {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);
  SPI.transfer(_WAKEUP);
  digitalWrite(CS, HIGH);
  delayMicroseconds(4);
  SPI.endTransaction();
}

void RESET_IC() {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);
  SPI.transfer(_RESET);
  digitalWrite(CS, HIGH);
  delay(10);
  SPI.endTransaction();
}

void START_IC() {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);
  SPI.transfer(_START);
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

void STOP() {
  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);
  SPI.transfer(_STOP);
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

void RDATA() {
  while (digitalRead(DRDY) == HIGH);  // Wait for DRDY LOW

  SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
  digitalWrite(CS, LOW);

  SPI.transfer(_RDATA);
  SPI.transfer(0x00); // Discard STATUS byte

  String dataStr = "";

  for (int ch = 0; ch < 8; ch++) {
    uint8_t b1 = SPI.transfer(0x00);
    uint8_t b2 = SPI.transfer(0x00);
    uint8_t b3 = SPI.transfer(0x00);
    int32_t sample = (b1 << 16) | (b2 << 8) | b3;
    if (sample & 0x00800000) sample |= 0xFF000000;

    dataStr += String(sample);
    if (ch < 7) dataStr += ",";  // Comma-separated values
  }

  digitalWrite(CS, HIGH);
  SPI.endTransaction();

  // Send over BLE
  eegCharacteristic->setValue(dataStr.c_str());
  eegCharacteristic->notify();

  // Debug to Serial Monitor
  Serial.println(dataStr);
}

void initializeADS1299() {
  digitalWrite(PWDN, HIGH);
  delay(1);
  digitalWrite(RESET, LOW);
  delay(1);
  digitalWrite(RESET, HIGH);
  delay(100);

  RESET_IC();
  WAKEUP();

  uint8_t config1[1] = {0x96};  // 250SPS, daisy off
  uint8_t config2[1] = {0xD0};  // Test signal
  uint8_t config3[1] = {0xE0};  // Internal ref
  WREG(CONFIG1, 1, config1);
  WREG(CONFIG2, 1, config2);
  WREG(CONFIG3, 1, config3);

  uint8_t chnset[8] = {0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45}; // All test
  WREG(CH1SET, 8, chnset);

  RREG(CONFIG1, 3);
  RREG(CH1SET, 8);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, SS
  pinMode(CS, OUTPUT);
  pinMode(PWDN, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(START, OUTPUT);
  pinMode(DRDY, INPUT);
  pinMode(CLKSEL, OUTPUT);

  digitalWrite(CS, HIGH);
  digitalWrite(PWDN, HIGH);
  digitalWrite(RESET, HIGH);
  digitalWrite(START, LOW);
  digitalWrite(CLKSEL, HIGH);  // Internal clock

  // Start BLE
  BLEDevice::init("ADS1299_EEG");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  eegCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY);
  eegCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE Advertising as ADS1299_EEG...");

  initializeADS1299();
  Serial.println("ADS1299 initialized");
  START_IC();
}

void loop() {
  RDATA();        // Read and send one EEG sample (8 channels)
  delay(4);       // 250Hz sampling rate (4ms delay)
}
