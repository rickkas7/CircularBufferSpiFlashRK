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
         * 
         * This method is used internally to allocate space, then read the data from
         * flash into the buffer.
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

        /**
         * @brief Get the length of the data
         * 
         * @return size_t 
         */
        size_t getLen() const { return len; };

        /**
         * @brief Set the length to newLen if it's <= the current length (only truncates, does not extend)
         * 
         * @param newLen 
         * 
         * Doesn't actually release the memory, just sets len.
         */
        void truncate(size_t newLen);

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

    /**
     * @brief Data stored in flash for each record in the sector
     * 
     * This 2 byte (16 bit) structure is stored packed after the SectorHeader.
     * If the size is all 1 bits (RECORD_SIZE_ERASED, 0xfff) then this record
     * has not been written yet. (SPI NOR flash sectors are initialized to all
     * 1s during sector or chip erase.)
     */
    struct RecordCommon { // 2 bytes
        unsigned int size : 12; //!< Number of bytes (0 - 4094, though less with overhead)
        unsigned int flags : 4; //!< Flag bits
    } __attribute__((__packed__));

    /**
     * @brief Data stored after the magic bytes in flash
     * 
     * A copy of this is kept in RAM as well, so the library will use 8 bytes of
     * RAM for each sector.
     */
    struct SectorCommon { // 8 bytes
        uint32_t sequence; //!< Monotonically increasing sequence number for sector used
        unsigned int flags:4; //!< Various flag bits
        unsigned int reserved:7; //!< Reserved for future use
        unsigned int recordCount:9; //!< Number of records, set during finalize
        unsigned int dataSize:12; //!< Number of bytes of data in records, set during finalize
    } __attribute__((__packed__));

    /**
     * @brief Structure store at the beginning of each sector
     * 
     * This is separate from SectorCommon since we store information about each
     * sector in the circular buffer. The magic bytes are necessary in flash but
     * not in RAM, so not storing it in RAM saves 4 bytes per sector.
     */
    struct SectorHeader { // 12 bytes
        uint32_t sectorMagic; //!< Magic bytes SECTOR_MAGIC = 0x0ceb6443
        SectorCommon c; //!< SectorCommon structure (8 bytes)
    } __attribute__((__packed__));


    /**
     * @brief Information about a sector, stored in RAM
     * 
     * This class is instantiated and owned by getSector() which manages the object 
     * lifetime. Do not delete an instance of this class directly.
     * 
     * The main reason for this class is that records are packed sequentially
     * into a sector. In order to access the records efficiently, the Sector
     * class builds an index in RAM. This requires an SPI read for each record.
     * 
     * This is done per-sector, since an index of every sector could be very large.
     * 
     * The sector class does not contain the data in the sector, so this class 
     * is relatively small (not the fully 4096 byte sector).
     */
    class Sector {
    public:
        /**
         * @brief Clear the records vector and common data
         * 
         * @param sectorNum 
         */
        void clear(uint16_t sectorNum = 0);

        /**
         * @brief Get the offset within the sector after the last record
         * 
         * @return uint16_t 
         */
        uint16_t getLastOffset() const;

        /**
         * @brief Log information about the sector to _log.
         * 
         * @param level The log level, such as LOG_LEVEL_TRACE or LOG_LEVEL_INFO
         * @param msg A message to insert at the beginning of the log message
         * @param includeData Not currently used
         */
        void log(LogLevel level, const char *msg, bool includeData = false) const;

        uint16_t sectorNum = 0; //!< Sector number this object contains
        std::vector<RecordCommon> records; //!< The RecordCommon structure for each record in this sector
        SectorCommon c; //!< The SectorCommon structure for this sector
    };


    /**
     * @brief Construct a new circular buffer object. This is typically done as a global variable.
     *
     * @param spiFlash The SpiFlashRK object for the SPI NOR flash chip.
     * @param addrStart Address to start at (typically 0). Must be sector aligned (multiple of 4096 bytes).
     * @param addrEnd Address to end at (not inclusive). Must be sector aligned (multiple of 4096 bytes).
     */
    CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd);

    /**
     * @brief Destroy the object
     */
    virtual ~CircularBufferSpiFlashRK();

    /**
     * @brief Load the metadata for the file system
     * 
     * @return true on success or false on failure
     * 
     * You must do this (or format) before using the file system. If this function returns
     * false the format is not valid and you should format it.
     */
    bool load();

    /**
     * @brief Formats the file system
     * 
     * @return true on success or false on failure
     * 
     * This will erase every sector and write an empty file structure to it. This must
     * be done if the file system is invalid or erased.
     */
    bool format();


    /**
     * @brief Perform a file system check. Not currently implemented!
     * 
     * @param repair True to attempt to repair a damaged circular buffer.
     * @return true on success or false on failure
     */
    bool fsck(bool repair);

    /**
     * @brief Structure used by readData and markAsRead
     * 
     * This class is derived from DataBuffer so it its methods can be used to access the data from readData
     */
    class ReadInfo : public DataBuffer {
    public:        
        /**
         * @brief 
         * 
         * @param level The log level, such as LOG_LEVEL_TRACE or LOG_LEVEL_INFO
         * @param msg 
         */
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
     * @return true on success or false on failure
     * 
     * After reading the data, you must pass the same readInfo to markAsRead
     * otherwise you'll read the same data again.
     */
    bool readData(ReadInfo &readInfo);

    /**
     * @brief Mark the data from readData as read
     * 
     * @param readInfo 
     * @return true on success or false on failure
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
     * @return true on success or false on failure
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
        /**
         * @brief 
         * 
         * @param level The log level, such as LOG_LEVEL_TRACE or LOG_LEVEL_INFO
         * @param msg 
         */
        void log(LogLevel level, const char *msg) const;

        size_t recordCount;
        size_t dataSize;
    };

    /**
     * @brief Get the usage statistics
     * 
     * @param usageStats 
     * @return true on success or false on failure
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

    /**
     * @brief Used internally to read the data from SPI flash. Use getSector() instead!
     * 
     * @param sectorNum 
     * @param sector 
     * @return true on success or false on failure
     */
    bool readSector(uint16_t sectorNum, Sector *sector);

    /**
     * @brief Used internally to write a sector header. Use writeData() instead!
     * 
     * @param sectorNum 
     * @param erase 
     * @param sequence 
     * @return true on success or false on failure
     */
    bool writeSectorHeader(uint16_t sectorNum, bool erase, uint32_t sequence);

    /**
     * @brief Used internally to append data to an existing sector. Use writeData() instead!
     * 
     * @param sector 
     * @param data 
     * @param flags 
     * @return true on success or false on failure
     */
    bool appendDataToSector(Sector *sector, const DataBuffer &data, uint16_t flags);
    
    /**
     * @brief Used internally when a sector is full and a new sector needs to be used. Use writeData() instead!
     * 
     * @param sector 
     * @return true on success or false on failure
     */
    bool finalizeSector(Sector *sector);

    /**
     * @brief Used internally to read a record from a sector. Use readData() instead!
     * 
     * @param sector 
     * @param index 
     * @param data 
     * @param meta 
     * @return true on success or false on failure
     */
    bool readDataFromSector(Sector *sector, size_t index, DataBuffer &data, RecordCommon &meta);

    /**
     * @brief Used internally to validate as sector. Only used for off-device unit tests.
     * 
     * @param pSector 
     * @return true on success or false on failure
     * 
     * This method validates all of the fields in the sector and assures that the data on flash
     * matches the internal cache. It's only used during off-device unit tests, and will assert
     * if the sector is not valid.
     * 
     * On-device it just always returns true.
     */
    bool validateSector(Sector *pSector);

    /**
     * @brief Used internally to find a sector number for a sequence number
     * 
     * @param sequence 
     * @param sectorNum 
     * @return true on success or false on failure
     */
    bool sequenceToSectorNum(uint32_t sequence, uint16_t &sectorNum) const;


    /**
     * @brief Convert a sector number to an address
     * 
     * @param sectorNum 0 is the first sector of this buffer, not the device! 
     * @return uint32_t The byte address in in the device for the beginning of this sector
     */
    uint32_t sectorNumToAddr(uint16_t sectorNum) const { return addrStart + sectorNum * spiFlash->getSectorSize(); };

    /**
     * @brief Remove entries from the sector cache
     */
    void clearCache();

    static const uint32_t SECTOR_MAGIC = 0x0ceb6443; //!< Magic bytes stored at beginning of SectorHeader structure
    static const uint32_t SECTOR_MAGIC_ERASED = 0xffffffff; //!< Magic bytes value if the sector is erased and not formatted.
    static const unsigned int SECTOR_FLAG_STARTED_MASK = 0x01; //!< Bit that is cleared when a sector is first written to after formatting
    static const unsigned int SECTOR_FLAG_FINALIZED_MASK = 0x02; //!< Bit that is cleared when a sector has been fully written to
    static const unsigned int SECTOR_FLAG_CORRUPTED_MASK = 0x04; //!< Bit that is cleared when a sector has invalid record structures

    static const unsigned int RECORD_SIZE_ERASED = 0xfff; //!< Record size value when there is no record at this location. This is the value of the 12-bit value when the sector is erased.
    static const unsigned int RECORD_FLAG_READ_MASK = 0x1; //!< Bit that is cleared when a record has been read.

    
    /**
     * @brief Number of cached Sector structures used by getSector
     * 
     * The Sector structure does not contain the data, so this is not a lot of RAM, but can add up
     * especially if you are storing small records because there's a vector of RecordCommon structures,
     * one for each record.
     * 
     * The cache exists because indexing a Sector requires n + 2 SPI reads where n is the number 
     * of records, so this can be a lot of transactions if you have small records.
     */
    static const size_t SECTOR_CACHE_SIZE = 8;

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
    SpiFlash *spiFlash; //!< The class to access the SPI flash chip
    size_t addrStart; //!< Address in SPI flash where circular buffer begins, must be sector aligned
    size_t addrEnd; //!< Address in SPI flash where circular buffer ends, must be sector aligned
    size_t sectorCount; //!< Calculated in constructor, number of sectors from addrStart to addrEnd

    SectorCommon *sectorMeta = nullptr; //!< Array of SectorCommon structures, one for each sector.


    bool isValid = false; //!< true once load() or format() has been called and is successful
    std::deque<Sector*> sectorCache; //!< Cache used by getSector()

    uint32_t firstSequence = 0; //!< Sequence number of read from
    uint32_t writeSequence = 0; //!< Sequence number to write to
    uint32_t lastSequence = 0; //!< Last sequence number used.

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
