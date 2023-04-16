#ifndef EDH_EEPROM_H
#define EDH_EEPROM_H

#undef EEPROM_SIM_ONLY

void eepromInitAll();
int16_t eepromFindActiveAndHealthCheck(bool doCure);
void eepromInitNext(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6);
void eepromWriteFiller(uint16_t filler);
void eepromWriteEntry(uint64_t entry, uint8_t pos);
void eepromInitReadPtr();
uint16_t eepromGetReadPtr();
void eepromSetReadPtr(uint16_t ptr);
void eepromAdvanceReadPtr();
uint16_t eepromCapacityInRecords();
uint8_t eeprom_read8();
uint16_t eeprom_read16();
uint64_t eeprom_read64();
void eepromAddError(uint8_t code, uint16_t hours, uint8_t minutes, uint8_t seconds);
uint8_t eepromNumberOfErrors();
void eepromGetError(uint8_t idx, uint8_t* code, uint16_t* hours, uint8_t* minutes, uint8_t* seconds);

#endif // EDH_EEPROM_H
