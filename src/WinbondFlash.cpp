#include "WinbondFlash.h"

WinbondFlash& getFlashMemory()
{
    return flash;
}

/**
 * Initialise Chip Select pin to the right pin for the flash
 * Initialise write address to 0
*/
WinbondFlash::WinbondFlash(uint8_t csPin) : m_csPin(csPin), writeAddress(0) {}

/**
 * Initialises SPI communication with chip
 * Sends a Read ID command 0x9F to the chip si we can check its ID
*/
bool WinbondFlash::init() 
{
    pinMode(m_csPin, OUTPUT);
    deselectFlash();
    SPI.begin();

    int manufacturer;
    int memoryType;
    int capacity;

    readJedecID(manufacturer, memoryType, capacity);

    /**
     * 0xEF is Winbond
     * 0x40 is Serial flash
     * Capacity should be 16 -> 4MB
     * 
     * This is useful to check
     * IF these don't match what we are expected we might have damaged the chip or we have a wiring problem
    */
    if (manufacturer == 0xEF) 
    {
        // Serial.println("Flash memory detected.");
        return true;
    } 
    else 
    {
        // Serial.println("Failed to detect flash memory.");
        return false;
    }
}

/**
 * @brief 
*/
void WinbondFlash::readJedecID(int& JEDEC, int& memoryType, int& capacity)
{
    selectFlash();
    SPI.transfer(0x9F); 
    JEDEC = SPI.transfer(0);
    memoryType = SPI.transfer(0);
    capacity = SPI.transfer(0);
    deselectFlash();
}

/**
 * 
 * Writres an array of bytes to the current writeAddress
 * Sends a 0x02 page program command to write the data sequentially
 * Sends the address of where the data should be written to
 * Casts the address of array of structs of data to a pointer to an array of bytes
 * Then iterate over these bytes and send them over SPI to the chip
 * 
 * Does so in chunks of 256 bytes because this is the maximum number of bytes
 * that can be written per page program (0x02) command 
 * 
 * so every 256 bytes we have to deselect, write enable (includes select and deselect), select chip, 
 * send write command, send address and then write again
*/
void WinbondFlash::logData(const uint8_t* data, const size_t size) 
{
    size_t bytesWritten = 0;

    while (bytesWritten < size) 
    {
        size_t chunkSize = size - bytesWritten;
        if (chunkSize > 256) 
        {
            chunkSize = 256; 
        }

        writeEnable();
        selectFlash();

        SPI.transfer(0x02); 

        SPI.transfer((writeAddress >> 16) & 0xFF);
        SPI.transfer((writeAddress >> 8) & 0xFF);
        SPI.transfer(writeAddress & 0xFF);

        for (size_t i = 0; i < chunkSize; i++) 
        {
            SPI.transfer(data[bytesWritten + i]);
        }

        deselectFlash();
        waitForWriteCompletion();

        m_writeAddress += chunkSize;
        bytesWritten += chunkSize;
    }
}

/**
 * Wipes flash by sending a 0xC7 Chip Erase command
*/
void WinbondFlash::wipeFlash() 
{
    writeEnable();
    selectFlash();
    SPI.transfer(0xC7); 
    deselectFlash();
    waitForWriteCompletion();

    m_writeAddress = 0; 
}

/**
 * Sned the read data command 0x03
 * Send the address from which to start reading from
 * Cast the address of data as a pointer to an array of bytes
 * For reading data, the SPI protocol requires the master to send "dummy" data (in this case, 0x00) to generate clock pulses.
 * Each clock pulse shifts one byte of data from the flash memory back to the master
 * i.e. the data is read byte by byte to the array
*/
void WinbondFlash::readFlash(uint32_t address, uint8_t* data, size_t size) 
{
    selectFlash();

    SPI.transfer(0x03); 
    SPI.transfer((address >> 16) & 0xFF); 
    SPI.transfer((address >> 8) & 0xFF);  
    SPI.transfer(address & 0xFF);       

    for (size_t i = 0; i < size; i++) {
        data[i] = SPI.transfer(0x00);
    }

    deselectFlash();
}

void WinbondFlash::readAllFlash(uint32_t address, uint8_t* data)
{
    selectFlash();

    SPI.transfer(0x03); 
    SPI.transfer((address >> 16) & 0xFF); 
    SPI.transfer((address >> 8) & 0xFF);  
    SPI.transfer(address & 0xFF); 

    for (size_t i = 0; i < getCurrentAddress() - address; i++) {
        data[i] = SPI.transfer(0x00);
    }

    deselectFlash();
}

uint32_t WinbondFlash::getCurrentAddress()
{
    return m_writeAddress;
}

/**
 * Pulls the CS pin low to activative SPI communication with flash chip
*/
void WinbondFlash::selectFlash() 
{
    digitalWrite(m_csPin, LOW);
}

/**
 * Pulls the CS pin high to deactivative SPI communication with flash chip
*/
void WinbondFlash::deselectFlash() 
{
    digitalWrite(m_csPin, HIGH);
}

/**
 * Sends the Write Enable command to the flash chip
 * 0x06 command sets the WEL bit in the status register of the flash chop
 * THis allwos the chip to accept write or erase commands
*/
void WinbondFlash::writeEnable() 
{
    selectFlash();
    SPI.transfer(0x06); 
    deselectFlash();
}

/**
 * Sends the read status register command 0x05
 * Waits while the WIP (write in progress) bit is set
*/
void WinbondFlash::waitForWriteCompletion() 
{
    selectFlash();
    SPI.transfer(0x05); 
    while (SPI.transfer(0) & 0x01) 
    { 
        delay(1);
    }
    deselectFlash();
}

/**
 * There are multiple protection settings for reading and writing to the flash chip
 * We want no protection, we will achieve this by setting WPS = 0, CMP = 0, BP2 = 0, BP1 = 0, BP0 = 0
 * 
 * To write non-volatile Status Register bits we need a Write Enable and then a Write Status Register
 * 
 * To write volatile Status Register bits we need a Write Enable for Volatile Status Register before a Write Status Register
 * 
 * WPS, CMP, BP2, BP1, BP0 are all volatile/non-volatile writable
 * 
 * BP2, BP1, BP0 are all in Status Register 1 so have Write Status Register instruction 01h
 * CMP is in Status Register 2 so have Write Status Register instruction 31h
 * WPS is in Status Register 3 so have Write Status Register instruction 11h
 * 
 * We will write these bits all as non volatile so for every register, we need to do:
 * 
 * CS pin low
 * Write Enable 06h
 * Write Status Register 01h/31h/11h
 * 8 bits in for register
 * Write Disable 
 * CS pin high
*/
void WinbondFlash::selectProtection()
{
    selectFlash();
    writeEnable();

    deselectFlash();
}
