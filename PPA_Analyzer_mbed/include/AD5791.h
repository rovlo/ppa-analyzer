#include "mbed.h"

#ifndef __MBED_AD5791
#define __MBED_AD5791

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')

class AD57X1
{
public:
    AD57X1(PinName cs_pin, SPI &spi, const uint8_t VALUE_OFFSET, uint32_t spiClockFrequency);//, PinName ldac_pin);
    void setValue(uint32_t value);
    void enableOutput();
    void setOffsetBinaryEncoding(bool enable);
    void setInternalAmplifier(bool enable);
    void setOutputClamp(bool enable);
    void setTristateMode(bool enable);
    void setReferenceInputRange(bool enable);
    void updateControlRegister();
    void setClearCodeValue(uint32_t value);
    void reset();
    void begin();

private:
    const uint8_t _VALUE_OFFSET;
    SPI &_spi;
    //DigitalOut _ldac_pin;
    DigitalOut _cs_pin;

protected:
    // DAC read/write mode
    static const uint32_t WRITE_REGISTERS = 0UL << 23;
    static const uint32_t READ_REGISTERS = 1UL << 23;

    // DAC value register (p. 21)
    static const uint32_t DAC_REGISTER = 0b001UL << 20;

    // Control register (p. 22)
    static const uint32_t CONTROL_REGISTER = 0b010UL << 20;
    static const uint8_t RBUF_REGISTER = 1;
    static const uint8_t OUTPUT_CLAMP_TO_GND_REGISTER = 2;
    static const uint8_t OUTPUT_TRISTATE_REGISTER = 3;
    static const uint8_t OFFSET_BINARY_REGISTER = 4;
    static const uint8_t SDO_DISABLE_REGISTER = 5;
    static const uint8_t LINEARITY_COMPENSATION_REGISTER = 6;

    static const uint32_t REFERENCE_RANGE_10V = 0b0000;
    static const uint32_t REFERENCE_RANGE_20V = 0b1100;

    // Clearcode register (p. 22)
    static const uint32_t CLEARCODE_REGISTER = 0b011UL << 20;

    // Software control register (p. 23)
    static const uint32_t SW_CONTROL_REGISTER = 0b100UL << 20;
    static const uint8_t SW_CONTROL_LDAC = 0b001;

    void write(uint32_t value);

    //uint8_t cs_pin;   // The ~Chip select pin used to address the DAC
    //int16_t ldac_pin; // The ~LDAC select pin used to address the DAC
    uint32_t controlRegister =
        (1 << RBUF_REGISTER) | (1 << OUTPUT_CLAMP_TO_GND_REGISTER) | (1 << OUTPUT_TRISTATE_REGISTER) | (1 << OFFSET_BINARY_REGISTER) | (0 << SDO_DISABLE_REGISTER) | (REFERENCE_RANGE_10V << LINEARITY_COMPENSATION_REGISTER); // This is the default register after reset (see p. 22 of the datasheet)
};

class AD5791: public AD57X1 {
  public:
    AD5791(PinName cs_pin, SPI& spi, uint32_t spiClockFrequency=1000000) /**PinMode ldac_pin=-1)**/ : AD57X1(cs_pin, spi, 0, spiClockFrequency)/**, ldac_pin)**/ {}
};

#endif