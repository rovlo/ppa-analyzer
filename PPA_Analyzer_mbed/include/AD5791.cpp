#include "mbed.h"
#include "AD5791.h"

AD57X1::AD57X1(PinName cs_pin, SPI &spi, const uint8_t VALUE_OFFSET, uint32_t spiClockFrequency) : _cs_pin(cs_pin), _spi(spi), _VALUE_OFFSET(VALUE_OFFSET)
{
}

void AD57X1::write(uint32_t value)
{
    _cs_pin = 0;
    char write_array[3] = {(char) ((value >> 16) & 0xFF), (char) ((value >> 8) & 0xFF), (char) ((value >> 0) & 0xFF)};
    char return_array[3] = {0, 0, 0};

    printf("Enter write function, value is: " BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(((value >> 16) & 0xFF)), BYTE_TO_BINARY(((value >> 8) & 0xFF)), BYTE_TO_BINARY(((value >> 0) & 0xFF)));
    _spi.format(8, 1);

    _spi.write(write_array, sizeof(write_array), return_array, sizeof(return_array));
    
    _cs_pin = 1;
}

void AD57X1::updateControlRegister()
{
    uint32_t value = this->WRITE_REGISTERS | this->CONTROL_REGISTER | this->controlRegister;
    this->write(value);
    printf("Enter write function, value is: " BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(((value >> 16) & 0xFF)), BYTE_TO_BINARY(((value >> 8) & 0xFF)), BYTE_TO_BINARY(((value >> 0) & 0xFF)));
}

void AD57X1::reset()
{
    this->write(0x800004);
    this->enableOutput();
}

// value is an 18 or 20 bit value
void AD57X1::setValue(uint32_t value)
{
    uint32_t command = this->WRITE_REGISTERS | this->DAC_REGISTER | ((value << this->_VALUE_OFFSET) & 0xFFFFF);
    printf("%d \n", command);
    this->write(command);
    this->write(this->SW_CONTROL_REGISTER | this->SW_CONTROL_LDAC);
}

void AD57X1::enableOutput()
{
    this->setOutputClamp(false);
    this->setTristateMode(false);
    this->updateControlRegister();
}

void AD57X1::setInternalAmplifier(bool enable)
{
    // (1 << this->RBUF_REGISTER) : internal amplifier is disabled (default)
    // (0 << this->RBUF_REGISTER) : internal amplifier is enabled
    this->controlRegister = (this->controlRegister & ~(1 << this->RBUF_REGISTER)) | (!enable << this->RBUF_REGISTER);
}

// Setting this to enabled will overrule the tristate mode and clamp the output to GND
void AD57X1::setOutputClamp(bool enable)
{
    // (1 << this->OUTPUT_CLAMP_TO_GND_REGISTER) : the output is clamped to GND (default)
    // (0 << this->OUTPUT_CLAMP_TO_GND_REGISTER) : the dac is in normal mode
    this->controlRegister = (this->controlRegister & ~(1 << this->OUTPUT_CLAMP_TO_GND_REGISTER)) | (enable << this->OUTPUT_CLAMP_TO_GND_REGISTER);
}

void AD57X1::setTristateMode(bool enable)
{
    // (1 << this->OUTPUT_TRISTATE_REGISTER) : the dac output is in tristate mode (default)
    // (0 << this->OUTPUT_TRISTATE_REGISTER) : the dac is in normal mode
    this->controlRegister = (this->controlRegister & ~(1 << this->OUTPUT_TRISTATE_REGISTER)) | (enable << this->OUTPUT_TRISTATE_REGISTER);
}

void AD57X1::setOffsetBinaryEncoding(bool enable)
{
    // (1 << this->OFFSET_BINARY_REGISTER) : the dac uses offset binary encoding, should be used when writing unsigned ints
    // (0 << this->OFFSET_BINARY_REGISTER) : the dac uses 2s complement encoding, should be used when writing signed ints (default)
    this->controlRegister = (this->controlRegister & ~(1 << this->OFFSET_BINARY_REGISTER)) | (enable << this->OFFSET_BINARY_REGISTER);
}

/* Linearity error compensation
 * 
 */
// enable = 0 -> Range 0-10 V
// enable = 1 -> Range 0-20 V
void AD57X1::setReferenceInputRange(bool enableCompensation)
{
    this->controlRegister = (this->controlRegister & ~(0b1111 << this->LINEARITY_COMPENSATION_REGISTER)) | ((enableCompensation ? this->REFERENCE_RANGE_20V : this->REFERENCE_RANGE_10V) << this->LINEARITY_COMPENSATION_REGISTER);
}

void AD57X1::begin()
{
    _cs_pin = 1;

    /** if (_ldac_pin >= 0)
    {
        _ldac_pin = 0;
    }**/
}
