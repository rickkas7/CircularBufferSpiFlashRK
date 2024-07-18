// Fake SpiFlash class for automated-tests
#pragma once 

#include "Particle.h"

class SpiFlash {
public:

	/**
	 * @brief Returns true if there is a flash chip present and it appears to be the correct manufacturer code.
	 */
	bool isValid();

	/**
	 * @brief Gets the JEDEC ID for the flash device.
	 *
	 * @return A 32-bit value containing the manufacturer ID and the two device IDs:
	 *
	 * byte[0] manufacturer ID mask 0x00ff0000
	 * byte[1] device ID 1     mask 0x0000ff00
	 * byte[2] device ID 2     mask 0x000000ff
	 */
	uint32_t jedecIdRead();

	/**
	 * @brief Reads the status register
	 */
	uint8_t readStatus();

	/**
	 * @brief Reads the configuration register (RDCR)
	 */
	uint8_t readConfiguration();

	/**
	 * @brief Checks the status register and returns true if a write is in progress
	 */
	bool isWriteInProgress();

	/**
	 * @brief Waits for any pending write operations to complete
	 *
	 * Waits up to waitWriteCompletionTimeoutMs milliseconds (default: 500) if
	 * not specified or 0. Otherwise, waits the specified number of milliseconds.
	 */
	void waitForWriteComplete(unsigned long timeout = 0);

	/**
	 * @brief Writes the status register.
	 */
	void writeStatus(uint8_t status);

	/**
	 * @brief Reads data synchronously. Reads data correctly across page boundaries.
	 *
	 * @param addr The address to read from
	 * @param buf The buffer to store data in
	 * @param bufLen The number of bytes to read
	 */
	void readData(size_t addr, void *buf, size_t bufLen);

	/**
	 * @brief Writes data synchronously. Can write data across page boundaries.
	 *
	 * @param addr The address to read from
	 * @param buf The buffer to store data in
	 * @param bufLen The number of bytes to write
	 */
	void writeData(size_t addr, const void *buf, size_t bufLen);

	/**
	 * @brief Erases a sector. Sectors are 4K (4096 bytes) and the smallest unit that can be erased.
	 *
	 * This call blocks for the duration of the erase, which take take some time (up to 500 milliseconds).
	 *
	 * @param addr Address of the beginning of the sector. Must be at the start of a sector boundary.
	 */
	void sectorErase(size_t addr);

	/**
	 * @brief Erases a block. Blocks are 64K (65536 bytes) or 16 sectors. There are 16 blocks on a 1 MByte device.
	 *
	 * This call blocks for the duration of the erase, which take take some time (up to 1 second).
	 * This call is not in the base API because the P1 does not support it. SPIFFS doesn't need it for operation
	 * and uses sector erase.
	 *
	 * @param addr Address of the beginning of the block
	 */
	void blockErase(size_t addr);

	/**
	 * @brief Erases the entire chip.
	 *
	 * This call blocks for the duration of the erase, which take take some time (several secoonds). This function
	 * uses delay(1) so the cloud connection will be serviced in non-system-threaded mode.
	 */
	void chipErase();

	/**
	 * @brief Sends the device reset sequence
	 *
	 * Winbond devices support this, ISSI devices do not.
	 */
	void resetDevice();

};