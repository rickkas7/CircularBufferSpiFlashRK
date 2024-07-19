#pragma once

// This file is used by both automated-test (off-device) and examples/1-automated-test (on-device)
// It is not needed during normal operation

#include "CircularBufferSpiFlashRK.h"

void test01(SpiFlash *spiFlash) {
    const size_t startAddr = 0;
    const size_t testSize = 1 * 1024 * 1024;
    
    CircularBufferSpiFlashRK circBuf(spiFlash, startAddr, testSize);
    circBuf.erase();

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

    circBuf.load();


    Log.info("test01 completed!");
}

void test02(SpiFlash *spiFlash) {
}

void runTestSuite(SpiFlash *spiFlash) {
    Log.info("jedecId=%06lx", (unsigned long) spiFlash->jedecIdRead());

	if (!spiFlash->isValid()) {
		Log.error("no valid flash chip");
		return;
    }

    test01(spiFlash);

}
