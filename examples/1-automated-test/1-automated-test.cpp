#include "CircularBufferSpiFlashRK.h"

#include "CircularBufferSpiFlashRK_AutomatedTest.h"

SerialLogHandler logHandler(LOG_LEVEL_TRACE);
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

// Pick a chip, port, and CS line
// SpiFlashISSI spiFlash(SPI, A2);
// SpiFlashWinbond spiFlash(SPI, A4);
SpiFlashMacronix spiFlash(SPI, A4);
// SpiFlashWinbond spiFlash(SPI1, D5);

const std::chrono::milliseconds testRunPeriod = 30s;
unsigned long testRunLast = 0;

void testRun();

void setup() {
	spiFlash.begin();

    // Particle.connect();
}

void loop() {
    if (testRunLast == 0 || millis() - testRunLast >= testRunPeriod.count()) {
        testRunLast = millis();

        testRun();
    }
}

void testRun() {
    Log.info("jedecId=%06lx", spiFlash.jedecIdRead());

	if (!spiFlash.isValid()) {
		Log.error("no valid flash chip");
		return;
	}

}
