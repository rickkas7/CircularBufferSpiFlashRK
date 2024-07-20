#include "CircularBufferSpiFlashRK.h"

CircularBufferSpiFlashRK::CircularBufferSpiFlashRK(SpiFlash *spiFlash, size_t addrStart, size_t addrEnd) :
    spiFlash(spiFlash), addrStart(addrStart), addrEnd(addrEnd) {

#ifndef UNITTEST
    os_mutex_create(&mutex);
#endif

    if ((addrStart % spiFlash->getSectorSize()) != 0) {
        Log.error("addrStart is not sector aligned addr=%d sectorSize=%d", (int)addrStart, (int)spiFlash->getSectorSize());
    }
    if ((addrEnd % spiFlash->getSectorSize()) != 0) {
        Log.error("addrEnd is not sector aligned addr=%d sectorSize=%d", (int)addrEnd, (int)spiFlash->getSectorSize());
    }
    sectorCount = (addrEnd - addrStart) / spiFlash->getSectorSize();

}

CircularBufferSpiFlashRK::~CircularBufferSpiFlashRK() {

}

bool CircularBufferSpiFlashRK::load() {
    SectorCommon *scanData = new SectorCommon[sectorCount];
    if (!scanData) {
        Log.error("could not allocate scanData sectorCount=%d", (int)sectorCount);
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

            Log.trace("loading sectorIndex=%d sequence=%d flags=0x%x", sectorIndex, (int)sectorHeader.c.sequence, (int)sectorHeader.c.flags);

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
        Log.trace("flash is empty");

        firstSector = lastSector = 0;
        firstSectorSequence = lastSectorSequence = 1;
        writeSectorHeader(firstSector, true, firstSectorSequence);
    }

    /*    
    Log.trace("firstSector=%d firstSectorSequence=%d lastSector=%d lastSectorSequence=%d", firstSector, (int)firstSectorSequence, lastSector, (int)lastSectorSequence);
    readSector(firstSector, currentReadSector);
    logSector(currentReadSector, "firstSector");


    readSector(lastSector, currentWriteSector);
     */

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
        pSector = currentReadSector;
    }

    for(auto iter = sectorCache.begin(); iter != sectorCache.end(); iter++) {
        if ((*iter)->sectorNum == sectorNum) {
            // Found in cache
            pSector = *iter;
            break;
        }
    }
    if (!pSector) {
        // Not found in cache
        if (sectorCache.size() >= SECTOR_CACHE_SIZE) {
            delete sectorCache.back();
            sectorCache.pop_back();
        }
        pSector = new Sector();

        
    }

    return pSector;
}



bool CircularBufferSpiFlashRK::readSector(uint16_t sectorNum, Sector &sector) {

    size_t addr = sectorNumToAddr(sectorNum);

    // Read header
    SectorHeader sectorHeader;
    spiFlash->readData(addr, &sectorHeader, sizeof(SectorHeader));
    if (sectorHeader.sectorMagic != SECTOR_MAGIC) {
        Log.error("readSector invalid sectorMagic sectorNum=%d", (int)sectorNum);
        return false;
    }
    
    sector.sectorNum = sectorNum;
    sector.internalFlags = 0;
    sector.c = sectorHeader.c;
    sector.records.clear(); 

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
            Log.error("readSector invalid recordMagic=%08x sectorNum=%d offset=%d", (int)recordHeader.recordMagic, (int)sectorNum, (int)offset);
            return false;
        }

        if (recordHeader.c.size >= (spiFlash->getSectorSize() - sizeof(RecordHeader) - sizeof(SectorHeader))) {
            Log.error("readSector invalid size sectorNum=%d offset=%d size=%d", (int)sectorNum, (int)offset, (int)recordHeader.c.size);
            return false;
        }

        uint16_t nextOffset = offset + sizeof(RecordHeader) + recordHeader.c.size;
        if (nextOffset > spiFlash->getSectorSize()) {
            Log.error("readSector invalid offset sectorNum=%d offset=%d size=%d", (int)sectorNum, (int)offset, (int)recordHeader.c.size);
            return false;
        }
        Record record;
        record.offset = offset;
        record.c = recordHeader.c;
        sector.records.push_back(record);

        offset = nextOffset;
    }

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

bool CircularBufferSpiFlashRK::appendToSector(Sector &sector, const void *data, size_t dataLen, uint16_t flags, bool write) {

    size_t addr = sectorNumToAddr(sector.sectorNum);

    uint16_t offset = getLastOffset(sector);

    uint16_t spaceLeft = spiFlash->getSectorSize() - offset;
    if ((dataLen + sizeof(RecordHeader)) > spaceLeft) {
        return false;
    }

    Record record;
    record.offset = offset;
    record.c.flags = flags;
    record.c.size = dataLen;
    sector.records.push_back(record);

    if (write) {
        RecordHeader recordHeader;
        recordHeader.recordMagic = RECORD_MAGIC;
        recordHeader.c = record.c;
        spiFlash->writeData(addr + offset, &recordHeader, sizeof(RecordHeader));

        spiFlash->writeData(addr + offset + sizeof(RecordHeader), data, dataLen);
    }

    return true;
}

uint16_t CircularBufferSpiFlashRK::getLastOffset(const Sector &sector) const {
    uint16_t lastOffset = sizeof(SectorHeader);
    for(auto iter = sector.records.begin(); iter != sector.records.end(); iter++) {
        lastOffset = iter->offset + iter->c.size;
    }
    return lastOffset;
}

void CircularBufferSpiFlashRK::logSector(const Sector &sector, const char *msg) const {
    uint16_t lastOffset = getLastOffset(sector);

    Log.trace("logSector %s sectorNum=%d flags=0x%x sequence=%d lastOffset=%d", msg, (int)sector.sectorNum, (int)sector.c.flags, (int)sector.c.sequence, (int)lastOffset); 

    for(auto iter = sector.records.begin(); iter != sector.records.end(); iter++) {
        Log.trace(" record offset=%d size=%d flags=%x", (int)iter->offset, (int)iter->c.size, (int)iter->c.flags);        
    }
}

