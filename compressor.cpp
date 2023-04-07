#ifdef ARDUINO
#include <Arduino.h>
#include <LibPrintf.h>
#else
#include "unitTests/unitTest.h"
#endif
#include "compressor.h"

void eepromInitNext(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6);
void eepromWriteFiller(uint16_t filler);
void eepromWriteEntry(uint64_t entry, uint8_t pos);
uint8_t eeprom_read8();
uint16_t eeprom_read16();
uint64_t eeprom_read64();

struct dataRecord {
  uint8_t inH;
  uint8_t inT;
  uint8_t outH;
  uint8_t outT;
  uint8_t auxH;
  uint8_t auxT;
};

bool decompDumpRaw = false;


#define DATA_BUF_MASK 0x0F

dataRecord dataBuffer[DATA_BUF_MASK + 1];
uint8_t dataInP = 0;
uint8_t dataOutP = 0;


#define COMP_BUF_MASK 0x3F


uint8_t compressorState = 0;
int16_t compressorBuffer[COMP_BUF_MASK + 1];
uint8_t compressorBufferInP = 0;
uint8_t compressorBufferOutP = 0;
dataRecord compressorLast;
int16_t lastDeltaInH;
int16_t lastDeltaInT;
int16_t lastDeltaOutH;
int16_t lastDeltaOutT;
int16_t lastDeltaAuxH;
int16_t lastDeltaAuxT;
#define compBufferPeek() (compressorBuffer[compressorBufferOutP])

// required Bits             2     3     4     5     6     7     8      9      10
uint16_t compMaxVal[]  = {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F,  0xFF,  0x1FF};
uint8_t compCapacity[] = {  31,   21,   15,   12,   10,    8,    7,     6,      5};
uint16_t compCode[]    = {0x02, 0x00, 0x0C, 0x0D, 0x0E, 0xF0, 0xF4, 0x3E0, 0x3F00};

uint8_t compressorAvailable() {
  return (compressorBufferInP - compressorBufferOutP) & COMP_BUF_MASK;
}

int16_t decompVLEcode[] = {0, 1, -1, 2, -2, 3};

#define DECOMP_BUF_MASK 0x1F

dataRecord decompRecord;
int16_t decompBuffer[DECOMP_BUF_MASK + 1];
uint8_t decompInP;
uint8_t decompOutP;
uint16_t decompOutput;
int16_t decompLastDeltaInH;
int16_t decompLastDeltaInT;
int16_t decompLastDeltaOutH;
int16_t decompLastDeltaOutT;
int16_t decompLastDeltaAuxH;
int16_t decompLastDeltaAuxT;

void putData(uint8_t h1, uint8_t t1, uint8_t h2, uint8_t t2, uint8_t h3, uint8_t t3){
  DBG_DB_PRINT("putData %d/%d %d/%d %d/%d\n", h1, t1, h2, t2, h3, t3);
  dataBuffer[dataInP].inH = h1;
  dataBuffer[dataInP].inT = t1;
  dataBuffer[dataInP].outH = h2;
  dataBuffer[dataInP].outT = t2;
  dataBuffer[dataInP].auxH = h3;
  dataBuffer[dataInP].auxT = t3;
  dataInP++;
  dataInP &= DATA_BUF_MASK;
}

bool getData(dataRecord* record) {
  if(dataInP == dataOutP) {
    return false;
  }
  record->inH = dataBuffer[dataOutP].inH;
  record->inT = dataBuffer[dataOutP].inT;
  record->outH = dataBuffer[dataOutP].outH;
  record->outT = dataBuffer[dataOutP].outT;
  record->auxH = dataBuffer[dataOutP].auxH;
  record->auxT = dataBuffer[dataOutP].auxT;
  dataOutP++;
  dataOutP &= DATA_BUF_MASK;
  DBG_DB_PRINT("getData %d/%d %d/%d %d/%d\n", record->inH, record->inT, record->outH, record->outT, record->auxH, record->auxT);
  return true;
}

dataRecord* getDataOutP() {
  return &(dataBuffer[dataOutP]);
}

void popDataTail() {
  dataOutP++;
  dataOutP &= DATA_BUF_MASK;
}

void returnData(dataRecord* record) {
  DBG_DB_PRINT("returnData %d/%d %d/%d %d/%d\n", record->inH, record->inT, record->outH, record->outT, record->auxH, record->auxT);
  dataOutP--;
  dataOutP &= DATA_BUF_MASK;
  dataBuffer[dataOutP].inH = record->inH;
  dataBuffer[dataOutP].inT = record->inT;
  dataBuffer[dataOutP].outH = record->outH;
  dataBuffer[dataOutP].outT = record->outT;
  dataBuffer[dataOutP].auxH = record->auxH;
  dataBuffer[dataOutP].auxT = record->auxT;
}

uint8_t availableData() {
  return (dataInP - dataOutP) & DATA_BUF_MASK;
}

void compBufferPut(int16_t d) {
  DBG_COMP_PRINT("compBufferPut %d\n", d);
  compressorBuffer[compressorBufferInP] = d;
  compressorBufferInP++;
  compressorBufferInP &= COMP_BUF_MASK;
}


int16_t compBufferGet() {
  int16_t ret = compressorBuffer[compressorBufferOutP];
  compressorBufferOutP++;
  compressorBufferOutP &= COMP_BUF_MASK;
  DBG_COMP_PRINT("compBufferGet %d\n", ret);
  return ret;
}

void compBufferPopTail() {
  DBG_COMP_PRINT("compBufferPopTail %d\n",compressorBuffer[compressorBufferOutP]);
  compressorBufferOutP++;
  compressorBufferOutP &= COMP_BUF_MASK;
}

bool compDoesFit(uint8_t n) {
  for (uint8_t i = 0; i < compCapacity[n]; i++) {
    int16_t val = compressorBuffer[(compressorBufferOutP + i) & COMP_BUF_MASK];
    int16_t max = compMaxVal[n];
    int16_t min = -compMaxVal[n] - 1;
    if (val > max || val < min) {
      return false;
    }
  }
  return true;
}

uint8_t compBitSize() {
  for (uint8_t i = 0; i < 9; ++i) {
    if (compDoesFit(i)) return i;
  }
  return 8;
}

uint8_t compVariableLenEnc(uint16_t* code, int16_t d) {
  uint8_t len;
  
  switch (d) {
   case 0:
     *code = 0;
     len = 1;
   break;
   case 1:
     *code = 0b10;
     len = 2;
   break;
   case -1:
     *code = 0b110;
     len = 3;
   break;
   case 2:
     *code = 0b1110;
     len = 4;
   break;
   case -2:
     *code = 0b11110;
     len = 5;
   break;
   case 3:
     *code = 0b111110;
     len = 6;
   break;
   default:
     *code = 0x7E00 | (d & 0x01FF);
     len = 15;
  }
  return len;
}

uint8_t compDoVarLenEnc(uint8_t bits_available, uint16_t* filler) {
  uint8_t i;
  for (i = 0; i <= 5 && bits_available > 0; ++i) {
    uint16_t code;
    uint8_t len = compVariableLenEnc(&code, compBufferPeek());
    if (bits_available >= len) {
      compBufferPopTail();
      *filler <<= len;
      *filler |= code;
      bits_available -= len;
    } else {
      *filler <<= bits_available;
      *filler |= ~(0xFFFF << bits_available);
      bits_available = 0;
    }
  }
  return bits_available;
}

void compressor() {
  switch (compressorState) {
    case 0:
      if (availableData() > 0) {
        getData(&compressorLast);
        eepromInitNext(compressorLast.inH, compressorLast.inT, compressorLast.outH, compressorLast.outT, compressorLast.auxH, compressorLast.auxT);
        compressorState = 1;
        compressorBufferInP = 0;
        compressorBufferOutP = 0;
      }
    break;
    case 1:
      if (availableData() >= 3) {
        uint8_t bits_available = 15;
        uint16_t filler;        
        dataRecord* r = getDataOutP();
        lastDeltaInH  = compressorLast.inH - r->inH;
        lastDeltaInT  = compressorLast.inT - r->inT;
        lastDeltaOutH = compressorLast.outH - r->outH;
        lastDeltaOutT = compressorLast.outT - r->outT;
        lastDeltaAuxH = compressorLast.auxH - r->auxH;
        lastDeltaAuxT = compressorLast.auxT - r->auxT;
        compBufferPut(lastDeltaInH);
        compBufferPut(lastDeltaInT);
        compBufferPut(lastDeltaOutH);
        compBufferPut(lastDeltaOutT);
        compBufferPut(lastDeltaAuxH);
        compBufferPut(lastDeltaAuxT);
        getData(&compressorLast);
        bits_available = compDoVarLenEnc(bits_available, &filler);

        while (bits_available > 0) {
          r = getDataOutP();
          int16_t deltaInH = compressorLast.inH - r->inH;
          int16_t deltaInT = compressorLast.inT - r->inT; 
          int16_t deltaOutH = compressorLast.outH - r->outH;
          int16_t deltaOutT = compressorLast.outT - r->outT; 
          int16_t deltaAuxH = compressorLast.auxH - r->auxH;
          int16_t deltaAuxT = compressorLast.auxT - r->auxT;
          compBufferPut(lastDeltaInH - deltaInH);
          compBufferPut(lastDeltaInT - deltaInT);
          compBufferPut(lastDeltaOutH - deltaOutH);
          compBufferPut(lastDeltaOutT - deltaOutT);
          compBufferPut(lastDeltaAuxH - deltaAuxH);
          compBufferPut(lastDeltaAuxT - deltaAuxT);
          lastDeltaInH = deltaInH;
          lastDeltaInT = deltaInT;
          lastDeltaOutH = deltaOutH;
          lastDeltaOutT = deltaOutT;
          lastDeltaAuxH = deltaAuxH;
          lastDeltaAuxT = deltaAuxT;
          getData(&compressorLast);
          bits_available = compDoVarLenEnc(bits_available, &filler);
        }
        filler |= 0x8000;
        eepromWriteFiller(filler);
        compressorState = 2;
      }
    break;
    default:
      if ((availableData() * 6 + compressorAvailable()) >= 31) {
        uint8_t tempLastInH = compressorLast.inH;
        uint8_t tempLastInT = compressorLast.inT;
        uint8_t tempLastOutH = compressorLast.outH;
        uint8_t tempLastOutT = compressorLast.outT;
        uint8_t tempLastAuxH = compressorLast.auxH;
        uint8_t tempLastAuxT = compressorLast.auxT;
        int16_t tempLastDeltaInH = lastDeltaInH;
        int16_t tempLastDeltaInT = lastDeltaInT;
        int16_t tempLastDeltaOutH = lastDeltaOutH;
        int16_t tempLastDeltaOutT = lastDeltaOutT;
        int16_t tempLastDeltaAuxH = lastDeltaAuxH;
        int16_t tempLastDeltaAuxT = lastDeltaAuxT;
        uint8_t tempCompressorBufferInP = compressorBufferInP;

        uint16_t i = 0;
        while (((tempCompressorBufferInP - compressorBufferOutP) & COMP_BUF_MASK) < 31) {
          dataRecord* r = &(dataBuffer[((dataOutP + i++) & DATA_BUF_MASK)]);
          int16_t deltaInH = tempLastInH - r->inH;
          int16_t deltaInT = tempLastInT - r->inT;
          int16_t deltaOutH = tempLastOutH - r->outH;
          int16_t deltaOutT = tempLastOutT - r->outT;
          int16_t deltaAuxH = tempLastAuxH - r->auxH;
          int16_t deltaAuxT = tempLastAuxT - r->auxT;
          int16_t deltaDelta;
          deltaDelta = tempLastDeltaInH - deltaInH;
          compressorBuffer[tempCompressorBufferInP++] = deltaDelta;
          tempCompressorBufferInP &= COMP_BUF_MASK;
          deltaDelta = tempLastDeltaInT - deltaInT;
          compressorBuffer[tempCompressorBufferInP++] = deltaDelta;
          tempCompressorBufferInP &= COMP_BUF_MASK;
          deltaDelta = tempLastDeltaOutH - deltaOutH;
          compressorBuffer[tempCompressorBufferInP++] = deltaDelta;
          tempCompressorBufferInP &= COMP_BUF_MASK;
          deltaDelta = tempLastDeltaOutT - deltaOutT;
          compressorBuffer[tempCompressorBufferInP++] = deltaDelta;
          tempCompressorBufferInP &= COMP_BUF_MASK;
          deltaDelta = tempLastDeltaAuxH - deltaAuxH;          
          compressorBuffer[tempCompressorBufferInP++] = deltaDelta;
          tempCompressorBufferInP &= COMP_BUF_MASK;
          deltaDelta = tempLastDeltaAuxT - deltaAuxT;
          compressorBuffer[tempCompressorBufferInP++] = deltaDelta;
          tempCompressorBufferInP &= COMP_BUF_MASK;
          tempLastDeltaInH = deltaInH;
          tempLastDeltaInT = deltaInT;
          tempLastDeltaOutH = deltaOutH;
          tempLastDeltaOutT = deltaOutT;
          tempLastDeltaAuxH = deltaAuxH;
          tempLastDeltaAuxT = deltaAuxT;
          tempLastInH = r->inH;
          tempLastInT = r->inT;
          tempLastOutH = r->outH;
          tempLastOutT = r->outT;
          tempLastAuxH = r->auxH;
          tempLastAuxT = r->auxT;
        }
        uint8_t bitSize = compBitSize();
        while (compressorAvailable() < compCapacity[bitSize]) {
          dataRecord* r = getDataOutP();
          int16_t deltaInH = compressorLast.inH - r->inH;
          int16_t deltaInT = compressorLast.inT - r->inT;
          int16_t deltaOutH = compressorLast.outH - r->outH;
          int16_t deltaOutT = compressorLast.outT - r->outT;
          int16_t deltaAuxH = compressorLast.auxH - r->auxH;
          int16_t deltaAuxT = compressorLast.auxT - r->auxT;
          compBufferPut(lastDeltaInH - deltaInH);
          compBufferPut(lastDeltaInT - deltaInT);
          compBufferPut(lastDeltaOutH - deltaOutH);
          compBufferPut(lastDeltaOutT - deltaOutT);
          compBufferPut(lastDeltaAuxH - deltaAuxH);
          compBufferPut(lastDeltaAuxT - deltaAuxT);
          lastDeltaInH = deltaInH;
          lastDeltaInT = deltaInT;
          lastDeltaOutH = deltaOutH;
          lastDeltaOutT = deltaOutT;
          lastDeltaAuxH = deltaAuxH;
          lastDeltaAuxT = deltaAuxT;
          getData(&compressorLast);
        }
        uint64_t bitSet = compCode[bitSize];
        for (uint8_t i = 0; i < compCapacity[bitSize]; i++) {
          bitSet <<= (bitSize + 2);
          uint16_t d = (uint16_t) compBufferGet();
          bitSet |= (d & (uint16_t) ~(0xFFFF << (bitSize + 2)));
        }
        eepromWriteEntry(bitSet, compressorState - 2);
        if (compressorState == 16) {
          compressorState = 0;
          if ((compressorBufferInP - compressorBufferOutP) != 0) {
            returnData(&compressorLast);
          }
        } else {
          compressorState++;
        }
      }
  }
}

int16_t compVLEcode[] = {0, 1, -1, 2, -2, 3};

void decompRecordOut() {
  if (decompDumpRaw) {
    printf("%d/%d %d/%d %d/%d\n", decompRecord.inH, decompRecord.inT, decompRecord.outH, decompRecord.outT, decompRecord.auxH, decompRecord.auxT);
  } else {
    printf("In Temp/Humi: %.2f/%.2f\tOut Temp/Humi: %.2f/%.2f\tAux Temp: %.2f Error: 0x%02X\n", (decompRecord.inT / 3.0f) -25.0f, decompRecord.inH / 2.5f, 
    (decompRecord.outT / 3.0f) -25.0f, decompRecord.outH / 2.5f, (decompRecord.auxT / 3.0f) -25.0f, decompRecord.auxH);
  }
  decompOutput++;
}

void decompBufferAdd(int16_t data) {
  DBG_DECOMP_PRINT("decompBufferAdd: %d\n", data);
  decompBuffer[decompInP] = data;
  decompInP = (decompInP + 1) & DECOMP_BUF_MASK;
  if (((decompInP - decompOutP) & DECOMP_BUF_MASK) >= 6) {
    if (decompOutput < 2) {
      decompLastDeltaInH = decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaInT = decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaOutH = decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaOutT = decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaAuxH = decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaAuxT = decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
    } else {
      decompLastDeltaInH = decompLastDeltaInH - decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaInT = decompLastDeltaInT - decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaOutH = decompLastDeltaOutH - decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaOutT = decompLastDeltaOutT - decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaAuxH = decompLastDeltaAuxH - decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);
      decompLastDeltaAuxT = decompLastDeltaAuxT - decompBuffer[decompOutP];
      decompOutP = ((decompOutP + 1) & DECOMP_BUF_MASK);

    }
    decompRecord.inH -= decompLastDeltaInH;
    decompRecord.inT -= decompLastDeltaInT;
    decompRecord.outH -= decompLastDeltaOutH;
    decompRecord.outT -= decompLastDeltaOutT;
    decompRecord.auxH -= decompLastDeltaAuxH;
    decompRecord.auxT -= decompLastDeltaAuxT;
    decompRecordOut();
  }
}

void decompEntry(uint64_t entry, uint8_t bSize, uint8_t n) {
  int64_t se = (uint64_t) entry;
  for (uint8_t i = 0; i < n; i++) {
    decompBufferAdd(((se >> (64 - bSize)) & 0xFFFF) | ((se >> 63) & 0x01 ? 0xFFFF << bSize : 0));
    se <<= bSize;
  }
}

void decompressEntry() {
  decompInP = 0;
  decompOutP = 0;
  decompOutput = 0;
 
  decompRecord.inH = eeprom_read8();
  decompRecord.inT = eeprom_read8();
  decompRecord.outH = eeprom_read8();
  decompRecord.outT = eeprom_read8();
  decompRecord.auxH = eeprom_read8();
  decompRecord.auxT = eeprom_read8();

  if (decompRecord.inH == 0xFF && decompRecord.outH == 0xFF) return;
  decompRecordOut();
  uint16_t filler = eeprom_read16();
  if ((filler | 0x8000) == 0xFFFF) return;
  filler <<= 1;
  filler |= 1;
  uint8_t i = 0;
  do {
    uint8_t j;
    for (j = 0; j < 6 && (filler & 0x8000) != 0; j++) {
      filler <<= 1;
      filler |= 1;
    }
    if (j == 6) {
      uint16_t d = 0x1FF & (filler >> 7);
      decompBufferAdd((int16_t)((d & (1 << 8)) ? 0xFE00 : 0x0000) | d);
      filler = 0xFFFF;
    } else {
      i += (j + 1);
      filler <<= 1;
      filler |= 1;
     decompBufferAdd(compVLEcode[j]);
    }
  } while (i < 15 && filler != 0xFFFF);
  for (i = 0; i < 15; i++) {
    uint64_t entry = eeprom_read64();
    if ((entry >> 48 & 0xFFFF) == 0xFFFF) {
      return;
    }
    if (((entry >> 63) & 1) == 0) {
      entry <<= 1;
      decompEntry(entry, 3, 21);
    } else if (((entry >> 62) & 0x03) == 0x02) {
      entry <<= 2;
      decompEntry(entry, 2, 31);
    } else if (((entry >> 60) & 0x0F) != 0x0F) {
      uint8_t sel = (entry >> 60) & 0x03;
      entry <<= 4;
      switch (sel) {
        case 0:
          decompEntry(entry, 4, 15);
          break;
        case 1:
          decompEntry(entry, 5, 12);
          break;
        default:
          decompEntry(entry, 6, 10);
          break;
      }
    } else {
      uint8_t sel = (entry >> 58) & 0x03;
      switch (sel) {
        case 0:
          entry <<= 8;
          decompEntry(entry, 7, 8);
          break;
        case 1:
          entry <<= 8;
          decompEntry(entry, 8, 7);
          break;
       case 2:
          entry <<= 10;
          decompEntry(entry, 9, 6);
          break;
       default:
          entry <<= 14;
          decompEntry(entry, 10, 5);
          break;
      }
    }
  }
}
