#include "CircularBufferSpiFlashRK.h"

CircularBufferSpiFlashRK::CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd) :
    spiFlash(spiFlash), addrStart(addrStart), addrEnd(addrEnd) {

}

CircularBufferSpiFlashRK::~CircularBufferSpiFlashRK() {

}



