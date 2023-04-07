#include <string.h>
#include "unitTest.h"

struct compRecord {
  uint8_t start[6];
  uint16_t filler;
  uint64_t data[15];
};

compRecord eeprom[32];
int eeprom_fake_cnt = 0;
uint8_t* eeprom_ptr = (uint8_t*) eeprom;

uint8_t eeprom_read8() {
  uint8_t ret = *eeprom_ptr;
  eeprom_ptr++;
  return ret;
}

uint16_t eeprom_read16() {
  uint16_t ret = *((uint16_t*)eeprom_ptr);
  eeprom_ptr += 2;
  return ret;
}

uint64_t eeprom_read64() {
  uint64_t ret = *((uint64_t*)eeprom_ptr);
  eeprom_ptr += 8;
  return ret;
}

#include "../compressor.h"

void eepromInitNext(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6) {
  //printf("Init: %d/%d %d/%d %d/%d\n", d1, d2, d3, d4, d5, d6);
  eeprom[eeprom_fake_cnt].start[0] = d1;
  eeprom[eeprom_fake_cnt].start[1] = d2;
  eeprom[eeprom_fake_cnt].start[2] = d3;
  eeprom[eeprom_fake_cnt].start[3] = d4;
  eeprom[eeprom_fake_cnt].start[4] = d5;
  eeprom[eeprom_fake_cnt].start[5] = d6;
}

void eepromWriteFiller(uint16_t filler) {
  //printf("Filler 0x%X\n", filler);
  eeprom[eeprom_fake_cnt].filler = filler;
}

void eepromWriteEntry(uint64_t entry, uint8_t pos) {
  //printf("Entry %d: 0x%lX\n", pos,  entry);
  eeprom[eeprom_fake_cnt].data[pos] = entry;
  if (pos == 14) eeprom_fake_cnt++;
}

// ===================== Init =====================
void setup() {

 
}

// ===================== Loop =====================
void loop() {

  compressor();

}

// ===================== =====================

int main(int argn, char** argv) {
  setup();
  
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 6; j++) eeprom[i].start[j] = 0xFF;
    eeprom[i].filler = 0xFFFF;
    for (int j = 0; j < 15; j++) eeprom[i].data[j] = 0xFFFFFFFFFFFFFFFF;
  }
  
  /*
   83 89 84 89 87 88
   184C
   FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF
   FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF
   FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF
   
  eeprom[1].start[0] = 0x83;
  eeprom[1].start[1] = 0x89;
  eeprom[1].start[2] = 0x84;
  eeprom[1].start[3] = 0x89;
  eeprom[1].start[4] = 0x87;
  eeprom[1].start[5] = 0x88;
  eeprom[1].filler = 0x184c;
  eeprom_ptr = ((uint8_t*) eeprom) + 128;
  decompressEntry();
   */
  
  if (argn != 3) {
    return 1;
  }
  FILE* f = fopen(argv[1], "r");
  if (f == NULL) {
    return 1;
  }
   
  int v1, v2, v3, v4, v5, v6;
  int u1, u2, u3, u4, u5, u6;
  int t1, t2, t3, t4, t5, t6;
    
  if (strcmp(argv[2], "raw") == 0) {
    decompDumpRaw = true;
  } else {
    decompDumpRaw = false;
  }
  
  fscanf(f, "%d/%d %d/%d %d/%d", &v1, &v2, &v3, &v4, &v5, &v6);
  putData((uint8_t) v1, (uint8_t) v2, (uint8_t) v3, (uint8_t) v4, (uint8_t) v5, (uint8_t) v6);
  compressor();
  compressor();
  //printf("%d/%d %d/%d %d/%d\n", v1, v2, v3, v4, v5, v6);
  fscanf(f, "%d/%d %d/%d %d/%d", &u1, &u2, &u3, &u4, &u5, &u6);
  putData((uint8_t) u1, (uint8_t) u2, (uint8_t) u3, (uint8_t) u4, (uint8_t) u5, (uint8_t) v6);
  compressor();
  compressor();
  //printf("%d/%d %d/%d %d/%d %d/%d %d/%d %d/%d\n", u1, u2, u3, u4, u5, u6, v1 - u1, v2 - u2, v3 - u3, v4 - u4, v5 - u5, v6 - u6);
  while(EOF != fscanf(f, "%d/%d %d/%d %d/%d", &t1, &t2, &t3, &t4, &t5, &t6)) {
    //printf("%d/%d %d/%d %d/%d | %d/%d %d/%d %d/%d | %d/%d %d/%d %d/%d\n", t1, t2, t3, t4, t5, t6, u1 - t1, u2 - t2, u3 - t3, u4 - t4, u5 - t5, u6 - t6,
    //      (v1 - u1) - (u1 - t1), (v2- u2) - (u2 - t2) , (v3 - u3) - (u3 - t3), (v4 - u4) - (u4 - t4), (v5 - u5) - (u5 - t5), (v6 - u6) - (u6 - t6));
    //v1 = u1; v2 = u2; v3 = u3; v4 = u4; v5 = u5; v6 = u6;
    //u1 = t1; u2 = t2; u3 = t3; u4 = t4; u5 = t5; u6 = t6;
    
    putData((uint8_t) t1, (uint8_t) t2, (uint8_t) t3, (uint8_t) t4, (uint8_t) t5, (uint8_t) t6);
    compressor();
    compressor();
    compressor();
    compressor();
    compressor();
  }
  fclose(f);
  for (int i = 0; i < 31; i++) {
    decompressEntry();
  }
}

// -------------------------------------------------------------------

