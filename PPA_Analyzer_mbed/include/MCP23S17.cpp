/* mbed MCP23S17 Library, for driving the MCP23S17 16-Bit I/O Expander with Serial Interface (SPI)
 * Copyright (c) 2015, Created by Steen Joergensen (stjo2809) inspired by Romilly Cocking MCP23S17 library
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
 
 #include "mbed.h"
 #include "MCP23S17.h"
 
//=============================================================================
// Public functions
//=============================================================================

    MCP23S17::MCP23S17(int hardwareaddress, SPI& spi, PinName nCs, PinName nReset) : _hardwareaddress(hardwareaddress), _spi(spi), _nCs(nCs), _nReset(nReset)
    {
        _make_opcode(_hardwareaddress);
        _initialization();        
    }
    
    MCP23S17::MCP23S17(int hardwareaddress, SPI& spi, PinName nCs) : _hardwareaddress(hardwareaddress), _spi(spi), _nCs(nCs), _nReset(NC) // _nReset(NC) added for the compiler
    {
        _make_opcode(_hardwareaddress);
        _initialization();      
    }
    
    char MCP23S17::read(char reg_address)
    {
        return _read(reg_address);    
    }

    void MCP23S17::write(char reg_address, char data)
    {
        _write(reg_address, data);    
    }
    
    void MCP23S17::bit(char reg_address, int bitnumber, bool high_low)
    {
        char i; 
        
        if(bitnumber >= 1 || bitnumber <= 8)
        {
            if(high_low == 1)
            {
                i = _read(reg_address); 
                i = i | (0x01 << (bitnumber-1));
                _write(reg_address, i);
            }
            if(high_low == 0)
            {
                i = _read(reg_address);
                i = i & ~(0x01 << (bitnumber-1));
                _write(reg_address, i);    
            } 
        }
    }

    void MCP23S17::reset()
    {
        _nReset = 0;
        wait_us(5);
        _nReset = 1;
        _initialization();
    }
    
    char MCP23S17::iodira()
    {
        return _read(IODIRA_ADDR);     
    }

    void MCP23S17::iodira(char data)
    {
        _write(IODIRA_ADDR, data);    
    }
   
    char MCP23S17::iodirb()
    {
        return _read(IODIRB_ADDR);    
    }
   
    void MCP23S17::iodirb(char data)
    {
        _write(IODIRB_ADDR, data);     
    }
      
    char MCP23S17::ipola()
    {
        return _read(IPOLA_ADDR);      
    }
   
    void MCP23S17::ipola(char data)   
    {
        _write(IPOLA_ADDR, data);    
    }
       
    char MCP23S17::ipolb()
    {
        return _read(IPOLB_ADDR);     
    }
   
    void MCP23S17::ipolb(char data)
    {
         _write(IPOLB_ADDR, data);   
    }
       
    char MCP23S17::gpintena()
    {
        return _read(GPINTENA_ADDR);     
    }
   
    void MCP23S17::gpintena(char data)
    {
         _write(GPINTENA_ADDR, data);   
    }
   
    char MCP23S17::gpintenb()
    {
        return _read(GPINTENB_ADDR);     
    }
   
    void MCP23S17::gpintenb(char data)
    {
         _write(GPINTENB_ADDR, data);   
    }
    
    char MCP23S17::defvala()
    {
        return _read(DEFVALA_ADDR);     
    }
   
    void MCP23S17::defvala(char data)
    {
        _write(DEFVALA_ADDR, data);    
    }
       
    char MCP23S17::defvalb()
    {
        return _read(DEFVALB_ADDR);     
    }
   
    void MCP23S17::defvalb(char data) 
    {
         _write(DEFVALB_ADDR, data);   
    }
       
    char MCP23S17::intcona()
    {
        return _read(INTCONA_ADDR);     
    }
   
    void MCP23S17::intcona(char data)
    {
        _write(INTCONA_ADDR, data);    
    }
       
    char MCP23S17::intconb()
    {
        return _read(INTCONB_ADDR);     
    }
   
    void MCP23S17::intconb(char data)
    {
         _write(INTCONB_ADDR, data);   
    }
       
    char MCP23S17::iocon()
    {
        return _read(IOCON_ADDR);     
    }
   
    void MCP23S17::iocon(char data)
    {
         _write(IOCON_ADDR, data);   
    }
       
    char MCP23S17::gppua()
    {
        return _read(GPPUA_ADDR);     
    }
   
    void MCP23S17::gppua(char data)
    {
         _write(GPPUA_ADDR, data);   
    }
   
    char MCP23S17::gppub()
    {
        return _read(GPPUB_ADDR);    
    }
   
    void MCP23S17::gppub(char data)
    {
        _write(GPPUB_ADDR, data);    
    }
   
    char MCP23S17::intfa()
    {
        return _read(INTFA_ADDR);     
    }
       
    char MCP23S17::intfb()
    {
        return _read(INTFB_ADDR);    
    }
       
    char MCP23S17::intcapa()
    {
        return _read(INTCAPA_ADDR);    
    }
       
    char MCP23S17::intcapb()     
    {
        return _read(INTCAPB_ADDR);     
    }
            
    /**char MCP23S17::gpioa()
    {
        return _read(GPIOA_ADDR);     
    }**/
   
    void MCP23S17::gpioa(char data)
    {
        //printf("Entered gpioa write function\n");
        _write(GPIOA_ADDR, data);    
    }
       
    char MCP23S17::gpiob()
    {
        return _read(GPIOB_ADDR);     
    }
   
    void MCP23S17::gpiob(char data)
    {
         _write(GPIOB_ADDR, data);   
    }
       
    char MCP23S17::olata()
    {
        return _read(OLATA_ADDR);    
    }
   
    void MCP23S17::olata(char data)
    {
        _write(OLATA_ADDR, data);    
    }
       
    char MCP23S17::olatb()
    {
        return _read(OLATB_ADDR);    
    }
   
    void MCP23S17::olatb(char data)
    {
        _write(OLATB_ADDR, data);    
    }
       
    void MCP23S17::intmirror(bool mirror)
    {
        char kopi_iocon = _read(IOCON_ADDR);
        if (mirror)
        {
            kopi_iocon = kopi_iocon | INTERRUPT_MIRROR_BIT;
        } 
        else
        {
            kopi_iocon = kopi_iocon & ~INTERRUPT_MIRROR_BIT;
        }
        _write(IOCON_ADDR, kopi_iocon);
    }
       
    void MCP23S17::intpol(bool polarity)
    {
        char kopi_iocon = _read(IOCON_ADDR);
        if (polarity == false)
        {
            kopi_iocon = kopi_iocon | INTERRUPT_POLARITY_BIT;
        } 
        else
        {
            kopi_iocon = kopi_iocon & ~INTERRUPT_POLARITY_BIT;
        }
        _write(IOCON_ADDR, kopi_iocon);   
    }
       
//=============================================================================
// Private functions
//=============================================================================

    void MCP23S17::_initialization()
    {
        _write(IOCON_ADDR, 0x2A); // setup af control register (BANK = 0, MIRROR = 0, SEQOP = 1, DISSLW = 0, HAEN = 1, ODR = 0, INTPOL = 1, NC = 0)
        _nCs = 1; 
    }
    
    void MCP23S17::_make_opcode(int _hardwareaddress)
    {
        switch(_hardwareaddress)
        {
            case 0:
            _writeopcode = 0x40;
            _readopcode = 0x41;
            break;

            case 1:
            _writeopcode = 0x42;
            _readopcode = 0x43;
            break;
            
            case 2:
            _writeopcode = 0x44;
            _readopcode = 0x45;
            break;
            
            case 3:
            _writeopcode = 0x46;
            _readopcode = 0x47;
            break;
            
            case 4:
            _writeopcode = 0x48;
            _readopcode = 0x49;
            break;                        
                        
            case 5:
            _writeopcode = 0x4A;
            _readopcode = 0x4B;
            break;
           
            case 6:
            _writeopcode = 0x4C;
            _readopcode = 0x4D;
            break;
            
            case 7:
            _writeopcode = 0x4E;
            _readopcode = 0x4F;
            break;                                
        }
    }
    
    char MCP23S17::_read(char address)                         
    {
        _nCs = 0;
        _spi.write(_readopcode);
        _spi.write(address);
        char response = _spi.write(0xFF);                      // 0xFF data to get response
        _nCs = 1;
        return response;
    }
                                    
    void MCP23S17::_write(char address, char data)             
    {
     //   printf("Entered _write function \n");
     //   printf("Address: %u Data: %u \n", address, data);
        _nCs = 0;
     //   printf("nCs set to 0 \n");
        _spi.write(_writeopcode);
     //   printf("Wrote op code \n");
        _spi.write(address);
      //  printf("Wrote address code \n");
        _spi.write(data);
      //  printf("Wrote data code \n");
        _nCs = 1;
      //  printf("Exiting _write function \n");
    }

