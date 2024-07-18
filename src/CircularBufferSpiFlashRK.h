#ifndef __CIRCULARBUFFERSPIFLASHRK_H
#define __CIRCULARBUFFERSPIFLASHRK_H

#include "Particle.h"

#include "SpiFlashRK.h"

class CircularBufferSpiFlashRK {
public:
    CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);
    virtual ~CircularBufferSpiFlashRK();


protected:
    SpiFlash *spiFlash;
    size_t addrStart;
    size_t addrEnd;
};


#endif // __CIRCULARBUFFERSPIFLASHRK_H
