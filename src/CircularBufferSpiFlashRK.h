#ifndef __CIRCULARBUFFERSPIFLASHRK_H
#define __CIRCULARBUFFERSPIFLASHRK_H

#include "Particle.h"

#ifndef UNITTEST
#include "SpiFlashRK.h"
#else
#include "SpiFlashTester.h"
#endif

class CircularBufferSpiFlashRK {
public:
    CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);
    virtual ~CircularBufferSpiFlashRK();

    bool load();

    bool erase();


    struct SectorHeader {
        uint32_t sectorMagic;
        uint32_t sequence;
        uint32_t flags;
    };

    struct Record {
        uint32_t recordMagic;
        uint16_t size;
        uint16_t flags;
    };

    static const uint32_t SECTOR_MAGIC = 0x0ceb6443;

    static const uint32_t RECORD_MAGIC = 0x26793787;
    static const uint16_t RECORD_FLAG_DELETED_MASK = 0x0001;

    // a4 17 a9 66 


protected:
    SpiFlash *spiFlash;
    size_t addrStart;
    size_t addrEnd;
};


#endif // __CIRCULARBUFFERSPIFLASHRK_H
