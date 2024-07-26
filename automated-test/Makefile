

all : CircularBufferTest
	./CircularBufferTest

CircularBufferTest : CircularBufferTest.cpp SpiFlashTester.cpp ../src/CircularBufferSpiFlashRK.cpp ../src/CircularBufferSpiFlashRK.h ../src/CircularBufferSpiFlashRK_AutomatedTest.h  libwiringgcc
	gcc CircularBufferTest.cpp SpiFlashTester.cpp ../src/CircularBufferSpiFlashRK.cpp UnitTestLib/libwiringgcc.a -std=c++17 -lc++ -IUnitTestLib -I../src -I. -o CircularBufferTest -DUNITTEST

check : CircularBufferTest.cpp  ../src/CircularBufferSpiFlashRK.cpp ../src/CircularBufferSpiFlashRK.h libwiringgcc
	gcc CircularBufferTest.cpp ../src/CircularBufferSpiFlashRK.cpp UnitTestLib/libwiringgcc.a -g -O0 -std=c++11 -lc++ -IUnitTestLib -I ../src -o CircularBufferTest && valgrind --leak-check=yes ./CircularBufferTest 

libwiringgcc :
	cd UnitTestLib && make libwiringgcc.a 	
	
.PHONY: libwiringgcc