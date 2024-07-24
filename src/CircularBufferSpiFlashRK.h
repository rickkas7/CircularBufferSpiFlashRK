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
         * @brief Allocate a buffer of the specified length but do not write the data
         * 
         * @param len 
         * @return uint8_t 
         */
        uint8_t *allocate(size_t len);
        
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
        uint8_t getByIndex(size_t index) const;

        /**
         * @brief Returns a pointer to the internal buffer
         * 
         * @return const uint8_t* Pointer to buffer, or nullptr
         */
        const uint8_t *getBuffer() const { return buf; };


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

    struct RecordCommon { // 2 bytes
        unsigned int size : 12; //!< Number of bytes (0 - 4094, though less with overhead)
        unsigned int flags : 4; //!< Flag bits
    } __attribute__((__packed__));

    struct SectorCommon { // 8 bytes
        uint32_t sequence; //!< Monotonically increasing sequence number for sector used
        unsigned int flags:4; //!< Various flag bits
        unsigned int reserved:7; //!< Reserved for future use
        unsigned int recordCount:9; //!< Number of records, set during finalize
        unsigned int dataSize:12; //!< Number of bytes of data in records, set during finalize
    } __attribute__((__packed__));

    struct SectorHeader { // 12 bytes
        uint32_t sectorMagic;
        SectorCommon c;
    } __attribute__((__packed__));

    class Sector {
    public:
        void clear(uint16_t sectorNum = 0);

        uint16_t getLastOffset() const;

        void log(LogLevel level, const char *msg, bool includeData = false) const;

        uint16_t sectorNum = 0;
        std::vector<RecordCommon> records;
        SectorCommon c;
    };



    CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);
    virtual ~CircularBufferSpiFlashRK();

    /**
     * @brief Load the metadata for the file system
     * 
     * @return true 
     * @return false 
     * 
     * You must do this (or format) before using the file system. If this function returns
     * false the format is not valid and you should format it.
     */
    bool load();

    /**
     * @brief Formats the file system
     * 
     * @return true 
     * @return false 
     * 
     * This will erase every sector and write an empty file structure to it. This must
     * be done if the file system is invalid or erased.
     */
    bool format();



    bool fsck();

    /**
     * @brief Structure used by readData and markAsRead
     * 
     * This class is derived from DataBuffer so it its methods can be used to access the data from readData
     */
    class ReadInfo : public DataBuffer {
    public:        
        void log(LogLevel level, const char *msg) const;
        uint16_t sectorNum; //!< sector number that was read from
        SectorCommon sectorCommon; //!< Information about the sector. The sequence is what's used from this currently.
        size_t index; //!< The record index that was read
        RecordCommon recordCommon; //!< Information about the record that was read
    };

    /**
     * @brief Read the next unread data from the circular buffer
     * 
     * @param readInfo 
     * @return true 
     * @return false 
     * 
     * After reading the data, you must pass the same readInfo to markAsRead
     * otherwise you'll read the same data again.
     */
    bool readData(ReadInfo &readInfo);

    /**
     * @brief Mark the data from readData as read
     * 
     * @param readInfo 
     * @return true 
     * @return false 
     * 
     * This method works properly even if the sector was overwritten because the 
     * buffer was full and additional data was written to it. It will ignore the
     * mark as read in this case, because the data no longer exists.
     */
    bool markAsRead(const ReadInfo &readInfo);

    /**
     * @brief Write data to the circular buffer
     * 
     * @param data 
     * @return true 
     * @return false
     * 
     * Data is always written to the buffer. If the circular buffer is full, the oldest
     * sector is deleted to make room for new data.
     * 
     * If there is a read in progress on the oldest sector, it will continue, however
     * a markAsRead will be ignored since the underlying data will already have been
     * deleted. 
     */
    bool writeData(const DataBuffer &data);

    /**
     * @brief Class for various stats about the circular buffer usage
     */
    class UsageStats {
    public:
        void log(LogLevel level, const char *msg) const;

        
    };

    /**
     * @brief Get the usage statistics
     * 
     * @param usageStats 
     * @return true 
     * @return false 
     * 
     * This method isn't const because it needs to obtain a lock on this object.
     */
    bool getUsageStats(UsageStats &usageStats);


#ifndef UNITTEST
protected:
#endif
    /**
     * @brief Get the Sector object for a sector if it exists in the cache
     * 
     * @param sectorNum 
     * @return Sector* Object if it exists, or nullptr if not
     * 
     * The Sector object is just the metadata and an index of the records in it. It does not
     * contain a copy of the data, so it's relatively small.
     * 
     * Do not delete the object returned by this method; it's owned by the cache and is
     * not a copy!
     */
    Sector *getSectorFromCache(uint16_t sectorNum);

    /**
     * @brief Get the Sector object for a sector, allocating and reading it if not in the cache
     * 
     * @param sectorNum 
     * @return Sector* 
     * 
     * The Sector object is just the metadata and an index of the records in it. It does not
     * contain a copy of the data, so it's relatively small.
     * 
     * Do not delete the object returned by this method; it's owned by the cache and is
     * not a copy!
     */
    Sector *getSector(uint16_t sectorNum);


    bool readSector(uint16_t sectorNum, Sector *sector);

    bool writeSectorHeader(uint16_t sectorNum, bool erase, uint32_t sequence);

    bool appendDataToSector(Sector *sector, const DataBuffer &data, uint16_t flags);

    bool finalizeSector(Sector *sector);

    bool readDataFromSector(Sector *sector, size_t index, DataBuffer &data, RecordCommon &meta);

    bool validateSector(Sector *pSector);

    bool sequenceToSectorNum(uint32_t sequence, uint16_t &sectorNum) const;


    /**
     * @brief Convert a sector number to an address
     * 
     * @param sectorNum 0 is the first sector of this buffer, not the device! 
     * @return uint32_t The byte address in in the device for the beginning of this sector
     */
    uint32_t sectorNumToAddr(uint16_t sectorNum) const { return addrStart + sectorNum * spiFlash->getSectorSize(); };

    static const uint32_t SECTOR_MAGIC = 0x0ceb6443;
    static const uint32_t SECTOR_MAGIC_ERASED = 0xffffffff;
    static const unsigned int SECTOR_FLAG_STARTED_MASK = 0x01;
    static const unsigned int SECTOR_FLAG_FINALIZED_MASK = 0x02;
    static const unsigned int SECTOR_FLAG_CORRUPTED_MASK = 0x04;

    static const unsigned int RECORD_SIZE_ERASED = 0xfff;
    static const unsigned int RECORD_FLAG_READ_MASK = 0x0001;

    static const uint32_t UNUSED_MAGIC = 0xa417a966;
    static const uint32_t UNUSED_MAGIC2 = 0x26793787;

    static const size_t SECTOR_CACHE_SIZE = 10;

public:
#ifndef UNITTEST
    /**
     * @brief Locks the mutex that protects shared resources
     * 
     * This is compatible with `WITH_LOCK(*this)`.
     * 
     * The mutex is not recursive so do not lock it within a locked section.
     */
    void lock() { os_mutex_recursive_lock(mutex); };

    /**
     * @brief Attempts to lock the mutex that protects shared resources
     * 
     * @return true if the mutex was locked or false if it was busy already.
     */
    bool tryLock() { return os_mutex_recursive_trylock(mutex); };

    /**
     * @brief Unlocks the mutex that protects shared resources
     */
    void unlock() { os_mutex_recursive_unlock(mutex); };
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

    SectorCommon *sectorMeta = nullptr;

    /*
    Sector *currentReadSector = nullptr;
    Sector *currentWriteSector = nullptr;
    int firstSector = -1;
    uint32_t firstSectorSequence = 0;

    int lastSector = -1;
    uint32_t lastSectorSequence = 0;
    */

    bool isValid = false;
    std::deque<Sector*> sectorCache;

    uint32_t firstSequence = 0;
    uint32_t writeSequence = 0;
    uint32_t lastSequence = 0;

    /**
     * @brief Mutex to protect shared resources
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
#ifndef UNITTEST
    os_mutex_recursive_t mutex = 0;
#endif

};


#endif // __CIRCULARBUFFERSPIFLASHRK_H
