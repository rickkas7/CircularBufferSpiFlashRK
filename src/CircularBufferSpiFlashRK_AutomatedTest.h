#pragma once

// This file is used by both automated-test (off-device) and examples/1-automated-test (on-device)
// It is not needed during normal operation

#include "CircularBufferSpiFlashRK.h"

void test01(SpiFlash *spiFlash) {
    const size_t startAddr = 0;
    const size_t testSize = 1 * 1024 * 1024;
    
    for(size_t addr = startAddr; addr < startAddr + testSize; addr += 4096) {
        spiFlash->sectorErase(addr);
    }

    // Check erase
    uint8_t tempBuf[256];
    uint8_t expectedBuf[sizeof(tempBuf)];
    memset(expectedBuf, 0xff, sizeof(expectedBuf));
    for(size_t addr = startAddr; addr < testSize; addr += sizeof(tempBuf)) {
        spiFlash->readData(addr, tempBuf, sizeof(tempBuf));
        for(size_t ii = 0; ii < sizeof(tempBuf); ii++) {
            if (tempBuf[ii] != 0xff) {
                Log.error("test failed ii=%d value=0x%02x expected=0x%02x line=%d", (int)ii, (int)tempBuf[ii], (int)expectedBuf[ii], (int)__LINE__);
                return;
            }
        }
    }
    // Check write
    for(size_t ii = 0; ii < sizeof(tempBuf); ii++) {
        tempBuf[ii] = expectedBuf[ii] = (uint8_t) ii;
    }
    spiFlash->writeData(startAddr, tempBuf, sizeof(tempBuf));

    memset(tempBuf, 0, sizeof(tempBuf));
    spiFlash->readData(startAddr, tempBuf, sizeof(tempBuf));
    for(size_t ii = 0; ii < sizeof(tempBuf); ii++) {
        if (tempBuf[ii] != expectedBuf[ii]) {
                Log.error("test failed ii=%d value=0x%02x expected=0x%02x line=%d", (int)ii, (int)tempBuf[ii], (int)expectedBuf[ii], (int)__LINE__);
            return;
        }
    }

    // Check NOR flash semantics (can only set bit to 0)
    tempBuf[0] = 0; // writes over 0x00
    tempBuf[1] = 0; // writes over 0x01
    tempBuf[2] = 0xff; // writes over 0x02
    spiFlash->writeData(startAddr, tempBuf, 3);

    memset(tempBuf, 0xff, sizeof(tempBuf));
    expectedBuf[0] = 0;
    expectedBuf[1] = 0;
    expectedBuf[2] = 2;
    spiFlash->readData(startAddr, tempBuf, 3);
    for(size_t ii = 0; ii < 2; ii++) {
        if (tempBuf[ii] != expectedBuf[ii]) {
                Log.error("test failed ii=%d value=0x%02x expected=0x%02x line=%d", (int)ii, (int)tempBuf[ii], (int)expectedBuf[ii], (int)__LINE__);
            return;
        }
    }

    spiFlash->sectorErase(startAddr);


    Log.info("test01 completed!");
}

String makeRandomString(size_t maxLen) {
    const char dict[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    char buf[maxLen + 1];    
    int stringLen = rand() % maxLen;

    for(int charNum = 0; charNum < stringLen; charNum++) {
        int index = rand() % sizeof(dict);

        buf[charNum] = dict[index];
    }
    buf[stringLen] = 0;

    return String(buf);
}

void test02(SpiFlash *spiFlash) {
    size_t testCount = 20;
    size_t maxLen = 128;
    size_t subTestSize = 20;

    const uint16_t sectorCount = 64;

    CircularBufferSpiFlashRK circBuffer(spiFlash, 0, sectorCount * 4096);
    circBuffer.format();
    
    std::deque<String> strings;

    for(size_t testNum = 0; testNum < testCount; testNum++) {
        int numToWrite = rand() % subTestSize;
        for(int ii = 0; ii < numToWrite; ii++) {
            String s = makeRandomString(maxLen);
            strings.push_back(s);

            CircularBufferSpiFlashRK::DataBuffer origBuffer(s);
            circBuffer.writeData(origBuffer);
        }


        // Read more than we write on average to avoid infinitely growing
        int numToRead = rand() % (2 * subTestSize);
        for(int ii = 0; ii < numToRead; ii++) {
            CircularBufferSpiFlashRK::DataInfo dataInfo;
            if (circBuffer.readData(dataInfo)) {
                circBuffer.markAsRead(dataInfo);

                CircularBufferSpiFlashRK::DataBuffer origBuffer(strings.front());
                strings.pop_front();

                if (strcmp(origBuffer.c_str(), dataInfo.c_str()) != 0) {
                    Log.info("testNum=%d ii=%d", (int)testNum, (int)ii);
                    Log.info("got: %s", dataInfo.c_str());
                    Log.info("exp: %s\n", origBuffer.c_str());

                    return;
                }
            }
        }
    }
    Log.info("test2 complete");
}

void runTestSuite(SpiFlash *spiFlash) {
    Log.info("jedecId=%06lx", (unsigned long) spiFlash->jedecIdRead());

	if (!spiFlash->isValid()) {
		Log.error("no valid flash chip");
		return;
    }

    test01(spiFlash);
    test02(spiFlash);

}
