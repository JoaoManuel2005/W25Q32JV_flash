#ifndef WINBOND_FLASH_H
#define WINBOND_FLASH_H

#include <Arduino.h>
#include <SPI.h>

#define FLASH_MAX_ADDRESS 4194303 // 3FFFFFh

class WinbondFlash 
{
    public:
        WinbondFlash(uint8_t csPin);
        bool init();
        void readJedecID(int& JEDEC, int& memoryType, int& capacity);
        void logData(const uint8_t data, const size_t size);
        void wipeFlash();
        void readFlash(uint32_t address, uint8_t* data);
        void readFlashBatch(uint32_t address, uint8_t* data, size_t count);
        void readAllFlash(uint32_t address = 0, uint8_t* data);
        uint32_t getCurrentAddress();

    private:
        uint8_t m_csPin;
        uint32_t m_writeAddress;
        void selectFlash();
        void deselectFlash();
        void writeEnable();
        void waitForWriteCompletion();  
        void selectProtection();

};

WindbondFlash& getFlashMemory();

extern WinbondFlash flash;

#endif // WINBOND_FLASH_H