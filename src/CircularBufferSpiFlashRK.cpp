#include "CircularBufferSpiFlashRK.h"

static Logger _log("app.circ");

CircularBufferSpiFlashRK::CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd) :
    spiFlash(spiFlash), addrStart(addrStart), addrEnd(addrEnd) {

#ifndef UNITTEST
    os_mutex_create(&mutex);
#endif

    if ((addrStart % spiFlash->getSectorSize()) != 0) {
        _log.error("addrStart is not sector aligned addr=%d sectorSize=%d", (int)addrStart, (int)spiFlash->getSectorSize());
    }
    if ((addrEnd % spiFlash->getSectorSize()) != 0) {
        _log.error("addrEnd is not sector aligned addr=%d sectorSize=%d", (int)addrEnd, (int)spiFlash->getSectorSize());
    }
    sectorCount = (addrEnd - addrStart) / spiFlash->getSectorSize();

}

CircularBufferSpiFlashRK::~CircularBufferSpiFlashRK() {
    while(!sectorCache.empty()) {
        delete sectorCache.back();
        sectorCache.pop_back();
    }

}

bool CircularBufferSpiFlashRK::load() {
    SectorCommon *scanData = new SectorCommon[sectorCount];
    if (!scanData) {
        _log.error("could not allocate scanData sectorCount=%d", (int)sectorCount);
        return false;
    }

    firstSector = -1;
    firstSectorSequence = 0;

    lastSector = -1;
    lastSectorSequence = 0;

    for(int sectorIndex = 0; sectorIndex < sectorCount; sectorIndex++) {
        SectorHeader sectorHeader;

        spiFlash->readData(addrStart + sectorIndex * spiFlash->getSectorSize(), &sectorHeader, sizeof(SectorHeader));
        if (sectorHeader.sectorMagic == SECTOR_MAGIC) {
            scanData[sectorIndex] = sectorHeader.c;

            _log.trace("loading sectorIndex=%d sequence=%d flags=0x%x", sectorIndex, (int)sectorHeader.c.sequence, (int)sectorHeader.c.flags);

            if (firstSector < 0 || sectorHeader.c.sequence < firstSectorSequence) {
                firstSector = sectorIndex;
                firstSectorSequence = sectorHeader.c.sequence ;
            }
            if (lastSector < 0 || sectorHeader.c.sequence > lastSectorSequence) {
                lastSector = sectorIndex;
                lastSectorSequence = sectorHeader.c.flags ;
            }
        }
    }
    if (firstSector < 0) {
        _log.trace("flash is empty");

        firstSector = lastSector = 0;
        firstSectorSequence = lastSectorSequence = 1;
        writeSectorHeader(firstSector, true, firstSectorSequence);
    }

    currentReadSector = getSector(firstSector);

    if (firstSector != lastSector) {
        currentWriteSector = getSector(lastSector);
    }
    else {
        currentWriteSector = currentReadSector;
    }

    return true;
}

bool CircularBufferSpiFlashRK::erase() {

    for(size_t addr = addrStart; addr < addrEnd; addr += spiFlash->getSectorSize()) {
        spiFlash->sectorErase(addr);
    }
    return true;
}

CircularBufferSpiFlashRK::Sector *CircularBufferSpiFlashRK::getSector(uint16_t sectorNum) {
    CircularBufferSpiFlashRK::Sector *pSector = nullptr;
    sectorNum %= sectorCount;

    if (currentReadSector && currentReadSector->sectorNum == sectorNum) {
        pSector = currentReadSector;
    }
    else
    if (currentWriteSector && currentWriteSector->sectorNum == sectorNum) {
        pSector = currentWriteSector;
    }
    else {
        for(auto iter = sectorCache.begin(); iter != sectorCache.end(); iter++) {
            if ((*iter)->sectorNum == sectorNum) {
                // Found in cache
                pSector = *iter;
                break;
            }
        }
    }

    if (!pSector) {
        // Not found in cache
        if (sectorCache.size() >= SECTOR_CACHE_SIZE) {
            delete sectorCache.back();
            sectorCache.pop_back();
        }
        pSector = new Sector();
        if (pSector) {
            readSector(sectorNum, pSector);
            sectorCache.push_back(pSector);
        }
        else {
            _log.error("getSector could not allocate");
        }        
    }

    return pSector;
}



bool CircularBufferSpiFlashRK::readSector(uint16_t sectorNum, Sector *sector) {

    size_t addr = sectorNumToAddr(sectorNum);

    sector->clear(sectorNum);

    // Read header
    SectorHeader sectorHeader;
    spiFlash->readData(addr, &sectorHeader, sizeof(SectorHeader));
    if (sectorHeader.sectorMagic == SECTOR_MAGIC_ERASED) { // 0xffffffff
        sector->internalFlags |= SECTOR_INTERNAL_FLAG_ERASED;
        return false;
    }

    if (sectorHeader.sectorMagic != SECTOR_MAGIC) {
        sector->internalFlags |= SECTOR_INTERNAL_FLAG_CORRUPTED;
        _log.error("readSector invalid sectorMagic sectorNum=%d", (int)sectorNum);
        return false;
    }
    
    static const uint32_t SECTOR_INTERNAL_FLAG_ERASED = 0x0001;
    static const uint32_t SECTOR_INTERNAL_FLAG_CORRUPTED = 0x0002;


    sector->c = sectorHeader.c;

    // Read records
    uint16_t offset = sizeof(SectorHeader);
    while((offset + sizeof(RecordHeader)) < spiFlash->getSectorSize()) {
        RecordHeader recordHeader;
        spiFlash->readData(addr + offset, &recordHeader, sizeof(RecordHeader));
        
        if (recordHeader.recordMagic == RECORD_MAGIC_ERASED) { // 0xffffffff
            // Erased, no more data
            break;
        }

        if (recordHeader.recordMagic != RECORD_MAGIC) {
            sector->internalFlags |= SECTOR_INTERNAL_FLAG_CORRUPTED;
            _log.error("readSector invalid recordMagic=%08x sectorNum=%d offset=%d", (int)recordHeader.recordMagic, (int)sectorNum, (int)offset);
            return false;
        }

        if (recordHeader.c.size >= (spiFlash->getSectorSize() - sizeof(RecordHeader) - sizeof(SectorHeader))) {
            sector->internalFlags |= SECTOR_INTERNAL_FLAG_CORRUPTED;
            _log.error("readSector invalid size sectorNum=%d offset=%d size=%d", (int)sectorNum, (int)offset, (int)recordHeader.c.size);
            return false;
        }

        uint16_t nextOffset = offset + sizeof(RecordHeader) + recordHeader.c.size;
        if (nextOffset > spiFlash->getSectorSize()) {
            sector->internalFlags |= SECTOR_INTERNAL_FLAG_CORRUPTED;
            _log.error("readSector invalid offset sectorNum=%d offset=%d size=%d", (int)sectorNum, (int)offset, (int)recordHeader.c.size);
            return false;
        }
        Record record;
        record.offset = offset;
        record.c = recordHeader.c;
        sector->records.push_back(record);

        offset = nextOffset;
    }
    
    sector->internalFlags |= SECTOR_INTERNAL_FLAG_VALID;

    return true;
}

bool CircularBufferSpiFlashRK::writeSectorHeader(uint16_t sectorNum, bool erase, uint32_t sequence) {
    sectorNum %= sectorCount;

    size_t addr = sectorNumToAddr(sectorNum);

    if (erase) {
        spiFlash->sectorErase(addr);
    }

    SectorHeader sectorHeader;
    sectorHeader.sectorMagic = SECTOR_MAGIC;
    sectorHeader.c.sequence = sequence;    
    sectorHeader.c.flags = (uint16_t) ~SECTOR_FLAG_HEADER_MASK; // 0xfffe
    sectorHeader.c.reserved = (uint16_t) ~0;
    spiFlash->writeData(addr, &sectorHeader, sizeof(SectorHeader));

    return true;
}

bool CircularBufferSpiFlashRK::appendToSector(Sector *sector, const void *data, size_t dataLen, uint16_t flags, bool write) {

    size_t addr = sectorNumToAddr(sector->sectorNum);

    uint16_t offset = sector->getLastOffset();

    uint16_t spaceLeft = spiFlash->getSectorSize() - offset;
    if ((dataLen + sizeof(RecordHeader)) > spaceLeft) {
        return false;
    }

    Record record;
    record.offset = offset;
    record.c.flags = flags;
    record.c.size = dataLen;
    sector->records.push_back(record);

    if (write) {
        RecordHeader recordHeader;
        recordHeader.recordMagic = RECORD_MAGIC;
        recordHeader.c = record.c;
        spiFlash->writeData(addr + offset, &recordHeader, sizeof(RecordHeader));

        spiFlash->writeData(addr + offset + sizeof(RecordHeader), data, dataLen);
    }

    return true;
}



void CircularBufferSpiFlashRK::Sector::clear(uint16_t sectorNum) {
    this->sectorNum = sectorNum;
    this->internalFlags = 0;
    this->records.clear();
    memset(&this->c, 0, sizeof(SectorCommon));
}


uint16_t CircularBufferSpiFlashRK::Sector::getLastOffset() const {
    uint16_t lastOffset = sizeof(SectorHeader);
    for(auto iter = records.begin(); iter != records.end(); iter++) {
        lastOffset = iter->offset + sizeof(RecordHeader) + iter->c.size;
    }
    return lastOffset;
}



void CircularBufferSpiFlashRK::Sector::log(const char *msg, bool includeData) const {
    uint16_t lastOffset = getLastOffset();

    bool isPrintable = true;
    if (includeData) {
        for(auto iter = records.begin(); iter != records.end(); iter++) {
        }
    }

    _log.trace("logSector %s sectorNum=%d flags=0x%x sequence=%d lastOffset=%d", msg, (int)sectorNum, (int)c.flags, (int)c.sequence, (int)lastOffset); 

    for(auto iter = records.begin(); iter != records.end(); iter++) {
        _log.trace(" record offset=%d size=%d flags=%x", (int)iter->offset, (int)iter->c.size, (int)iter->c.flags);        
    }
}


CircularBufferSpiFlashRK::DataBuffer::DataBuffer() : buf(nullptr), len(0) {

}

CircularBufferSpiFlashRK::DataBuffer::~DataBuffer() {
    if (buf) {
        delete[] buf;
        buf = nullptr;
    }
}

CircularBufferSpiFlashRK::DataBuffer::DataBuffer(const void *buf, size_t len) {
    this->buf = nullptr;
    copy(buf, len);
}

CircularBufferSpiFlashRK::DataBuffer::DataBuffer(const DataBuffer &other) {
    this->buf = nullptr;
    copy(other.buf, other.len);
}

CircularBufferSpiFlashRK::DataBuffer::DataBuffer(const char *str) {
    this->buf = nullptr;
    copy(str);    
}

CircularBufferSpiFlashRK::DataBuffer &CircularBufferSpiFlashRK::DataBuffer::operator=(const DataBuffer &other) {
    this->buf = nullptr;
    copy(other.buf, other.len);
    return *this;
}

void CircularBufferSpiFlashRK::DataBuffer::free() {
    if (buf) {
        delete[] this->buf;
        this->buf = nullptr;
    }
    len = 0 ;
}

void CircularBufferSpiFlashRK::DataBuffer::copy(const void *buf, size_t len) {
    free();

    if (len > 0) {
        this->buf = new uint8_t[len];
        if (this->buf) {
            memcpy(this->buf, buf, len);
        }
    }
    this->len = len;
}

void CircularBufferSpiFlashRK::DataBuffer::copy(const char *str) {
    copy(str, strlen(str) + 1);
}


bool CircularBufferSpiFlashRK::DataBuffer::operator==(const DataBuffer &other) const {
    if (buf && other.buf) {
        return len == other.len && memcmp(buf, other.buf, len) == 0;
    }
    else {
        return false;
    }
}

bool CircularBufferSpiFlashRK::DataBuffer::equals(const char *str) const {
    if (buf) {
        return strcmp((const char *)buf, str) == 0;
    }
    else {
        return false;
    }
}

const char *CircularBufferSpiFlashRK::DataBuffer::c_str() const {
    static char nullValue = 0;

    if (!buf || len == 0 || buf[len - 1] != 0) {
        return &nullValue;
    }
    else {
        return (const char *)buf;
    }
}
