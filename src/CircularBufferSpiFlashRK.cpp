#include "CircularBufferSpiFlashRK.h"

CircularBufferSpiFlashRK::CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd) :
    spiFlash(spiFlash), addrStart(addrStart), addrEnd(addrEnd) {

}

CircularBufferSpiFlashRK::~CircularBufferSpiFlashRK() {

}

bool CircularBufferSpiFlashRK::load() {
    return true;
}

bool CircularBufferSpiFlashRK::erase() {
    if ((addrStart % spiFlash->getSectorSize()) != 0) {
        Log.error("startAddr is not sector aligned addr=%d sectorSize=%d", (int)addrStart, (int)spiFlash->getSectorSize());
        return false;
    }

    for(size_t addr = addrStart; addr < addrEnd; addr += spiFlash->getSectorSize()) {
        spiFlash->sectorErase(addr);
    }
    return true;
}



