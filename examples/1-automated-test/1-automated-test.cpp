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

const std::chrono::milliseconds testRunPeriod = 5min;
unsigned long testRunLast = 0;

void setup() {
    waitFor(Serial.isConnected, 10000); delay(2000);

	spiFlash.begin();

    // Particle.connect();
}

void loop() {
    if (testRunLast == 0 || millis() - testRunLast >= testRunPeriod.count()) {
        testRunLast = millis();

        runTestSuite(&spiFlash);
    }
}

