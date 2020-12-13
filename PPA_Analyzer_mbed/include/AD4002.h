#include "mbed.h"

#ifndef MBED_AD4002_H
#define MBED_AD4002_H


class AD4002
{
public:
    AD4002(PinName cs_pin, SPI &spi, uint32_t spiClockFrequency);
    uint32_t readValue();

private:
    SPI &_spi;
    DigitalOut _cs_pin;
    uint32_t _spiClockFrequency;
};

#endif