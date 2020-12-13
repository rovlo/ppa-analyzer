#include "mbed.h"
#include "AD4002.h"

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


AD4002::AD4002(PinName cs_pin, SPI &spi, uint32_t spiClockFrequency): _cs_pin(cs_pin), _spi(spi), _spiClockFrequency(spiClockFrequency)
{
    _spi.format(8, 1);
}

uint32_t AD4002::readValue(){
    uint8_t readByte[3];
    uint32_t convValue = 0;
    _cs_pin = 0;
    wait_us(1);
    _cs_pin = 1;

    wait_us(1);
    _cs_pin = 0;
    wait_us(1);
    readByte[0] = (_spi.write(0x00));
    wait_us(1);
    readByte[1] = (_spi.write(0x00));
    wait_us(1);
    readByte[2] = (_spi.write(0x00));
    wait_us(1);
    _cs_pin = 1;
    printf("Read value: "BYTE_TO_BINARY_PATTERN""BYTE_TO_BINARY_PATTERN""BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY((char) readByte[0]), BYTE_TO_BINARY((char)readByte[1]), BYTE_TO_BINARY((char)readByte[2]));
    wait_us(100);
    convValue = (readByte[0] << 10) | (readByte[1] << 2) | ((readByte[2] & 0b11000000) >> 6);
    

    return convValue;
}