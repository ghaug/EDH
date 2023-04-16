#ifdef ARDUINO
#include <Arduino.h>
#include <EEPROM.h>
#include <LibPrintf.h>
#else
#include "unitTests/unitTest.h"
#include "unitTests/eepromUnitTest.h"
#endif
#include "eeprom.h"
#include "edh.h"

void eepromInitAll() {
  for (uint16_t ptr = 0; ptr < EEPROM.length(); ) {
    EEPROM.update(ptr, 0xFF);
    EEPROM.update(ptr + 2, 0xFF);
    ptr += 6;
    if (ptr == 6) {
      EEPROM.put(ptr, (uint16_t) 0xFFFF);
    } else {
      EEPROM.put(ptr, (uint16_t) 0x7FFF);
    }
    ptr += 2;
    for (uint8_t i = 0; i < 15; i++) {
      EEPROM.put(ptr + 6, (uint16_t) 0xFFFF);
      ptr += 8;
    }
  }
}

bool eepromIsErrorRecord(uint16_t ptr) {
  return EEPROM.read(ptr) == 0xFE;
}

uint16_t eepromLastRcord() {
  return EEPROM.length() - sizeof(compressorRecord);
}

int16_t eepromFindErrorRecord() {
  for (int16_t ptr = 0; ptr < EEPROM.length(); ptr += sizeof(compressorRecord)) {
    if (eepromIsErrorRecord(ptr)) {
      return ptr;
    }
  }
  return -1;
}

int16_t eepromFindOrCreateErrorRecord() {
  int16_t ptr = eepromFindErrorRecord();
  if (ptr != -1) {
    return ptr;
  }
  ptr = eepromFindActiveAndHealthCheck(false);
  if (ptr == -1 || ptr == eepromLastRcord()) {
    ptr = 0;
  } else {
    ptr += sizeof(compressorRecord);
  }
  EEPROM.write(ptr, 0xFE);
  for (uint8_t i = 1; i < sizeof(compressorRecord); i += 5) {
    EEPROM.write(ptr + i, 0xFF);
  }
  return ptr;
}

void eepromAddError(uint8_t code, uint16_t hours, uint8_t minutes, uint8_t seconds) {
  int16_t ptr = eepromFindOrCreateErrorRecord();
  uint8_t i;
  for (i = 1; i < (sizeof(compressorRecord) - 5) && (EEPROM.read(ptr + i) & 0x80) != 0x80; i += 5) ;
  EEPROM.write(ptr + i++, code);
  EEPROM.put(ptr + i, hours);
  i += 2;
  EEPROM.update(ptr + i++, minutes);
  EEPROM.update(ptr + i++, seconds);
  if ((i + 5) >  sizeof(compressorRecord)) {
    i = 1;
  }
  EEPROM.update(ptr + i, EEPROM.read(ptr + i) | 0x80);
}

uint8_t eepromNumberOfErrors() {
  int16_t ptr = eepromFindErrorRecord();
  if (ptr == -1) {
    return 0;
  }
  uint8_t cnt = 0;
  for (uint8_t i = 1; i <= (sizeof(compressorRecord) - 5) && EEPROM.read(ptr + i) != 0xFF; i += 5) {
    cnt++;
  }
  return cnt;
}

void eepromGetError(uint8_t idx, uint8_t* code, uint16_t* hours, uint8_t* minutes, uint8_t* seconds) {
  *code = 0xFF;
  int16_t ptr = eepromFindErrorRecord();
  if (ptr == -1) return;
  uint8_t i;
  for (i = 1; (EEPROM.read(ptr + i) & 0x80) != 0x80; i += 5) ;
  if ((i + 10) < (sizeof(compressorRecord)) && EEPROM.read(ptr + i + 5) == 0xFF) {
    i = 1;
  }
  int16_t p = i + idx * 5;
  if (p > (sizeof(compressorRecord) - 5)) {
    p = p - (sizeof(compressorRecord)  / 5) * 5;
  }
  ptr += p;
  *code = EEPROM.read(ptr++) & 0x7F;
  EEPROM.get(ptr, *hours);
  ptr += 2;
  *minutes = EEPROM.read(ptr++);
  *seconds = EEPROM.read(ptr);
}

int16_t eepromFindActiveAndHealthCheck(bool doCure) {
  int16_t p[3];
  uint8_t c = 0;
  for (uint16_t ptr = 0; ptr < EEPROM.length() && c < 3; ptr += sizeof(compressorRecord)) {
    uint16_t filler;
    EEPROM.get(ptr + 6, filler);
    if (filler & 0x8000) {
      p[c++] = ptr;
    }
  }
  switch (c) {
    case 3:
      if (doCure) {
        eepromInitAll();
        return 0;
      }
      else {
        return -1;
      }
    case 2:
      if ((p[1] == p[0] + sizeof(compressorRecord)) ||
          ((p[1] == (p[0] + 2 * sizeof(compressorRecord)) && eepromIsErrorRecord(p[0] + sizeof(compressorRecord))))) {
        uint16_t filler;
        EEPROM.get(p[0] + 6, filler);
        EEPROM.put(p[0] + 6, filler & 0x0FFF);
        return p[1];
      } else if ((p[0] == 0 && p[1] == eepromLastRcord()) ||
                 (p[0] == 0 && p[1] == (eepromLastRcord() - 2 * sizeof(compressorRecord)) && eepromIsErrorRecord(eepromLastRcord())) ||
                 (p[0] == 1 && p[1] == (eepromLastRcord()) && eepromIsErrorRecord(sizeof(compressorRecord)))) {
        uint16_t filler;
        EEPROM.get(p[1] + 6, filler);
        EEPROM.put(p[1] + 6, filler & 0x0FFF);
        return p[0];
      } else {
        if (doCure) {
          eepromInitAll();
          return 0;
        } else {
          return -1;
        }
      }
    case 1:
      return p[0];
    case 0:
      if (doCure) {
        eepromInitAll();
        return 0;
      } else {
        return -1;
      }
  }
  return -1;
}

uint16_t eepromActivPtr;

void eepromAdvanceActive(uint8_t d1) {
  uint16_t current = eepromFindActiveAndHealthCheck(true);
  if (current == eepromLastRcord()) {
    if (eepromIsErrorRecord(0)) {
      eepromActivPtr = sizeof(compressorRecord);
    } else {
      eepromActivPtr = 0;
    }
  } else if (current == eepromLastRcord() - sizeof(compressorRecord) && eepromIsErrorRecord(eepromLastRcord())) {
    eepromActivPtr = 0;
  } else {
    if (eepromIsErrorRecord(current + sizeof(compressorRecord))) {
      eepromActivPtr = current + 2 * sizeof(compressorRecord);
    }
    else {
      eepromActivPtr = current + sizeof(compressorRecord);
    }
  }
#ifndef EEPROM_SIM_ONLY
  EEPROM.update(eepromActivPtr, d1);
  EEPROM.put(eepromActivPtr + 6, (uint16_t) 0XFFFF);
  uint16_t filler;
  EEPROM.get(current + 6, filler);
  filler &= 0x7FFF;
  EEPROM.put(current + 6, filler);
#endif
}

void eepromInitNext(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6) {
  eepromAdvanceActive(d1);
#ifdef EEPROM_SIM_ONLY
 printf("eepromInitNext %02X, %02X, %02X, %02X, %02X, %02X\n", d1, d2, d3, d4, d5, d6);
#else
  EEPROM.update(eepromActivPtr + 1, d2);
  EEPROM.update(eepromActivPtr + 2, d3);
  EEPROM.update(eepromActivPtr + 3, d4);
  EEPROM.update(eepromActivPtr + 4, d5);
  EEPROM.update(eepromActivPtr + 5, d6);
#endif
}


void eepromWriteFiller(uint16_t filler) {
#ifdef EEPROM_SIM_ONLY
  printf("eepromWriteFiller: %04X\n", filler);
#else
  EEPROM.put(eepromActivPtr + 6, filler);
#endif
}


void eepromWriteEntry(uint64_t entry, uint8_t pos) {
#ifdef EEPROM_SIM_ONLY
  printf("eepromWriteEntry %08llX at %d\n", entry, pos);
#else
  EEPROM.put(eepromActivPtr + 8 + pos * 8, entry);
  if (pos < 14) {
    EEPROM.put(eepromActivPtr + 22 + pos * 8, (uint16_t) 0xFFFF);
  }
#endif
}

uint16_t eepromCapacityInRecords() {
  uint16_t errorRec = 0;
  for (uint16_t i = 0; i < EEPROM.length() / sizeof(compressorRecord); i++) {
    if (eepromIsErrorRecord(i * sizeof(compressorRecord))) {
      errorRec++;
    }
  }
  return EEPROM.length() / sizeof(compressorRecord) - errorRec;
}

uint16_t eepromReadPtr;

void eepromInitReadPtr() {
  eepromReadPtr = eepromFindActiveAndHealthCheck(false);
  eepromReadPtr = eepromReadPtr < 0 ? 0 : eepromReadPtr;
}

uint16_t eepromGetReadPtr() {
  return eepromReadPtr;
}

void eepromSetReadPtr(uint16_t ptr) {
  eepromReadPtr = ptr;
}

void eepromAdvanceReadPtr() {
  if (eepromReadPtr == eepromLastRcord() - sizeof(compressorRecord)) {
    if (eepromIsErrorRecord(eepromLastRcord())) {
      eepromReadPtr = 0;
    } else {
      eepromReadPtr = eepromLastRcord();
    }
    return;
  }
  eepromReadPtr += sizeof(compressorRecord);
  if (eepromReadPtr > eepromLastRcord()) {
    eepromReadPtr = 0;
  }
  if (eepromIsErrorRecord(eepromReadPtr)) {
    eepromReadPtr += sizeof(compressorRecord);
  }
}

uint8_t eeprom_read8() {
  return EEPROM.read(eepromReadPtr++);
}

uint16_t eeprom_read16() {
  uint16_t d;
  EEPROM.get(eepromReadPtr, d);
  eepromReadPtr += 2;
  return d;
}

uint64_t eeprom_read64() {
  uint64_t d;
  EEPROM.get(eepromReadPtr, d);
  eepromReadPtr += 8;
  return d;
}

