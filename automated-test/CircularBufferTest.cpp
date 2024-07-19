#include <stdio.h>
#include "CircularBufferSpiFlashRK.h"
#include "SpiFlashTester.h"
#include "CircularBufferSpiFlashRK_AutomatedTest.h"

void runTest();

const size_t flashSize = 8 * 1024 * 1024; // 8 MB
uint8_t flashBuffer[flashSize];
SpiFlash spiFlash(flashBuffer, flashSize);

int main(int argc, char *argv[]) {
    spiFlash.begin();
    runTest();
    return 0;
}

#define assertDouble(exp, val, tol) \
    if (val < (exp - tol) || val > (exp + tol)) { printf("exp=%lf val=%lf\n", (double)exp, (double)val); assert(false); }

void runTest() {

    Log.info("jedecId=%06lx", (unsigned long) spiFlash.jedecIdRead());

	if (!spiFlash.isValid()) {
		Log.error("no valid flash chip");
		return;
	}

}




