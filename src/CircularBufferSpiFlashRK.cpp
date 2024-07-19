#include "CircularBufferSpiFlashRK.h"

struct SectorScanData {
    uint32_t sequence;
    uint32_t flags;
};

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
    SectorScanData *scanData = new SectorScanData[sectorCount];
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
            scanData[sectorIndex].sequence = sectorHeader.sequence;
            scanData[sectorIndex].flags = sectorHeader.flags;

            Log.trace("loading sectorIndex=%d sequence=%d flags=0x%x", sectorIndex, (int)sectorHeader.sequence, (int)sectorHeader.flags);

            if (firstSector < 0 || sectorHeader.sequence < firstSectorSequence) {
                firstSector = sectorIndex;
                firstSectorSequence = sectorHeader.sequence ;
            }
            if (lastSector < 0 || sectorHeader.sequence > lastSectorSequence) {
                lastSector = sectorIndex;
                lastSectorSequence = sectorHeader.sequence ;
            }
        }
    }
    if (firstSector < 0) {
        Log.trace("flash is empty");

        firstSector = lastSector = 0;
        firstSectorSequence = lastSectorSequence = 1;
        writeSectorHeader(firstSector, true, firstSectorSequence);
    }
    
    Log.trace("firstSector=%d firstSectorSequence=%d lastSector=%d lastSectorSequence=%d", firstSector, (int)firstSectorSequence, lastSector, (int)lastSectorSequence);
    readSector(firstSector, currentReadSector);
    logSector(currentReadSector, "firstSector");


    readSector(lastSector, currentWriteSector);
     

    return true;
}

bool CircularBufferSpiFlashRK::erase() {

    for(size_t addr = addrStart; addr < addrEnd; addr += spiFlash->getSectorSize()) {
        spiFlash->sectorErase(addr);
    }
    return true;
}


bool CircularBufferSpiFlashRK::readSector(uint16_t sectorNum, Sector &sector) {
    sectorNum %= sectorCount;

    size_t addr = sectorNumToAddr(sectorNum);

    // Read header
    SectorHeader sectorHeader;
    spiFlash->readData(addr, &sectorHeader, sizeof(SectorHeader));
    if (sectorHeader.sectorMagic != SECTOR_MAGIC) {
        Log.error("readSector invalid sectorMagic sectorNum=%d", (int)sectorNum);
        return false;
    }
    
    sector.sectorNum = sectorNum;
    sector.flags = sectorHeader.flags;
    sector.sequence = sectorHeader.sequence;
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

        if (recordHeader.size >= (spiFlash->getSectorSize() - sizeof(RecordHeader) - sizeof(SectorHeader))) {
            Log.error("readSector invalid size sectorNum=%d offset=%d size=%d", (int)sectorNum, (int)offset, (int)recordHeader.size);
            return false;
        }

        uint16_t nextOffset = offset + sizeof(RecordHeader) + recordHeader.size;
        if (nextOffset > spiFlash->getSectorSize()) {
            Log.error("readSector invalid offset sectorNum=%d offset=%d size=%d", (int)sectorNum, (int)offset, (int)recordHeader.size);
            return false;
        }
        Record record;
        record.offset = offset;
        record.size = recordHeader.size;
        record.flags = recordHeader.flags;
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
    sectorHeader.sequence = sequence;    
    sectorHeader.flags = (uint16_t) ~SECTOR_FLAG_HEADER_MASK; // 0xfffe
    sectorHeader.reserved = (uint16_t) ~0;
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
    record.flags = flags;
    record.size = dataLen;
    sector.records.push_back(record);

    if (write) {
        RecordHeader recordHeader;
        recordHeader.recordMagic = RECORD_MAGIC;
        recordHeader.flags = flags;
        recordHeader.size = dataLen;

        spiFlash->writeData(addr + offset, &recordHeader, sizeof(RecordHeader));
        spiFlash->writeData(addr + offset + sizeof(RecordHeader), data, dataLen);
    }

    return true;
}

uint16_t CircularBufferSpiFlashRK::getLastOffset(const Sector &sector) const {
    uint16_t lastOffset = sizeof(SectorHeader);
    for(auto iter = sector.records.begin(); iter != sector.records.end(); iter++) {
        lastOffset = iter->offset + iter->size;
    }
    return lastOffset;
}

void CircularBufferSpiFlashRK::logSector(const Sector &sector, const char *msg) const {
    uint16_t lastOffset = getLastOffset(sector);

    Log.trace("logSector %s sectorNum=%d flags=0x%x sequence=%d lastOffset=%d", msg, (int)sector.sectorNum, (int)sector.flags, (int)sector.sequence, (int)lastOffset); 

    for(auto iter = sector.records.begin(); iter != sector.records.end(); iter++) {
        Log.trace(" record offset=%d size=%d flags=%x", (int)iter->offset, (int)iter->size, (int)iter->flags);        
    }
}

