#!/bin/bash

type clang 2>/dev/null >/dev/null
if [[ $? != 0 ]]; then
    CC=gcc
else
    CC=clang
fi


$CC -c -I . -o compressor.o ../compressor.cpp
$CC -c -o compressorUnitTest.o compressorUnitTest.cpp
$CC -o compressorUnitTest compressor.o compressorUnitTest.o

./compressorUnitTest data.txt >data.out raw

echo '75/143 74/144 75/144' >>data.out
echo '75/143 74/144 74/144' >>data.out
echo '75/142 74/143 75/143' >>data.out
echo '75/141 75/141 75/141' >>data.out

diff data.txt data.out
if [[ $? != 0 ]]; then
   echo 'compressorUnitTest 1 failed'
   exit 1
fi


./compressorUnitTest data_corner.txt >data.out raw

echo '75/143 74/144 74/144' >>data.out
echo '75/142 74/143 75/143' >>data.out
echo '75/141 75/141 75/141' >>data.out

diff data_corner.txt data.out
if [[ $? != 0 ]]; then
    echo 'compressorUnitTest 2 failed'
    COMP_FAILED=1
else
    COMP_FAILED=0
fi


$CC -c -I . -o eeprom.o ../eeprom.cpp
$CC -c -o eepromUnitTest.o eepromUnitTest.cpp
$CC -o eepromUnitTest eeprom.o eepromUnitTest.o

./eepromUnitTest 

if [[ $? != 0 ]]; then
    echo 'compressorUnitTest 1 failed'
    EEPROM_FAILED=1
else
    EEPROM_FAILED=0
fi

rm -rf *.o compressorUnitTest data.out eepromUnitTest


if [[ COMP_FAILED == 1 || EEPROM_FAILED == 1 ]]; then
    exit 1
fi

exit 0
