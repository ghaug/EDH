#include <stdio.h>
#include "unitTest.h"
#include "eepromUnitTest.h"

uint8_t _eeprom[4096];

EEPROMclass EEPROM;

#include "../eeprom.h"

// ===================== =====================

int main(int argn, char** argv) {
  for (int i = 0; i < 4096; ++i) _eeprom[i] = 0;
  
  int16_t p = eepromFindActiveAndHealthCheck(false);
  if (p > 0) {
    printf("EEPROM should be invalid (0)\n");
    return 1;
  }
  p = eepromFindActiveAndHealthCheck(true);
  if (p != 0) {
    printf("EEPROM should be fixed (1)\n");
    return 1;
  }
  
  *((uint16_t*) &_eeprom[3*128+6]) = 0x8000;
  
  p = eepromFindActiveAndHealthCheck(false);
  if (p > 0) {
    printf("EEPROM should be invalid (2)\n");
    return 1;
  }
  *((uint16_t*) &_eeprom[3*128+6]) = 0x0000;
  *((uint16_t*) &_eeprom[1*128+6]) = 0x8000;
  *((uint16_t*) &_eeprom[2*128+6]) = 0x8000;

  p = eepromFindActiveAndHealthCheck(false);
  if (p > 0) {
    printf("EEPROM should be invalid (3)\n");
    return 1;
  }

  *((uint16_t*) &_eeprom[1*128+6]) = 0x0000;
  p = eepromFindActiveAndHealthCheck(false);
  if (p > 0) {
    printf("EEPROM should be invalid (4)\n");
    return 1;
  }
  _eeprom[128] = 0xFE;
  p = eepromFindActiveAndHealthCheck(false);
  if (p != 256) {
    printf("EEPROM should be valid (5)\n");
    return 1;
  }
  
  _eeprom[128] = 0xFF;
  *((uint16_t*) &_eeprom[0*128+6]) = 0x7FFF;
  *((uint16_t*) &_eeprom[1*128+6]) = 0x7FFF;
  *((uint16_t*) &_eeprom[2*128+6]) = 0x7FFF;
  *((uint16_t*) &_eeprom[31*128+6]) = 0xFFFF;
  p = eepromFindActiveAndHealthCheck(false);
  if (p != (31*128)) {
    printf("EEPROM should be valid (6)\n");
    return 1;
  }
  _eeprom[0] = 0xFE;
  
  eepromInitNext(0, 1, 2, 3, 4, 5);
  
  if (eepromFindActiveAndHealthCheck(false) != 128 || _eeprom[129] != 1) {
    printf("Advance didn't work (7)\n");
    return 1;
  }
  eepromInitNext(0, 77, 2, 3, 4, 5);
  
  if (eepromFindActiveAndHealthCheck(false) != 256 || _eeprom[257] != 77) {
    printf("Advance didn't work (8)\n");
    return 1;
  }

  for (uint8_t i = 0; i < 31; i++) {
    eepromInitNext(0, i, 2, 3, 4, 5);
  }
  if (eepromFindActiveAndHealthCheck(false) != 256 || _eeprom[257] != 30) {
    printf("Advance didn't work (9)\n");
    return 1;
  }

  _eeprom[0] = 0xFF;

  for (uint8_t i = 0; i < 32; i++) {
    eepromInitNext(0, i, 2, 3, 4, 5);
  }
  if (eepromFindActiveAndHealthCheck(false) != 256 || _eeprom[257] != 31) {
    printf("Advance didn't work (9)\n");
    return 1;
  }

  eepromInitReadPtr();
  
  eeprom_read8();
  
  if (eeprom_read8() != 31) {
    printf("Read didn't work (10)\n");
    return 1;
  }
  for (int16_t i = 0; i < 31; i++) {
    eepromAdvanceReadPtr();
  }
  eeprom_read8();
  if (eeprom_read8() != 31) {
    printf("Read didn't work (11)\n");
    return 1;
  }
  if (eepromCapacityInRecords() != 32) {
    printf("Wrong capacity (12)\n");
    return 1;
  }
  _eeprom[0] = 0xFE;
  if (eepromCapacityInRecords() != 31) {
    printf("Wrong capacity (13)\n");
    return 1;
  }
  for (int16_t i = 0; i < 30; i++) {
    eepromAdvanceReadPtr();
  }
  eeprom_read8();
  if (eeprom_read8() != 31) {
    printf("Read didn't work (11)\n");
    return 1;
  }


  _eeprom[0] = 0x00;

  if (eepromNumberOfErrors() != 0) {
    printf("There should be no error records!\n");
    return 1;
  }

  eepromAddError(1, 2, 3, 4);

  if (eepromNumberOfErrors() != 1) {
    printf("There should be one error not %d!\n", eepromNumberOfErrors());
    return 1;
  }

  uint8_t c, m, s;
  uint16_t h;

  eepromGetError(0, &c, &h, &m, &s);

  if (c != 1 || h != 2 || m != 3 || s != 4) {
    printf("Error not as expected!\n");
    return 1;
  }

  eepromAddError(5, 40000, 55, 55);

  eepromGetError(0, &c, &h, &m, &s);

  if (c != 1 || h != 2 || m != 3 || s != 4) {
    printf("Error not as expected!\n");
    return 1;
  }

  eepromGetError(1, &c, &h, &m, &s);

  if (c != 5 || h != 40000 || m != 55 || s != 55) {
    printf("Error not as expected!\n");
    return 1;
  }

  for (int i = 0; i < 26; i++) {
    eepromAddError(i, 1, 8, 5);
  }

  if (eepromNumberOfErrors() != 25) {
    printf("There should be 25 errors not %d!\n", eepromNumberOfErrors());
    return 1;
  }


  eepromGetError(0, &c, &h, &m, &s);

  if (c != 1) {
    printf("Error not 1 as expected!\n");
    return 1;
  }

  eepromGetError(1, &c, &h, &m, &s);

  if (c != 2) {
    printf("Error not 2 as expected!\n");
    return 1;
  }


  eepromGetError(20, &c, &h, &m, &s);
  if (c != 21) {
    printf("Error not 21 as expected!\n");
    return 1;
  }


  eepromGetError(21, &c, &h, &m, &s);
  if (c != 22) {
    printf("Error not 22 as expected!\n");
    return 1;
  }

  eepromGetError(22, &c, &h, &m, &s);

  if (c != 23) {
    printf("Error %d not 23 as expected!\n", c);
    return 1;
  }

  eepromGetError(24, &c, &h, &m, &s);

  if (c != 25) {
    printf("Error %d not 25 as expected!\n", c);
    return 1;
  }

  _eeprom[0x180] = 0x00;

  for (int i = 0; i < 49; i++) {
    eepromAddError(i, 1, 8, 5);
  }


  eepromGetError(0, &c, &h, &m, &s);

  if (c != 24) {
    printf("Error %d not 24 as expected!\n", c);
    return 1;
  }

  eepromGetError(1, &c, &h, &m, &s);

  if (c != 25) {
    printf("Error %d not 25 as expected!\n", c);
    return 1;
  }


  _eeprom[0x180] = 0x00;

  for (int i = 0; i < 50; i++) {
    eepromAddError(i, 1, 8, 5);
  }


  eepromGetError(0, &c, &h, &m, &s);

  if (c != 25) {
    printf("Error %d not 25 as expected!\n", c);
    return 1;
  }

  eepromGetError(1, &c, &h, &m, &s);

  if (c != 26) {
    printf("Error %d not 26 as expected!\n", c);
    return 1;
  }

  eepromGetError(24, &c, &h, &m, &s);

  if (c != 49) {
    printf("Error %d not 49 as expected!\n", c);
    return 1;
  }

  return 0;
}
