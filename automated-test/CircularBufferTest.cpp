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

        assert(strcmp(t.c_str(), "testing") == 0);

        CircularBufferSpiFlashRK::DataBuffer t2(t);
        assert(t == t2);

        t2.copy("different");
        assert(t != t2);
    }

    {
        uint8_t b1[4] = { 2, 3, 0, 1 };

        CircularBufferSpiFlashRK::DataBuffer t(b1, sizeof(b1));

        assert(t.size() == 4);
        assert(t.getByIndex(0) == (const uint8_t)2);
        assert(t.getByIndex(1) == 3);
        assert(t.getByIndex(2) == 0);
        assert(t.getByIndex(3) == 1);


        CircularBufferSpiFlashRK::DataBuffer t2(t);
        assert(t == t2);



    }
}

void testUnitSectorAppend() {
    // Unit testing of sector append functions
    {
        CircularBufferSpiFlashRK circBuffer(&spiFlash, 0, 1024 * 1024);
        circBuffer.erase();
        circBuffer.load();

        CircularBufferSpiFlashRK::Sector *pSector = circBuffer.getSector(0);

        bool bResult;

        pSector->log(String(__LINE__));

        CircularBufferSpiFlashRK::DataBuffer origBuffer("testing!");

        bResult = circBuffer.appendDataToSector(pSector, origBuffer, 0xffff);
        assert(bResult);

        CircularBufferSpiFlashRK::DataBuffer tempBuffer;
        CircularBufferSpiFlashRK::RecordCommon meta;

        bResult = circBuffer.readDataFromSector(pSector, 0, tempBuffer, meta);
        assert(bResult);
        Log.info("tempBuffer %s", tempBuffer.c_str());
        assert(origBuffer == tempBuffer);

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




