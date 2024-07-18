#include <stdio.h>
#include "CircularBufferSpiFlashRK.h"
#include "SpiFlashTester.h"

void runTest();

int main(int argc, char *argv[]) {
    runTest();
    return 0;
}

#define assertDouble(exp, val, tol) \
    if (val < (exp - tol) || val > (exp + tol)) { printf("exp=%lf val=%lf\n", (double)exp, (double)val); assert(false); }

void runTest() {


}




