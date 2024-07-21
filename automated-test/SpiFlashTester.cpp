#include "SpiFlashTester.h"


SpiFlash::SpiFlash(uint8_t *buffer, size_t size) : buffer(buffer), size(size) {

}

void SpiFlash::readData(size_t addr, void *buf, size_t bufLen) {
    assert((addr + bufLen) < size);

    for(size_t ii = 0; ii < bufLen; ii++) {
        ((uint8_t *)buf)[ii] = buffer[addr + ii];
    }
}

void SpiFlash::writeData(size_t addr, const void *buf, size_t bufLen) {
    assert((addr + bufLen) < size);

    for(size_t ii = 0; ii < bufLen; ii++) {
        uint8_t value = ((uint8_t *)buf)[ii];

        buffer[addr + ii] &= value;
    }

}

void SpiFlash::sectorErase(size_t addr) {
    // Verify addr is at a sector boundary
    assert((addr % sectorSize) == 0);

    // Set to 0xff
    for(size_t ii = addr; ii < (addr + sectorSize); ii++) {
        buffer[ii] = 0xff;
    }
}


void SpiFlash::chipErase() {
    for(size_t ii = 0; ii < size; ii++) {
        buffer[ii] = 0xff;
    }
}
