#ifndef __CIRCULARBUFFERSPIFLASHRK_H
#define __CIRCULARBUFFERSPIFLASHRK_H

#include "Particle.h"

#ifndef UNITTEST
#include "SpiFlashRK.h"
#else
#include "SpiFlashTester.h"
#endif

#include <vector>

class CircularBufferSpiFlashRK {
public:

    struct RecordHeader {
        uint32_t recordMagic;
        uint16_t size;
        uint16_t flags;
    };

    struct Record {
        size_t offset;
        uint16_t size;
        uint16_t flags;
    };

    struct SectorHeader {
        uint32_t sectorMagic;
        uint32_t sequence;
        uint16_t flags;
        uint16_t reserved;
    };

    struct Sector {
        uint16_t sectorNum;
        uint16_t flags;
        uint32_t sequence;
        std::vector<Record> records;
    };


    CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);
    virtual ~CircularBufferSpiFlashRK();

    bool load();

    bool erase();

    bool readSector(uint16_t sectorNum, Sector &sector);

    bool writeSectorHeader(uint16_t sectorNum, bool erase, uint32_t sequence);

    /**
     * @brief Convert a sector number to an address
     * 
     * @param sectorNum 0 is the first sector of this buffer, not the device! 
     * @return uint32_t The byte address in in the device for the beginning of this sector
     */
    uint32_t sectorNumToAddr(uint16_t sectorNum) const { return addrStart + sectorNum * spiFlash->getSectorSize(); };


    static const uint32_t SECTOR_MAGIC = 0x0ceb6443;
    static const uint32_t SECTOR_FLAG_HEADER_MASK = 0x0001;
    static const uint32_t SECTOR_FLAG_FINALIZED_MASK = 0x0002;

    static const uint32_t RECORD_MAGIC = 0x26793787;
    static const uint16_t RECORD_FLAG_DELETED_MASK = 0x0001;

    // a4 17 a9 66 

#ifndef UNITTEST
    /**
     * @brief Locks the mutex that protects shared resources
     * 
     * This is compatible with `WITH_LOCK(*this)`.
     * 
     * The mutex is not recursive so do not lock it within a locked section.
     */
    void lock() { os_mutex_lock(mutex); };

    /**
     * @brief Attempts to lock the mutex that protects shared resources
     * 
     * @return true if the mutex was locked or false if it was busy already.
     */
    bool tryLock() { return os_mutex_trylock(mutex); };

    /**
     * @brief Unlocks the mutex that protects shared resources
     */
    void unlock() { os_mutex_unlock(mutex); };
#else
    void lock() {};
    bool tryLock() { return true; };
    void unlock() {};
#endif // UNITTEST

protected:
    SpiFlash *spiFlash;
    size_t addrStart;
    size_t addrEnd;
    size_t sectorCount; //!< Calculated in constructor, number of sectors from addrStart to addrEnd

    /**
     * @brief Mutex to protect shared resources
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
#ifndef UNITTEST
    os_mutex_t mutex = 0;
#endif

};


#endif // __CIRCULARBUFFERSPIFLASHRK_H
