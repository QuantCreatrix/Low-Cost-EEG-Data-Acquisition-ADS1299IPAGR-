#include <SPI.h>

// Command Definitions
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
#define ID       0x00
#define CONFIG1  0x01
#define CONFIG2  0x02
#define CONFIG3  0x03
#define CH1SET   0x04
#define CH2SET   0x05
#define CH3SET   0x06
#define CH4SET   0x07
#define CH5SET   0x08
#define CH6SET   0x09
#define CH7SET   0x0A
#define CH8SET   0x0B
#define MISC1    0x15

// Pin Definitions
#define CS      5    // Chip Select
#define PWDN    22   // Power Down
#define RESET   16   // Reset
#define START   21   // Start Conversion
#define DRDY    17   // Data Ready
#define CLKSEL  4    // Clock Select

// SPI Settings
const uint32_t SPI_CLOCK = 1000000;  // 1 MHz
const uint8_t BIT_ORDER = MSBFIRST;
const uint8_t SPI_MODE = SPI_MODE1;

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
    
    uint8_t cmd_byte = _WREG | reg_address;
    uint8_t num_regs = reg_num - 1;
    
    SPI.transfer(_SDATAC);
    SPI.transfer(cmd_byte);
    SPI.transfer(num_regs);
    
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
    
    uint8_t cmd_byte = _RREG | reg_address;
    uint8_t num_regs = reg_num - 1;
    
    SPI.transfer(_SDATAC);
    SPI.transfer(cmd_byte);
    SPI.transfer(num_regs);
    
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
    // Wait for data ready
    while (digitalRead(DRDY) == HIGH) {
        delayMicroseconds(1);
    }

    SPI.beginTransaction(SPISettings(SPI_CLOCK, BIT_ORDER, SPI_MODE));
    digitalWrite(CS, LOW);
    
    // Send RDATA command
    SPI.transfer(_RDATA);
    
    // Read status byte
    uint8_t status = SPI.transfer(0x00);
    Serial.print("\nStatus: 0x"); 
    if (status < 0x10) Serial.print("0");
    Serial.println(status, HEX);
    
    // Read channel data (8 channels × 3 bytes)
    for (int ch = 0; ch < 8; ch++) {
        uint8_t byte1 = SPI.transfer(0x00);
        uint8_t byte2 = SPI.transfer(0x00);
        uint8_t byte3 = SPI.transfer(0x00);
        
        // Print in hex format only
        Serial.print("CH"); Serial.print(ch + 1); 
        Serial.print(": 0x");
        if (byte1 < 0x10) Serial.print("0");
        Serial.print(byte1, HEX);
        if (byte2 < 0x10) Serial.print("0");
        Serial.print(byte2, HEX);
        if (byte3 < 0x10) Serial.print("0");
        Serial.println(byte3, HEX);
    }
    
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void initializeADS1299() {
    // Hardware reset
    digitalWrite(PWDN, HIGH);
    delay(1);
    digitalWrite(RESET, LOW);
    delay(1);
    digitalWrite(RESET, HIGH);
    delay(100);
    
    // Software reset
    RESET_IC();
    WAKEUP();
    
    // Configure registers
    uint8_t config1[1] = {0x96};  // 250 SPS, daisy-chain off
    WREG(CONFIG1, 1, config1);
    
    uint8_t config2[1] = {0xC0};  // Test signal disabled
    WREG(CONFIG2, 1, config2);
    
    uint8_t config3[1] = {0xEC};  // Internal reference, SRB1 enabled
    WREG(CONFIG3, 1, config3);
    
    uint8_t misc1[1] = {0x20};    // SRB2 disconnected
    WREG(MISC1, 1, misc1);
    
    // Configure channels: gain=24, SRB2=0, normal input
    uint8_t chnset[8] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60};
    WREG(CH1SET, 8, chnset);
    
    // Verify configuration
    RREG(CONFIG1, 3);
    RREG(CH1SET, 8);
    RREG(MISC1, 1);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    // Initialize SPI
    SPI.begin(18, 19, 23, 5);
    
    // Configure GPIO pins
    pinMode(CS, OUTPUT);
    pinMode(PWDN, OUTPUT);
    pinMode(RESET, OUTPUT);
    pinMode(START, OUTPUT);
    pinMode(DRDY, INPUT);
    pinMode(CLKSEL, OUTPUT);
    
    // Set default states
    digitalWrite(CS, HIGH);
    digitalWrite(PWDN, HIGH);
    digitalWrite(RESET, HIGH);
    digitalWrite(START, LOW);
    digitalWrite(CLKSEL, HIGH);
    
    // Initialize ADS1299
    initializeADS1299();
    
    Serial.println("\nADS1299 Initialized. Starting data collection...");
    START_IC();
}

void loop() {
    RDATA();
    delay(100);
}