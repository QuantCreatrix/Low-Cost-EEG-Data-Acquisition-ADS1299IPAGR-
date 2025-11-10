#include <SPI.h>
#include "Definitions.h"

// Pin Definitions
#define CS 5  // Chip Select Pin
#define PWDN 22
#define RESET 16
#define START 21
#define DRDY 17
#define CLKSEL 4


// SPI Settings (Define these constants)
const uint32_t SPI_CLOCK = 1000000;  
const uint8_t BIT_ORDER = MSBFIRST;  
const uint8_t SPI_MODE = SPI_MODE1;  

// Print Register Values
void printRegisterValue(uint8_t address, uint8_t value) {
    Serial.print("Reg 0x");
    if (address < 0x10) Serial.print("0");  // Pad single-digit HEX
    Serial.print(address, HEX);
    Serial.print(" = 0x");
    if (value < 0x10) Serial.print("0");
    Serial.println(value, HEX);
}

// WREG
void WREG(uint8_t register_address, uint8_t register_num, uint8_t* register_data) {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));

    uint8_t cb1 = (_WREG) | register_address;  
    uint8_t cb2 = register_num - 1;

    digitalWrite(CS, LOW);  // Enable Chip
    SPI.transfer(_SDATAC);  // Stop Read Data Continuous
    SPI.transfer(cb1);
    SPI.transfer(cb2);
    for (int i = 0; i <= cb2; i++) {
        SPI.transfer(register_data[i]);
        printRegisterValue(register_address + i, register_data[i]);
    }
    digitalWrite(CS, HIGH);
    delayMicroseconds(10);  // CS recovery time

    SPI.endTransaction();
}

// RREG
void RREG(uint8_t register_address, uint8_t register_num) {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));

    uint8_t cb1 = (_RREG) | register_address;  // Command byte: RREG + address
    uint8_t cb2 = register_num - 1;            // Number of registers to read - 1

    digitalWrite(CS, LOW);  // Select ADS1299
    SPI.transfer(_SDATAC);  // Stop Read Data Continuous
    SPI.transfer(cb1);
    SPI.transfer(cb2);

    Serial.println("\n--- Register Read ---");
    for (int i = 0; i <= cb2; i++) {
        uint8_t data = SPI.transfer(0x00);  // Read register
        printRegisterValue(register_address + i, data);
    }
    digitalWrite(CS, HIGH);  // Deselect ADS1299
    delayMicroseconds(10);   // Short delay for stability
    SPI.endTransaction();
}

// Read Data Commands
void RDATAC() {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);
    SPI.transfer(_RDATAC);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void SDATAC() {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);
    SPI.transfer(_SDATAC);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void RDATA() {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);

    // Send RDATA command (0x12)
    SPI.transfer(_RDATA);

    // Read status byte (optional but recommended)
    uint8_t status = SPI.transfer(0x00); // Dummy write to read status
    Serial.print("Status: 0x"); Serial.println(status, HEX);

    // Read 24-bit data for each channel (assuming 8 channels)
    for (int i = 0; i < 24; i++) { // 8 channels × 3 bytes each
        uint8_t data_byte = SPI.transfer(0x00); // Dummy write to read data
        Serial.print("Byte "); Serial.print(i); 
        Serial.print(": 0x"); Serial.println(data_byte, HEX);
    }

    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

// System Commands
void WAKEUP() {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);
    SPI.transfer(_WAKEUP);
    digitalWrite(CS, HIGH);
    delayMicroseconds(4);  // Must wait at least 4 tCLK cycles (Datasheet, pg. 35)
    SPI.endTransaction();
}

void STANDBY() {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);
    SPI.transfer(_STANDBY);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void RESET_IC() {
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);
    SPI.transfer(_RESET);
    digitalWrite(CS, HIGH);
    delay(10);  // Reset delay (Datasheet requirement)
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

void setup() {
    Serial.begin(115200);
    SPI.begin(18,19,23,5);
    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);  // Deselect ADS1299 initially

    // Declaring Pin Modes for GPIOs
    pinMode(PWDN, OUTPUT);
    pinMode(START, OUTPUT);
    pinMode(DRDY, INPUT);
    pinMode(RESET, OUTPUT);
    pinMode(CLKSEL, OUTPUT);

    // Default Values
    digitalWrite(CLKSEL, HIGH);
    digitalWrite(START, HIGH);
    digitalWrite(PWDN, HIGH);
    digitalWrite(RESET, HIGH);


    WAKEUP();
    RESET_IC();
    uint8_t config1[1] = {0xB5};
    WREG(CONFIG1, 1, config1);
    delay(10);

    // Starting Device
    RREG(ID, 1);

    // Setting up registers
    uint8_t config2[1] = {0xD5};
    WREG(CONFIG2, 1, config2);
    uint8_t regData[8] = {0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45}; // Array of values
    WREG(CH1SET, 8, regData); 
    delayMicroseconds(50);

    Serial.println("This is first check for test signal: ");
    START_IC();
    delay(100);
    RDATA();
    delay(100);
    STOP();
    digitalWrite(PWDN, LOW);
}

void loop() {
}   