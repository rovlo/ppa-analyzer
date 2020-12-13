#include <stdio.h>

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

#include "platform/Callback.h"
#include "events/EventQueue.h"
#include "platform/NonCopyable.h"
#include <mbed.h>
#include "ble/GattServer.h"
#include "BLEProcess.h"
#include "MCP23S17.h"
#include "mbed_trace.h"
#include "mbedtls/debug.h"
#include "aws-credentials.h"
#include "AD5791.h"
#include "AD4002.h"

#define WAITING_TIME 100000

extern "C"
{
// sdk initialization
#include "iot_init.h"
// mqtt methods
#include "iot_mqtt.h"
}
//#include "aws-mqtt-function.h"

using mbed::callback;

//SPI _spi2(PD_4, PD_3, PD_1);                        // SPI2: mosi, miso, sclk
SPI _spi1(D11, D12, D13);                 // SPI1:  mosi, miso, sclk
MCP23S17 _MCP23S17_chip(0x00, _spi1, D4); //spi expander
AD5791 _AD5791_chip(D6, _spi1);           //AD5791(SPI &spi, PinName cs_pin, PinName ldac_pin, const uint8_t VALUE_OFFSET, uint32_t spiClockFrequency)
AD4002 ad4002(D3, _spi1, 1000000);
AnalogOut _aout(D7);
DigitalOut _switch(D2);
AnalogIn _ain(A5);
//DigitalOut cs(PD_2);
//DigitalOut _nfc_disable(PE_2);
//DigitalOut _spsgrf_disable(PB_15);

// debugging facilities
#define TRACE_GROUP "Main"
static Mutex trace_mutex;
static void trace_mutex_lock()
{
    trace_mutex.lock();
}
static void trace_mutex_unlock()
{
    trace_mutex.unlock();
}
extern "C" void aws_iot_puts(const char *msg)
{
    trace_mutex_lock();
    puts(msg);
    trace_mutex_unlock();
}

#define MQTT_TIMEOUT_MS 15000

/**
 * A Clock service that demonstrate the GattServer features.
 *
 * The clock service host three characteristics that model the current hour,
 * minute and second of the clock. The value of the second characteristic is
 * incremented automatically by the system.
 *
 * A client can subscribe to updates of the clock characteristics and get
 * notified when one of the value is changed. Clients can also change value of
 * the second, minute and hour characteristric.
 */
/**
void set_resistance_fn(char value){
    _event_queue.call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) value);
}**/

class PPAServices
{
    typedef PPAServices Self;

public:
    PPAServices() : _hour_char("485f4145-52b9-4644-af1f-7a6b9322490f", 0),
                    _minute_char("0a924ca7-87cd-4699-a3bd-abdcd9cf126a", 0),
                    _second_char("8dd6a1b7-bc75-4741-8a26-264af75807de", 0),
                    _readout_char("7a28c888-02f9-11eb-adc1-0242ac120002", readout_array, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE),
                    _sample_rate_char("3ecc08da-3a09-11eb-adc1-0242ac120002", 0),
                    _DACexternal_char("711d90da-0334-11eb-adc1-0242ac120002", 0),
                    _DACinternal_char("f3cd69ee-265f-11eb-adc1-0242ac120002", 0),
                    _resvalue_char("7b26228e-0ef8-11eb-adc1-0242ac120002", 0),
                    _clock_service(
                        /* uuid */ "51311102-030e-485f-b122-f8f381aa84ed",
                        /* characteristics */ _clock_characteristics,
                        /* numCharacteristics */ sizeof(_clock_characteristics) /
                            sizeof(_clock_characteristics[0])),
                    _PPA_service(
                        /* uuid */ "10cb4c5c-278f-11eb-adc1-0242ac120002",
                        /* characteristics */ _PPA_characteristics,
                        /* numCharacteristics */ sizeof(_PPA_characteristics) /
                            sizeof(_PPA_characteristics[0])),
                    _server(NULL),
                    _event_queue(NULL)
    {
        // update internal pointers (value, descriptors and characteristics array)
        _clock_characteristics[0] = &_hour_char;
        _clock_characteristics[1] = &_minute_char;
        _clock_characteristics[2] = &_second_char;
        _PPA_characteristics[0] = &_resvalue_char;
        _PPA_characteristics[1] = &_readout_char;
        _PPA_characteristics[2] = &_DACexternal_char;
        _PPA_characteristics[3] = &_DACinternal_char;
        _PPA_characteristics[4] = &_sample_rate_char;

        _MCP23S17_chip.iodira(0x00);
        _MCP23S17_chip.iodirb(0xFF);

        sample_time_per_step = 50;

        printf("Initialized successfully\n");

        // setup clock authorization handlers
        _hour_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
        _minute_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
        _second_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);

        // setup PPA authorization handlers
        _readout_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
        _DACexternal_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
        _DACinternal_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
        _resvalue_char.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
    }

    void start(BLE &ble_interface, events::EventQueue &event_queue)
    {
        if (_event_queue)
        {
            return;
        }

        _server = &ble_interface.gattServer();
        _event_queue = &event_queue;

        // register the service
        printf("Adding clock service\r\n");
        ble_error_t err = _server->addService(_clock_service);

        if (err)
        {
            printf("Error %u during clock service registration.\r\n", err);
            return;
        }

        err = _server->addService(_PPA_service);

        if (err)
        {
            printf("Error %u during clock service registration.\r\n", err);
            return;
        }

        // read write handler
        _server->onDataSent(as_cb(&Self::when_data_sent));
        _server->onDataWritten(as_cb(&Self::when_data_written));
        _server->onDataRead(as_cb(&Self::when_data_read));

        // updates subscribtion handlers
        _server->onUpdatesEnabled(as_cb(&Self::when_update_enabled));
        _server->onUpdatesDisabled(as_cb(&Self::when_update_disabled));
        _server->onConfirmationReceived(as_cb(&Self::when_confirmation_received));

        // print the handles
        printf("clock service registered\r\n");
        printf("clock service handle: %u\r\n", _clock_service.getHandle());
        printf("PPA service handle %u\r\n", _PPA_service.getHandle());
        printf("\thour characteristic value handle %u\r\n", _hour_char.getValueHandle());
        printf("\tminute characteristic value handle %u\r\n", _minute_char.getValueHandle());
        printf("\tsecond characteristic value handle %u\r\n", _second_char.getValueHandle());
        printf("\treadout characteristic value handle %u\r\n", _readout_char.getValueHandle());
        printf("\tDACexternal characteristic value handle %u\r\n", _DACexternal_char.getValueHandle());
        printf("\tDACinternal characteristic value handle %u\r\n", _DACinternal_char.getValueHandle());
        printf("\tresvalue characteristic value handle %u\r\n", _resvalue_char.getValueHandle());
        printf("\tsamplerate characteristic value handle %u\r\n", _sample_rate_char.getValueHandle());

        //_event_queue->call_every(1000 /* ms */, callback(this, &Self::increment_second));
        //_event_queue->call_every(1000 /* ms */, callback(&_MCP23S17_chip, &MCP23S17::gpioa));
    }

private:
    /**
     * Handler called when a notification or an indication has been sent.
     */
    void when_data_sent(unsigned count)
    {
        printf("sent %u updates\r\n", count);
    }

    /**
     * Handler called after an attribute has been written.
     */
    void when_data_written(const GattWriteCallbackParams *e)
    {
        printf("data written:\r\n");
        printf("\tconnection handle: %u\r\n", e->connHandle);
        printf("\tattribute handle: %u", e->handle);
        if (e->handle == _hour_char.getValueHandle())
        {
            printf(" (hour characteristic)\r\n");
        }
        else if (e->handle == _minute_char.getValueHandle())
        {
            printf(" (minute characteristic)\r\n");
        }
        else if (e->handle == _second_char.getValueHandle())
        {
            printf(" (second characteristic)\r\n");
        }
        else if (e->handle == _readout_char.getValueHandle())
        {
            printf(" (readout characteristic)\r\n");
        }
        else if (e->handle == _sample_rate_char.getValueHandle())
        {
            sample_rate = ((uint16_t)e->data[0]) * sample_time_per_step;
            printf(" (sampletime characteristic)\r\n");
        }
        else if (e->handle == _DACexternal_char.getValueHandle())
        {
            set_DAC_external(e->data[0]);
            printf(" (DAC external characteristic)\r\n");
        }
        else if (e->handle == _DACinternal_char.getValueHandle())
        {
            set_DAC_internal(e->data[0]);
            printf(" (DAC internal characteristic)\r\n");
        }
        else if (e->handle == _resvalue_char.getValueHandle())
        {
            printf("Entered if statement for resvalue \n");
            set_resistance(e->data[0]);
            printf(" (resvalue characteristic)\r\n");
        }
        else
        {
            printf("\r\n");
        }
        printf("\twrite operation: %u\r\n", e->writeOp);
        printf("\toffset: %u\r\n", e->offset);
        printf("\tlength: %u\r\n", e->len);
        printf("\t data: ");

        for (size_t i = 0; i < e->len; ++i)
        {
            printf("%02X", e->data[i]);
        }

        printf("\r\n");
    }

    /**
     * Handler called after an attribute has been read.
     */
    void when_data_read(const GattReadCallbackParams *e)
    {
        printf("data read:\r\n");
        printf("\tconnection handle: %u\r\n", e->connHandle);
        printf("\tattribute handle: %u", e->handle);
        if (e->handle == _hour_char.getValueHandle())
        {
            printf(" (hour characteristic)\r\n");
        }
        else if (e->handle == _minute_char.getValueHandle())
        {
            printf(" (minute characteristic)\r\n");
        }
        else if (e->handle == _second_char.getValueHandle())
        {
            printf(" (second characteristic)\r\n");
        }
        else
        {
            printf("\r\n");
        }
    }

    /**
     * Handler called after a client has subscribed to notification or indication.
     *
     * @param handle Handle of the characteristic value affected by the change.
     */
    void when_update_enabled(GattAttribute::Handle_t handle)
    {
        if ((handle - 1) == _readout_char.getValueHandle())
        {
            if (!readout_event_id)
            {
                readout_event_id = _event_queue->call_every(sample_rate/* ms */, callback(this, &Self::read_ADC));
            }
        }
        printf("update enabled on handle %d\r\n", handle);
    }

    /**
     * Handler called after a client has cancelled his subscription from
     * notification or indication.
     *
     * @param handle Handle of the characteristic value affected by the change.
     */
    void when_update_disabled(GattAttribute::Handle_t handle)
    {
        if ((handle - 1) == _readout_char.getValueHandle())
        {
            _event_queue->cancel(readout_event_id);
            readout_event_id = 0;
        }
        printf("update disabled on handle %d\r\n", handle);
    }

    /**
     * Handler called when an indication confirmation has been received.
     *
     * @param handle Handle of the characteristic value that has emitted the
     * indication.
     */
    void when_confirmation_received(GattAttribute::Handle_t handle)
    {
        printf("confirmation received on handle %d\r\n", handle);
    }

    /**
     * Handler called when a write request is received.
     *
     * This handler verify that the value submitted by the client is valid before
     * authorizing the operation.
     */
    void authorize_client_write(GattWriteAuthCallbackParams *e)
    {
        printf("characteristic %u write authorization\r\n", e->handle);

        if (e->offset != 0)
        {
            printf("Error invalid offset\r\n");
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
            return;
        }

        if (e->len != 1)
        {
            printf("Error invalid len\r\n");
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
            return;
        }

        if ((e->data[0] > 0xFF) ||
            ((e->data[0] >= 24) && (e->handle == _hour_char.getValueHandle())))
        {
            printf("Error invalid data\r\n");
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
            return;
        }

        e->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }

    /**
     * Increment the second counter.
     */
    void increment_second(void)
    {
        uint8_t second = 0;
        ble_error_t err = _second_char.get(*_server, second);
        if (err)
        {
            printf("read of the second value returned error %u\r\n", err);
            return;
        }

        second = (second + 1) % 60;

        err = _second_char.set(*_server, second);
        if (err)
        {
            printf("write of the second value returned error %u\r\n", err);
            return;
        }

        if (second == 0)
        {
            increment_minute();
        }
        //(*_event_queue).call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) 0xFF);
        read_ADC();
    }

    /**
     * Increment the minute counter.
     */
    void increment_minute(void)
    {
        uint8_t minute = 0;
        ble_error_t err = _minute_char.get(*_server, minute);
        if (err)
        {
            printf("read of the minute value returned error %u\r\n", err);
            return;
        }

        minute = (minute + 1) % 60;

        err = _minute_char.set(*_server, minute);
        if (err)
        {
            printf("write of the minute value returned error %u\r\n", err);
            return;
        }

        if (minute == 0)
        {
            increment_hour();
        }
    }

    /**
     * Increment the hour counter.
     */
    void increment_hour(void)
    {
        uint8_t hour = 0;
        ble_error_t err = _hour_char.get(*_server, hour);
        if (err)
        {
            printf("read of the hour value returned error %u\r\n", err);
            return;
        }

        hour = (hour + 1) % 24;

        err = _hour_char.set(*_server, hour);
        if (err)
        {
            printf("write of the hour value returned error %u\r\n", err);
            return;
        }
    }

    void read_ADC(void)
    {
        uint8_t readout;
        float output_voltage;
        float gain = 2;
        float readout_float = 0;
        uint32_t readout_long = 0;
        float current;

        uint16_t value_length = sizeof(readout_array);
        _server->read(_readout_char.getValueHandle(), readout_array, &value_length);

        /**
        ble_error_t err = _readout_char.get(*_server, readout);
        if (err)
        {
            printf("read of the readout value returned error %u\r\n", err);
            return;
        }**/

        counter++;
        /**
        for(int i = 0; i < 33; i++){
            _aout.write((float) i/33);
            wait_us(100000);
            readout_long = (float) ad4002.readValue();
            printf("Readout value ofis %f\n", readout_long);
        }**/

        //printf("%d\n", counter);
        wait_us(1000);
        readout_long = ad4002.readValue();
        readout_float = (float)readout_long;
        wait_us(1000);
        printf("Readout value of is %f\n", readout_float);
        current = (readout_float / 262144 * 5.0);
        printf("Current voltage of external ADC is: %.3f mV, current is then: $%.3f; uA\n", current * 1000, current / (gain * 1000) * 1000000);
        //current = (_ain.read()*3.3);// / (gain * 1000);
        //printf("Current voltage of internal ADC is: $%.2f; V\n", current);

        /**
        COOLING DOWN 
        
        if (counter < 60)
        {
            readout_long = (uint32_t) ad4002.readValue();
            printf("Readout value is %d\n", readout_long);
            current = (readout_long / 262144 * 5.0) / (gain * 1000);
            printf("Current is: $%.4f; uA\n", current * 1000000);
        }
        else if (counter == 60)
        {
            _aout.write(3.0/3.3);

            output_voltage = 2.0;
            uint32_t value = static_cast<uint32_t>(((output_voltage + 10.0) / 20.0) * 1048575.0);

            _AD5791_chip.setValue(value);
            wait_us(100000);
        }
        else if (counter > 60  && counter < 120)
        {
            readout_long = (float) ad4002.readValue();
            printf("Readout value is %d\n", readout_long);
            current = (readout_long / 262144 * 5.0) / (gain * 1000);
            printf("Current is: $%.4f; uA\n", current * 1000000);   
            return;
        }
        else
        {
            _aout.write(3.0/3.3);
            output_voltage = 2.0;
            uint32_t value = static_cast<uint32_t>(((output_voltage + 10.0) / 20.0) * 1048575.0);

            _AD5791_chip.setValue(value);
            wait_us(100000);
            counter = 0;
        }
        **/

        printf(BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY((uint8_t)((readout_long) >> 24)),
               BYTE_TO_BINARY((uint8_t)((readout_long) >> 16)), BYTE_TO_BINARY((uint8_t)((readout_long) >> 8)), BYTE_TO_BINARY((uint8_t)((readout_long) >> 0)));

        readout_array[0] = (uint8_t)((0x00030000 & readout_long) >> 16);
        readout_array[1] = (uint8_t)((0x0000FF00 & readout_long) >> 8);
        readout_array[2] = (uint8_t)((0x000000FF & readout_long) >> 0);

        _server->write(_readout_char.getValueHandle(), readout_array, sizeof(readout_array), false);

        /**
        err = _readout_char.set(*_server, readout);
        if (err)
        {
            printf("write of the readout value returned error %u\r\n", err);
            return;
        }**/
    }

    void set_resistance(uint8_t res_value)
    {
        if (res_value > 7 || res_value < 1)
        {
            printf("Res_value invalid\n");
            return;
        }
        char reset, res_value_inv;
        printf("Enterd set_resistance function\n");
        uint8_t res_value_current = 0;
        ble_error_t err = _resvalue_char.get(*_server, res_value_current);
        if (err)
        {
            printf("read of the bias value returned error %u\r\n", err);
            return;
        }
        printf("Arrived to gpioa setting\n");
        printf("Value to set is: %b\n", (uint8_t)res_value);

        printf("Erasing current value\n");
        for (int i = 0; i < 7; i++)
        {
            reset = (0b00000001 << i);
            //printf("Reset value: %b \n", reset);
            printf("Reset value inverted is " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(reset));
            //_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) reset);
            _MCP23S17_chip.gpioa(reset);
            wait_us(WAITING_TIME);
            //_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) 0x00);
            _MCP23S17_chip.gpioa(0x00);
            wait_us(WAITING_TIME);
            printf("current bit: %d\n", i);
        }

        res_value_inv = (0xFF ^ (0x01 << (res_value - 1)));
        printf("Res value inverted is " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(res_value_inv));
        wait_us(WAITING_TIME * 5);
        //_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, res_value_inv);
        _MCP23S17_chip.gpioa(res_value_inv);
        wait_us(WAITING_TIME);
        _MCP23S17_chip.gpioa(0xFF);
        wait_us(WAITING_TIME);

        //(*_event_queue).dispatch();
        //set_resistance_fn(res_value);
        ///_e_set_resistance.post(0x01);
        //_event_queue->dispatch(); // _MCP23S17_chip.gpioa(0x01);//_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) 1);
        printf("Resvalue set to %u", (uint8_t)res_value);
        err = _resvalue_char.set(*_server, res_value);
        if (err)
        {
            printf("write of the bias value returned error %u\r\n", err);
            return;
        }
    }

    void set_DAC_external(uint8_t bias_value)
    {
        float output_voltage;
        uint8_t biasing_point = 0;
        ble_error_t err = _DACexternal_char.get(*_server, biasing_point);
        if (err)
        {
            printf("read of the bias value returned error %u\r\n", err);
            return;
        }

        output_voltage = (float)bias_value / 255.0 * 10.0;
        uint32_t value = static_cast<uint32_t>(((output_voltage + 10.0) / 20.0) * 1048575.0);

        _AD5791_chip.setValue(value);
        wait_us(100000);

        err = _DACexternal_char.set(*_server, bias_value);
        if (err)
        {
            printf("write of the bias value returned error %u\r\n", err);
            return;
        }
    }

    void set_DAC_internal(uint8_t bias_value)
    {
        //float output_voltage;
        uint8_t biasing_point = 0;
        ble_error_t err = _DACinternal_char.get(*_server, biasing_point);
        if (err)
        {
            printf("read of the bias value returned error %u\r\n", err);
            return;
        }

        _aout.write_u16(((uint8_t)bias_value) << 8);

        err = _DACinternal_char.set(*_server, bias_value);
        if (err)
        {
            printf("write of the bias value returned error %u\r\n", err);
            return;
        }
    }

private:
    /**
     * Helper that construct an event handler from a member function of this
     * instance.
     */
    template <typename Arg>
    FunctionPointerWithContext<Arg> as_cb(void (Self::*member)(Arg))
    {
        return makeFunctionPointer(this, member);
    }

    /**
     * Read, Write, Notify, Indicate  Characteristic declaration helper.
     *
     * @tparam T type of data held by the characteristic.
     */
    template <typename T>
    class ReadWriteNotifyIndicateCharacteristic : public GattCharacteristic
    {
    public:
        /**
         * Construct a characteristic that can be read or written and emit
         * notification or indication.
         *
         * @param[in] uuid The UUID of the characteristic.
         * @param[in] initial_value Initial value contained by the characteristic.
         */
        ReadWriteNotifyIndicateCharacteristic(const UUID &uuid, const T &initial_value) : GattCharacteristic(
                                                                                              /* UUID */ uuid,
                                                                                              /* Initial value */ &_value,
                                                                                              /* Value size */ sizeof(_value),
                                                                                              /* Value capacity */ sizeof(_value),
                                                                                              /* Properties */ GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                                                                                                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
                                                                                                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                                                                                                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
                                                                                              /* Descriptors */ NULL,
                                                                                              /* Num descriptors */ 0,
                                                                                              /* variable len */ false),
                                                                                          _value(initial_value)
        {
        }

        /**
         * Get the value of this characteristic.
         *
         * @param[in] server GattServer instance that contain the characteristic
         * value.
         * @param[in] dst Variable that will receive the characteristic value.
         *
         * @return BLE_ERROR_NONE in case of success or an appropriate error code.
         */
        ble_error_t get(GattServer &server, T &dst) const
        {
            uint16_t value_length = sizeof(dst);
            return server.read(getValueHandle(), &dst, &value_length);
        }

        /**
         * Assign a new value to this characteristic.
         *
         * @param[in] server GattServer instance that will receive the new value.
         * @param[in] value The new value to set.
         * @param[in] local_only Flag that determine if the change should be kept
         * locally or forwarded to subscribed clients.
         */
        ble_error_t set(
            GattServer &server, uint8_t &value, bool local_only = false) const
        {
            return server.write(getValueHandle(), &value, sizeof(value), local_only);
        }

    private:
        uint8_t _value;
    };

    ReadWriteNotifyIndicateCharacteristic<uint8_t> _hour_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _minute_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _second_char;
    ReadWriteArrayGattCharacteristic<uint8_t, 3> _readout_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _sample_rate_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _DACexternal_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _DACinternal_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _resvalue_char;

    // list of the characteristics of the clock service
    GattCharacteristic *_clock_characteristics[3];
    GattCharacteristic *_PPA_characteristics[5];

    uint8_t readout_array[3];
    uint16_t counter;
    uint16_t sample_rate, sample_time_per_step;
    int readout_event_id = 0;
    // demo service
    GattService _clock_service;
    GattService _PPA_service;

    GattServer *_server;
    events::EventQueue *_event_queue;
};

/**void disablePeripherals(){
    _nfc_disable = 1;
    _spsgrf_disable = 1;
}**/

int main()
{
    wait_us(1000000);
    uint8_t reset, res_value_inv;
    uint8_t res_value = 1;
    float output_voltage; //for external DAC bcs app isn't updated
    Thread thread_ble;
    BLE &ble_interface = BLE::Instance();
    events::EventQueue event_queue;
    PPAServices demo_service;
    printf("PPA Service initialized\n");

    BLEProcess ble_process(event_queue, ble_interface);

    srand(5);

    mbed_trace_mutex_wait_function_set(trace_mutex_lock);      // only if thread safety is needed
    mbed_trace_mutex_release_function_set(trace_mutex_unlock); // only if thread safety is needed
    mbed_trace_init();

    ble_process.on_init(callback(&demo_service, &PPAServices::start));

    // bind the event queue to the ble interface, initialize the interface
    // and start advertising
    thread_ble.start(callback(&ble_process, &BLEProcess::start));

    //disablePeripherals();

    _aout.write(0);
    wait_us(1000000);

    _aout.write(2.0 / 3.3);

    _switch = 0;

    // Initialization of the AD5791 DAC

    _AD5791_chip.reset();
    wait_us(100);
    _AD5791_chip.begin();
    _spi1.frequency(1000000);
    wait_us(100);
    _AD5791_chip.setOffsetBinaryEncoding(true);
    wait_us(100);
    _AD5791_chip.enableOutput();
    wait_us(100000);

    // Dispatch the event queue containing BLE process forever

    //set external dac voltage

    output_voltage = 0.0;
    uint32_t value = static_cast<uint32_t>(((output_voltage + 10.0) / 20.0) * 1048575.0);

    _AD5791_chip.setValue(value);
    wait_us(100000);

    output_voltage = 3.0;
    value = static_cast<uint32_t>(((output_voltage + 10.0) / 20.0) * 1048575.0);

    _AD5791_chip.setValue(value);
    wait_us(100000);

    printf("Erasing current value\n");
    for (int i = 0; i < 7; i++)
    {
        reset = (0b00000001 << i);
        //printf("Reset value: %b \n", reset);
        printf("Reset value inverted is " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(reset));
        //_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) reset);
        _MCP23S17_chip.gpioa(reset);
        wait_us(WAITING_TIME);
        //_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, (char) 0x00);
        _MCP23S17_chip.gpioa(0x00);
        wait_us(WAITING_TIME);
        printf("current bit: %d\n", i);
    }

    res_value_inv = (0xFF ^ (0x01 << (res_value - 1)));
    printf("Res value inverted is " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(res_value_inv));
    //_event_queue->call(&_MCP23S17_chip, &MCP23S17::gpioa, res_value_inv);
    _MCP23S17_chip.gpioa(res_value_inv);
    wait_us(WAITING_TIME);
    _MCP23S17_chip.gpioa(0xFF);
    wait_us(WAITING_TIME);

    event_queue.dispatch_forever();

    return 0;
}