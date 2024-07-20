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

    /**
     * @brief Class to hold a copy of data, either by pointer and length or a c-string
     * 
     * Uses a buffer allocated by new internally. When storing c-strings internally, the
     * null is included in the buffer for efficiently using c_str() and operator const char *().
     */
    class DataBuffer {
    public:
        /**
         * @brief Construct an empty buffer
         */
        DataBuffer();

        /**
         * @brief Destroy object. Frees the allocated buffer.
         */
        virtual ~DataBuffer();

        /**
         * @brief Construct a data buffer with a copy of the data by pointer and length
         * 
         * @param buf Pointer to a data. If null, essentially just does free().
         * @param len Length of data. If 0, essentially just does free().
         */
        DataBuffer(const void *buf, size_t len);

        /**
         * @brief Construct an object that's a copy of another object
         * 
         * @param other 
         * 
         * The objects will have different allocated pointers, so changes to one
         * will not affect the other.
         */
        DataBuffer(const DataBuffer &other);

        /**
         * @brief Set this object with a copy of another.
         * 
         * @param other 
         * 
         * The objects will have different allocated pointers, so changes to one
         * will not affect the other.
         */
        DataBuffer &operator=(const DataBuffer &other);

        /**
         * @brief Construct a new object from a c-string
         * 
         * @param str String to copy. If null, essentially just calls free() on this object.
         * 
         * The str value is copied, so it does not need to remain valid after
         * this call returns.
         */
        DataBuffer(const char *str);

        /**
         * @brief Returns the size of the allocated buffer
         * 
         * @return size_t 
         * 
         * For c-strings, the size includes the trailing null, so this will be exactly
         * 1 larger than strlen(). In other words, an empty c-string has a size() of 1.
         */
        size_t size() const { return buf ? len : 0; };

        /**
         * @brief Frees the allocate buffer and sets the length to 0
         */
        void free();
        
        /**
         * @brief Copies the data in buf and len into an allocated buffer in this object.
         * 
         * @param buf Pointer to a data. If null, essentially just does free().
         * @param len Length of data. If 0, essentially just does free().
         */
        void copy(const void *buf, size_t len);

        /**
         * @brief Copies the c-string str 
         * 
         * @param str A c-string to copy. If null, essentially just does free()
         * 
         * The internal buffer includes the null terminator so c_str() is efficient.
         */
        void copy(const char *str);
        
        /**
         * @brief Returns true if size and bytes are the same as another object
         * 
         * @param other 
         * @return true 
         * @return false 
         * 
         * Returns true only if both objects are allocated, the same length, and have
         * the same byte values. Returns false in all other cases.
         */
        bool operator==(const DataBuffer &other) const;

        /**
         * @brief Returns true of the strings are equal
         * 
         * @param str 
         * @return true 
         * @return false 
         * 
         * Returns true only if this object has a buffer allocated, it's a c-string,
         * and lengths are the same, and the bytes are the same. In all other cases
         * false is returned.
         */
        bool equals(const char *str) const;

        /**
         * @brief Returns a pointer to the internal string
         * 
         * @return const char* A valid c-string
         * 
         * This always returns a valid c-string, even if this buffer is freed or
         * not a c-string. If this object is not a valid c-string, returns a pointer
         * to a static null value.
         * 
         * The returned string will not be valid after this object is modified
         * as a new buffer is typically allocated instead of modifying the buffer
         * in place!
         */
        const char *c_str() const;

        /**
         * @brief Returns a pointer to the internal string
         * 
         * @return const char* A valid c-string
         * 
         * This always returns a valid c-string, even if this buffer is freed or
         * not a c-string. If this object is not a valid c-string, returns a pointer
         * to a static null value.
         * 
         * The returned string will not be valid after this object is modified
         * as a new buffer is typically allocated instead of modifying the buffer
         * in place!
         */
        operator const char *() const { return c_str(); };

        /**
         * @brief Get a byte by index from the internal buffer
         * 
         * @param index 0 = first byte of buffer
         * @return uint8_t Return 0 if buffer or index is invalid
         */
        uint8_t get(size_t index) const;


    protected:
        /**
         * @brief Internal allocated buffer
         * 
         * It's allocated mostly by copy().
         * 
         * It's freed by free(). The destructor calls free().
         * 
         * It's nullptr when not allocated.
         */
        uint8_t *buf; 

        /**
         * @brief Length of the data in buf
         * 
         * The length includes the trailing null for c-strings.
         * 
         * If length is 0, buf is not allocated and will be nullptr.
         */
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
