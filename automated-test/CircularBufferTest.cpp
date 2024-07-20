#include <stdio.h>
#include "CircularBufferSpiFlashRK.h"
#include "SpiFlashTester.h"
#include "CircularBufferSpiFlashRK_AutomatedTest.h"

void runUnitTests();

const size_t flashSize = 8 * 1024 * 1024; // 8 MB
uint8_t flashBuffer[flashSize];
SpiFlash spiFlash(flashBuffer, flashSize);

int main(int argc, char *argv[]) {
    spiFlash.begin();
    runUnitTests();
    runTestSuite(&spiFlash);
    return 0;
}

#define assertDouble(exp, val, tol) \
    if (val < (exp - tol) || val > (exp + tol)) { printf("exp=%lf val=%lf\n", (double)exp, (double)val); assert(false); }

void saveSectorToFile(CircularBufferSpiFlashRK::Sector *pSector, const char *filename) {
    FILE *fd = fopen(filename, "w+");
    
    
    fclose(fd);
}

void testDataBuffer() {
    {
        CircularBufferSpiFlashRK::DataBuffer t;
    }
    {
        CircularBufferSpiFlashRK::DataBuffer t("testing");
        assert(t.size() == 8);
        assert(t.equals("testing"));
        assert(!t.equals("testing!"));
        assert(!t.equals("testin"));

        CircularBufferSpiFlashRK::DataBuffer t2(t);
        assert(t == t2);

    }
}

void testUnitSectorAppend() {
    // Unit testing of sector append functions
    {
        CircularBufferSpiFlashRK circBuffer(&spiFlash, 0, 1024 * 1024);

        CircularBufferSpiFlashRK::Sector *pSector = new CircularBufferSpiFlashRK::Sector;

        pSector->log(String(__LINE__));

        circBuffer.appendToSector(pSector, "test", 4, 0xffff, true);

        pSector->log(String(__LINE__));

        saveSectorToFile(pSector, "test01-1");

        delete pSector;        
    }


}

void runUnitTests() {
    // Local unit tests only used off-device 
    testDataBuffer();
    
    testUnitSectorAppend();

}




