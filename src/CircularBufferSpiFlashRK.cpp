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
    _log.trace("addrStart=0x%x addrEnd=0x%x sectorSize=%d sectorCount=%d", (int)addrStart, (int)addrEnd, (int)spiFlash->getSectorSize(), (int)sectorCount);

    // SectorCommon structure is 8 bytes
    // A 1 MB flash chip has 256 sectors (4096 bytes each), so sectorMeta would be 2048 bytes.
    // This is a reasonable allocation as it greatly reduces the number of reads during normal operation.
    sectorMeta = new SectorCommon[sectorCount];
    if (!sectorMeta) {
        _log.error("could not allocate sectorMeta sectorCount=%d", (int)sectorCount);
    }

}

CircularBufferSpiFlashRK::~CircularBufferSpiFlashRK() {
    while(!sectorCache.empty()) {
        delete sectorCache.back();
        sectorCache.pop_back();
    }

}

bool CircularBufferSpiFlashRK::load() {
    bool bResult = true;

    WITH_LOCK(*this) {
        isValid = false;

        if (!sectorMeta) {
            _log.error("sectorMeta not allocated");
            return false;
        }

        for(int sectorIndex = 0; sectorIndex < (int)sectorCount; sectorIndex++) {
            SectorHeader sectorHeader;

            spiFlash->readData(addrStart + sectorIndex * spiFlash->getSectorSize(), &sectorHeader, sizeof(SectorHeader));
            sectorMeta[sectorIndex] = sectorHeader.c;

            if (sectorHeader.sectorMagic == SECTOR_MAGIC) {

                if (sectorHeader.c.sequence > lastSequence) {
                    lastSequence = sectorHeader.c.sequence;
                }

                // _log.trace("loading sectorIndex=%d sequence=%d flags=0x%x", sectorIndex, (int)sectorHeader.c.sequence, (int)sectorHeader.c.flags);
            }
            else {
                _log.error("sector %d invalid magic 0x%x", (int)sectorIndex, (int)sectorHeader.sectorMagic);
                sectorMeta[sectorIndex].flags &= ~SECTOR_FLAG_CORRUPTED_MASK;

                bResult = false;            
            }
        }

        isValid = true;

        // TODO: Check that sequence numbers are reasonable

        SectorInfo sectorInfo;
        getSectorInfo(sectorInfo);
        sectorInfo.log(LOG_LEVEL_TRACE, "load");
    }

    return bResult;
}


bool CircularBufferSpiFlashRK::format() {
    bool bResult = false;

    WITH_LOCK(*this) {

        uint32_t sequence = 1;

        for(uint16_t sectorNum = 0; sectorNum < sectorCount; sectorNum++) {
            writeSectorHeader(sectorNum, true /* erase */, sequence++);
        }

        bResult = load();
    }

    return bResult;
}

bool CircularBufferSpiFlashRK::fsck() {
    WITH_LOCK(*this) {
    }

    return true;
}



CircularBufferSpiFlashRK::Sector *CircularBufferSpiFlashRK::getSectorFromCache(uint16_t sectorNum) {
    CircularBufferSpiFlashRK::Sector *pSector = nullptr;
    sectorNum %= sectorCount;


    for(auto iter = sectorCache.begin(); iter != sectorCache.end(); iter++) {
        if ((*iter)->sectorNum == sectorNum) {
            // Found in cache
            pSector = *iter;
            break;
        }
    }
    return pSector;
}


CircularBufferSpiFlashRK::Sector *CircularBufferSpiFlashRK::getSector(uint16_t sectorNum) {
    sectorNum %= sectorCount;

    CircularBufferSpiFlashRK::Sector *pSector = getSectorFromCache(sectorNum);

    if (!pSector) {
        // Not found in cache
        if (sectorCache.size() >= SECTOR_CACHE_SIZE) {
            delete sectorCache.back();
            sectorCache.pop_back();
        }
        pSector = new Sector();
        if (pSector) {
            readSector(sectorNum, pSector);
            sectorCache.push_front(pSector);
        }
        else {
            _log.error("getSector could not allocate");
        }        
    }

    return pSector;
}




bool CircularBufferSpiFlashRK::readSector(uint16_t sectorNum, Sector *sector) {
    sectorNum %= sectorCount;

    if (!isValid) {
        _log.error("%s not isValid", "readSector");
        return false;
    }

    size_t addr = sectorNumToAddr(sectorNum);

    sector->clear(sectorNum);

    sector->c = sectorMeta[sectorNum];
    /*
    // Read header
    SectorHeader sectorHeader;
    spiFlash->readData(addr, &sectorHeader, sizeof(SectorHeader));
    if (sectorHeader.sectorMagic == SECTOR_MAGIC_ERASED) { // 0xffffffff
        return false;
    }

    if (sectorHeader.sectorMagic != SECTOR_MAGIC) {
        _log.error("readSector invalid sectorMagic=%08x sectorNum=%d", (int)sectorHeader.sectorMagic, (int)sectorNum);
        return false;
    }

    sector->c = sectorHeader.c;
    */
    
    // Read records
    uint16_t offset = sizeof(SectorHeader);
    while((offset + sizeof(RecordCommon)) < spiFlash->getSectorSize()) {
        RecordCommon recordCommon;
        spiFlash->readData(addr + offset, &recordCommon, sizeof(RecordCommon));
        
        if (recordCommon.size == 0xffff) {
            // Erased, no more data
            break;
        }

        const char *corruptedError = nullptr;


        if (recordCommon.size >= (spiFlash->getSectorSize() - sizeof(RecordCommon) - sizeof(SectorHeader))) {
            corruptedError = "invalid size";
        }

        uint16_t nextOffset = offset + sizeof(RecordCommon) + recordCommon.size;
        if (nextOffset > spiFlash->getSectorSize()) {
            corruptedError = "invalid offset";
        }

        if (corruptedError) {
            sectorMeta[sectorNum].flags &= ~SECTOR_FLAG_CORRUPTED_MASK;
            sector->c = sectorMeta[sectorNum];
            _log.error("readSector corrupted %s sectorNum=%d offset=%d size=%d", corruptedError, (int)sectorNum, (int)offset, (int)recordCommon.size);
            return false;
        }

        sector->records.push_back(recordCommon);

        offset = nextOffset;
    }
    
    return true;
}

bool CircularBufferSpiFlashRK::writeSectorHeader(uint16_t sectorNum, bool erase, uint32_t sequence) {

    // Don't check isValid here, because this function is used to format flash. before it's valid

    sectorNum %= sectorCount;

    size_t addr = sectorNumToAddr(sectorNum);

    // _log.trace("writeSectorHeader sectorNum=%d addr=0x%x sequence=%d", (int)sectorNum, (int)addr, (int)sequence);

    if (erase) {
        spiFlash->sectorErase(addr);
    }

    // Update SPI flash
    SectorHeader sectorHeader;
    sectorHeader.sectorMagic = SECTOR_MAGIC;
    sectorHeader.c.sequence = sequence;    
    sectorHeader.c.flags = ~0;
    sectorHeader.c.reserved = ~0;
    spiFlash->writeData(addr, &sectorHeader, sizeof(SectorHeader));

    // Update metadata in RAM
    sectorMeta[sectorNum] = sectorHeader.c;

    // Update cache
    Sector *pSector = getSectorFromCache(sectorNum);
    if (pSector) {
        pSector->c = sectorHeader.c;
    }

    return true;
}



bool CircularBufferSpiFlashRK::appendDataToSector(Sector *sector, const DataBuffer &data, uint16_t flags) {

    if (!isValid) {
        _log.error("%s not isValid", "appendDataToSector");
        return false;
    }

    size_t addr = sectorNumToAddr(sector->sectorNum);

    uint16_t offset = sector->getLastOffset();

    uint16_t spaceLeft = spiFlash->getSectorSize() - offset;
    if ((data.size() + sizeof(RecordCommon)) > spaceLeft) {
        return false;
    }


    RecordCommon recordCommon;
    recordCommon.flags = flags;
    recordCommon.size = data.size();
    sector->records.push_back(recordCommon);

    spiFlash->writeData(addr + offset, &recordCommon, sizeof(RecordCommon));

    spiFlash->writeData(addr + offset + sizeof(RecordCommon), data.getBuffer(), data.size());

    return true;
}

bool CircularBufferSpiFlashRK::finalizeSector(Sector *sector) {
    if (!isValid) {
        _log.error("%s not isValid", "finalizeSector");
        return false;
    }

    sector->c.flags &= ~SECTOR_FLAG_FINALIZED_MASK;
    sector->c.recordCount = sector->c.dataSize = 0;

    for(auto iter = sector->records.begin(); iter != sector->records.end(); iter++) {
        sector->c.recordCount++;
        sector->c.dataSize += iter->size;
    }

    size_t addr = sectorNumToAddr(sector->sectorNum);
    spiFlash->writeData(addr, &sector->c, sizeof(SectorHeader));

    sectorMeta[sector->sectorNum] = sector->c;

    return true;
}


bool CircularBufferSpiFlashRK::readDataFromSector(Sector *sector, size_t index, DataBuffer &data, RecordCommon &meta) {
    if (!isValid) {
        _log.error("%s not isValid", "readDataFromSector");
        return false;
    }

    bool bResult = false;

    size_t addr = sectorNumToAddr(sector->sectorNum);

    size_t curIndex = 0;

    uint16_t offset = sizeof(SectorHeader);
    for(auto iter = sector->records.begin(); iter != sector->records.end(); iter++, curIndex++) {
        if (index == curIndex) {
            uint8_t *dataBuf = data.allocate(iter->size);
            spiFlash->readData(addr + offset + sizeof(RecordCommon), dataBuf, data.size());

            meta = *iter;
            bResult = true;
            break;
        }
        offset += sizeof(RecordCommon) + iter->size;
    }

    return bResult;
}


bool CircularBufferSpiFlashRK::getSectorInfo(SectorInfo &sectorInfo) const {

    if (!isValid) {
        _log.error("%s not isValid", "getSectorInfo");
        return false;
    }
    sectorInfo.firstSector = sectorInfo.lastSector = 0;

    for(uint16_t sectorNum = 0; sectorNum < sectorCount; sectorNum++) {
        if ((sectorMeta[sectorNum].flags & SECTOR_FLAG_CORRUPTED_MASK) == 0) {
            // This sector is corrupted, ignore
            continue;
        }

        if (sectorMeta[sectorNum].sequence < sectorMeta[sectorInfo.firstSector].sequence) {
            sectorInfo.firstSector = sectorNum;
        }
        if (sectorMeta[sectorNum].sequence > sectorMeta[sectorInfo.lastSector].sequence) {
            sectorInfo.lastSector = sectorNum;
        }        
    }

    if (sectorInfo.firstSector > sectorInfo.lastSector) {
        // Wraps around
        sectorInfo.lastSector += sectorCount;
    }

    sectorInfo.writeSector = sectorInfo.firstSector;

    for(uint16_t sectorNum = sectorInfo.firstSector; sectorNum < sectorInfo.lastSector; sectorNum++) {
        // Note: sectorNum may be > sectorCount because of wrapping!
        if ((sectorMeta[sectorNum % sectorCount].flags & SECTOR_FLAG_FINALIZED_MASK) == SECTOR_FLAG_FINALIZED_MASK) {
            // Finalized bit is not cleared, so this sector has not been finalized
            sectorInfo.writeSector = sectorNum;
            break;
        }
    }
    
    return true;
}

void CircularBufferSpiFlashRK::SectorInfo::log(LogLevel level, const char *msg) const {

    _log.log(level, "%s firstSector=%d lastSector=%d writeSector=%d", msg, (int)firstSector, (int)lastSector, (int)writeSector);

}

bool CircularBufferSpiFlashRK::readData(DataInfo &dataInfo) {
    bool bResult = false;

    if (!isValid) {
        _log.error("%s not isValid", "readData");
        return false;
    }

    WITH_LOCK(*this) {

        SectorInfo sectorInfo;
        if (!getSectorInfo(sectorInfo)) {
            return false;
        }

        dataInfo.sectorNum = sectorInfo.firstSector;

        Sector *pSector = getSector(dataInfo.sectorNum);
        if (!pSector) {
            return false;
        }

        dataInfo.sectorCommon = pSector->c;

        size_t addr = sectorNumToAddr(dataInfo.sectorNum);

        dataInfo.index = 0;

        uint16_t offset = sizeof(SectorHeader);
        for(auto iter = pSector->records.begin(); iter != pSector->records.end(); iter++, dataInfo.index++) {
            if ((iter->flags & RECORD_FLAG_READ_MASK) == RECORD_FLAG_READ_MASK) {
                // Not marked as read
                uint8_t *dataBuf = dataInfo.allocate(iter->size);
                spiFlash->readData(addr + offset + sizeof(RecordCommon), dataBuf, dataInfo.size());

                dataInfo.recordCommon = *iter;
                bResult = true;
                break;
            }
            offset += sizeof(RecordCommon) + iter->size;
        }
    }

    return bResult;
}

bool CircularBufferSpiFlashRK::markAsRead(const DataInfo &dataInfo) {
    bool bResult = false;
    if (!isValid) {
        _log.error("%s not isValid", "markAsRead");
        return false;
    }

    WITH_LOCK(*this) {

        Sector *pSector = getSector(dataInfo.sectorNum);
        if (!pSector) {
            return false;
        }

        if (pSector->c.sequence != dataInfo.sectorCommon.sequence) {
            _log.info("sector %d reused, not marking as read", (int)dataInfo.sectorNum);
            return false;
        }

        size_t addr = sectorNumToAddr(dataInfo.sectorNum);

        if ((dataInfo.index + 1) >= pSector->records.size()) {
            // This is the last record in the sector, erase the sector
            writeSectorHeader(dataInfo.sectorNum, true /* erase */, ++lastSequence);
        }
        else {
            // Just mark this record as read
            size_t curIndex = 0;
            uint16_t offset = sizeof(SectorHeader);
            for(auto iter = pSector->records.begin(); iter != pSector->records.end(); iter++, curIndex++) {
                if (curIndex == dataInfo.index) {
                    iter->flags &= ~RECORD_FLAG_READ_MASK;
                    spiFlash->writeData(addr + offset, &(*iter), sizeof(RecordCommon));
                }
            }
        }
        bResult = true;
    }

    return bResult;
}

void CircularBufferSpiFlashRK::DataInfo::log(LogLevel level, const char *msg) const {
    _log.log(level, "%s sectorNum=%d sequence=%d flags=0x%x, recordIndex=%d", msg, (int)sectorNum, (int)sectorCommon.sequence, (int)sectorCommon.flags, (int)index);
}


bool CircularBufferSpiFlashRK::writeData(const DataBuffer &data) {
    bool bResult = false;
    if (!isValid) {
        _log.error("%s not isValid", "writeData");
        return false;
    }

    WITH_LOCK(*this) {

        SectorInfo sectorInfo;
        if (!getSectorInfo(sectorInfo)) {
            return false;
        }
        uint16_t sectorNum = sectorInfo.writeSector;
        
        Sector *pSector = getSector(sectorNum);
        if (!pSector) {
            return false;
        }

        bResult = appendDataToSector(pSector, data, ~0);
        if (!bResult) {
            // Sector is full, finalize this sector
            finalizeSector(pSector);

            // Start a new one
            uint32_t lastSequence = pSector->c.sequence;
            sectorNum++; // May wrap around

            pSector = getSector(sectorNum);
            if (!pSector) {
                return false;
            }

            if (pSector->c.sequence <= lastSequence) {
                // Wrapped around to a sector that hasn't been read yet
                // writeSectorHeader updates pSector since it will be in the cache
                writeSectorHeader(sectorNum, true /* erase */, ++lastSequence);
            }

            // Write data to the new sector
            bResult = appendDataToSector(pSector, data, ~0);
        }
    }

    return bResult;
}

bool CircularBufferSpiFlashRK::getUsageStats(UsageStats &usageStats) {
    bool bResult = false;
    if (!isValid) {
        _log.error("%s not isValid", "writeData");
        return false;
    }

    WITH_LOCK(*this) {
    }
    return bResult;
}

void CircularBufferSpiFlashRK::UsageStats::log(LogLevel level, const char *msg) const {
    _log.log(level, "%s", msg);
    
}


void CircularBufferSpiFlashRK::Sector::clear(uint16_t sectorNum) {
    this->sectorNum = sectorNum;
    this->records.clear();
    memset(&this->c, 0, sizeof(SectorCommon));
}


uint16_t CircularBufferSpiFlashRK::Sector::getLastOffset() const {
    uint16_t lastOffset = sizeof(SectorHeader);
    for(auto iter = records.begin(); iter != records.end(); iter++) {
        lastOffset += sizeof(RecordCommon) + iter->size;
    }
    return lastOffset;
}



void CircularBufferSpiFlashRK::Sector::log(LogLevel level, const char *msg, bool includeData) const {
    uint16_t lastOffset = getLastOffset();

    // bool isPrintable = true;
    if (includeData) {
        for(auto iter = records.begin(); iter != records.end(); iter++) {
        }
    }

    _log.trace("logSector %s sectorNum=%d flags=0x%x sequence=%d lastOffset=%d", msg, (int)sectorNum, (int)c.flags, (int)c.sequence, (int)lastOffset); 
    if ((c.flags & SECTOR_FLAG_FINALIZED_MASK) == 0) {
        _log.log(level, " finalized recordCount=%d dataSize=%d", (int)c.recordCount, (int)c.dataSize); 
    }


    uint16_t offset = sizeof(SectorHeader);
    for(auto iter = records.begin(); iter != records.end(); iter++) {
        _log.log(level, " record offset=%d size=%d flags=%x", (int)offset, (int)iter->size, (int)iter->flags);        
        offset += sizeof(RecordCommon) + iter->size;
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
    len = 0;
}

void CircularBufferSpiFlashRK::DataBuffer::copy(const void *buf, size_t len) {
    free();

    if (len > 0) {
        this->buf = new uint8_t[len];
        if (buf && this->buf) {
            memcpy(this->buf, buf, len);
        }
    }
    this->len = len;
}

void CircularBufferSpiFlashRK::DataBuffer::copy(const char *str) {
    if (str) {
        copy(str, strlen(str) + 1);
    }
    else {
        free();
    }
}

uint8_t *CircularBufferSpiFlashRK::DataBuffer::allocate(size_t len) {
    copy(nullptr, len);
    return this->buf;
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

uint8_t CircularBufferSpiFlashRK::DataBuffer::getByIndex(size_t index) const {
    if (buf && index < len) {
        return buf[index];
    }
    else {
        return 0;
    }
}

