#ifndef __CIRCULARBUFFERSPIFLASHRK_H
#define __CIRCULARBUFFERSPIFLASHRK_H

#include "Particle.h"

#ifndef UNITTEST
#include "SpiFlashRK.h"
#else
#include "SpiFlashTester.h"
#endif

#include <vector>
#include <deque>

class CircularBufferSpiFlashRK {
public:
    struct RecordCommon {
        uint16_t size;
        uint16_t flags;
    };

    struct RecordHeader {
        uint32_t recordMagic;
        RecordCommon c;
    };

    struct Record {
        size_t offset;
        RecordCommon c;
    };

    struct SectorCommon {
        uint32_t sequence;
        uint16_t flags;
        uint16_t reserved;
    };

    struct SectorHeader {
        uint32_t sectorMagic;
        SectorCommon c;
    };

    class Sector {
    public:
        void clear(uint16_t sectorNum = 0);

        uint16_t getLastOffset() const;

        void log(const char *msg, bool includeData = false) const;


        uint16_t sectorNum = 0;
        uint16_t internalFlags = 0;
        std::vector<Record> records;
        SectorCommon c;
    };

    class DataBuffer {
    public:
        DataBuffer();
        virtual ~DataBuffer();

        DataBuffer(const void *buf, size_t len);

        DataBuffer(const DataBuffer &other);
        DataBuffer &operator=(const DataBuffer &other);

        DataBuffer(const char *str);

        size_t size() const { return buf ? len : 0; };

        void free();

        void copy(const void *buf, size_t len);
        void copy(const char *str);

        bool operator==(const DataBuffer &other) const;
        bool equals(const char *str) const;

        const char *c_str() const;

        operator const char *() const { return c_str(); };



    protected:
        uint8_t *buf;
        size_t len;
    };


    CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);
    virtual ~CircularBufferSpiFlashRK();

    bool load();

    bool erase();

    Sector *getSector(uint16_t sectorNum);


    bool readSector(uint16_t sectorNum, Sector *sector);

    bool writeSectorHeader(uint16_t sectorNum, bool erase, uint32_t sequence);

    bool appendToSector(Sector *sector, const void *data, size_t dataLen, uint16_t flags, bool write);


    /**
     * @brief Convert a sector number to an address
     * 
     * @param sectorNum 0 is the first sector of this buffer, not the device! 
     * @return uint32_t The byte address in in the device for the beginning of this sector
     */
    uint32_t sectorNumToAddr(uint16_t sectorNum) const { return addrStart + sectorNum * spiFlash->getSectorSize(); };


    static const uint32_t SECTOR_MAGIC = 0x0ceb6443;
    static const uint32_t SECTOR_MAGIC_ERASED = 0xffffffff;
    static const uint32_t SECTOR_FLAG_HEADER_MASK = 0x0001;
    static const uint32_t SECTOR_FLAG_FINALIZED_MASK = 0x0002;
    static const uint32_t SECTOR_FLAG_DELETED_MASK = 0x0004;

    static const uint32_t SECTOR_INTERNAL_FLAG_ERASED = 0x0001;
    static const uint32_t SECTOR_INTERNAL_FLAG_CORRUPTED = 0x0002;
    static const uint32_t SECTOR_INTERNAL_FLAG_VALID = 0x8000;

    static const uint32_t RECORD_MAGIC = 0x26793787;
    static const uint32_t RECORD_MAGIC_ERASED = 0xffffffff;
    static const uint16_t RECORD_FLAG_DELETED_MASK = 0x0001;

    static const uint32_t UNUSED_MAGIC = 0xa417a966;

    static const size_t SECTOR_CACHE_SIZE = 10;

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


    Sector *currentReadSector = nullptr;
    Sector *currentWriteSector = nullptr;
    int firstSector = -1;
    uint32_t firstSectorSequence = 0;

    int lastSector = -1;
    uint32_t lastSectorSequence = 0;

    std::deque<Sector*> sectorCache;

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
