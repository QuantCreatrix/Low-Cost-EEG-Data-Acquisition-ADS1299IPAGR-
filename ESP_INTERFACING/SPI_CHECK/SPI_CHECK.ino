#include <SPI.h>

#define _WAKEUP 0x02 // Wake-up from standby mode
#define _STANDBY 0x04 // Enter Standby mode
#define _RESET 0x06 // Reset the device
#define _START 0x08 // Start and restart (synchronize) conversions
#define _STOP 0x0A // Stop conversion
#define _RDATAC 0x10 // Enable Read Data Continuous mode (default mode at power-up)
#define _SDATAC 0x11 // Stop Read Data Continuous mode
#define _RDATA 0x12 // Read data by command; supports multiple read back

#define _RREG 0x20 // (also = 00100000) is the first opcode that the address must be added to for RREG communication
#define _WREG 0x40 // 01000000 in binary (Datasheet, pg. 35)

//Register Addresses
#define ID 0x00

#define CS 5  // Chip Select Pin
#define PWDN 22
#define RESET 16
#define START 21
#define DRDY 17
#define CLKSEL 4

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(PWDN, OUTPUT);
  pinMode(START, OUTPUT);
  pinMode(DRDY, INPUT);
  pinMode(RESET, OUTPUT);
  pinMode(CLKSEL, OUTPUT);

    // Default Values
  digitalWrite(CLKSEL, LOW);
  digitalWrite(START, HIGH);
  digitalWrite(PWDN, HIGH);
  digitalWrite(RESET, HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(CS, LOW);
  SPI.transfer(_WAKEUP);
  delay(10);
  SPI.transfer(_RESET);
  delay(10);
  SPI.transfer(_START);
  delay(10);
  
  uint8_t cb1 = (_RREG) | 0x00;  // Command byte: RREG + address
    // uint8_t cb2 = register_num - 1;            // Number of registers to read - 1
    SPI.transfer(_SDATAC);  // Stop Read Data Continuous

    SPI.transfer();
    SPI.transfer()
    SPI.transfer(cb1);
    SPI.transfer(0x00);

    Serial.println("\n--- Register Read ---");
    Serial.print(SPI.transfer(0x00));
    digitalWrite(CS, HIGH);  // Deselect ADS1299
    delayMicroseconds(10);
}

void loop() {
  // put your main code here, to run repeatedly:

}
