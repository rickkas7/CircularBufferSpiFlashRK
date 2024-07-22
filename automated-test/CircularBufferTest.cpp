#include <stdio.h>
#include "CircularBufferSpiFlashRK.h"
#include "SpiFlashTester.h"
#include "CircularBufferSpiFlashRK_AutomatedTest.h"

void runUnitTests();

const size_t flashSize = 8 * 1024 * 1024; // 8 MB
uint8_t flashBuffer[flashSize];
SpiFlash spiFlash(flashBuffer, flashSize);

std::vector<String> randomString1024;
std::vector<String> randomStringSmall;

int main(int argc, char *argv[]) {
    spiFlash.begin();
    runUnitTests();
    runTestSuite(&spiFlash);
    return 0;
}

#define assertDouble(exp, val, tol) \
    if (val < (exp - tol) || val > (exp + tol)) { printf("exp=%lf val=%lf\n", (double)exp, (double)val); assert(false); }

void generateRandomStrings() {
    {
        FILE *fd = fopen("test01/randomString1024.txt", "w+");

        char buf[1025];

        int stringCount = 1000;
        
        for(int stringNum = 0; stringNum < stringCount; stringNum++) {
            int stringLen = rand() % 1024;

            for(int charNum = 0; charNum < stringLen; charNum++) {
                int c = rand() % 95;

                buf[charNum] = (char)(32 + c);
            }
            buf[stringLen] = 0;

            fprintf(fd, "%s\n", buf);
        }
        
        fclose(fd);
    }
    {
        FILE *fd = fopen("test01/randomStringSmall.txt", "w+");

        char buf[1025];

        int stringCount = 1000;

        const char dict[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        
        for(int stringNum = 0; stringNum < stringCount; stringNum++) {
            int stringLen = rand() % 64;

            for(int charNum = 0; charNum < stringLen; charNum++) {
                int index = rand() % sizeof(dict);

                buf[charNum] = dict[index];
            }
            buf[stringLen] = 0;

            fprintf(fd, "%s\n", buf);
        }
        
        fclose(fd);
    }


}

void readRandomStrings() {
    char buf[1025];

    {
        FILE *fd = fopen("test01/randomString1024.txt", "r");
        while(true) {
            char *cp = fgets(buf, sizeof(buf), fd);
            if (!cp) {
                break;
            }
            int len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = 0;
            }

            randomString1024.push_back(String(buf));
        }

        fclose(fd);
    }

    {
        FILE *fd = fopen("test01/randomStringSmall.txt", "r");
        while(true) {
            char *cp = fgets(buf, sizeof(buf), fd);
            if (!cp) {
                break;
            }
            int len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = 0;
            }

            randomStringSmall.push_back(String(buf));
        }

        fclose(fd);
    }

}

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

void testUnitSectorAppend(std::vector<String> &testSet) {
    // Unit testing of sector append functions
    {
        const uint16_t sectorCount = 512;

        CircularBufferSpiFlashRK circBuffer(&spiFlash, 0, sectorCount * 4096);
        circBuffer.format();

        int stringNum = 0;

        for(uint16_t sectorNum = 0; sectorNum < sectorCount && stringNum < testSet.size(); sectorNum++) {
            CircularBufferSpiFlashRK::Sector *pSector = circBuffer.getSector(sectorNum);

            while(stringNum < testSet.size()) {
                bool bResult;

                CircularBufferSpiFlashRK::DataBuffer origBuffer(testSet.at(stringNum).c_str());
                bResult = circBuffer.appendDataToSector(pSector, origBuffer, 0xffff);
                if (!bResult) {
                    break;
                }

                stringNum++;
            }

            circBuffer.finalizeSector(pSector);
            // pSector->log("test");
        }

        
        stringNum = 0;
        bool error = false;

        for(uint16_t sectorNum = 0; sectorNum < sectorCount && stringNum < testSet.size() && !error; sectorNum++) {
            CircularBufferSpiFlashRK::Sector *pSector = circBuffer.getSector(sectorNum);

            int stringIndex = 0;
            while(stringNum < testSet.size()) {
                CircularBufferSpiFlashRK::DataBuffer tempBuffer;
                CircularBufferSpiFlashRK::RecordCommon meta;

                bool bResult = circBuffer.readDataFromSector(pSector, stringIndex, tempBuffer, meta);
                if (!bResult) {
                    break;
                }

                if (strcmp(tempBuffer.c_str(), testSet.at(stringNum).c_str()) == 0) {

                }
                else {
                    Log.error("mismatch line=%d stringIndex=%d stringNum=%d sectorNum=%d", __LINE__, stringIndex, (int)stringNum, (int)sectorNum );
                    printf("got: %s\nexp: %s\n", tempBuffer.c_str(), testSet.at(stringNum).c_str());

                    for(int tempStringIndex = 0; tempStringIndex < testSet.size(); tempStringIndex++) {
                        if (randomStringSmall.at(tempStringIndex) == tempBuffer.c_str()) {
                            printf("found match at stringIndex=%d\n", tempStringIndex);
                            break;
                        }
                    }

                    error = true;
                    break;
                }

                stringNum++;
                stringIndex++;
            }
        }
    }



}

void runUnitTests() {
    // Local unit tests only used off-device 

    // generateRandomStrings();
    readRandomStrings();

    testDataBuffer();
    
    testUnitSectorAppend(randomStringSmall);
    testUnitSectorAppend(randomString1024);

}




