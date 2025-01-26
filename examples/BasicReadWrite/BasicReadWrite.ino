#include <WinbondFlash.h> 

#define FLASH_CS_PIN 10

WinbondFlash flash(FLASH_CS_PIN);

void setup() 
{
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println("Winbond Flash Basic Read/Write Example");

    if (!flash.init()) {
        Serial.println("Failed to initialize flash chip");
        while (1); 
    }

    const char dataToWrite[] = "Hello world";
    size_t dataLength = sizeof(dataToWrite);
    flash.logData((const uint8_t*)dataToWrite, dataLength);

    char readBuffer[32] = {0}; 
    flash.readFlash(0, (uint8_t*)readBuffer, dataLength);
    Serial.print("Data read back: ");
    Serial.println(readBuffer);
}

void loop() 
{

}