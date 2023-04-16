#include <LibPrintf.h>
#include <Wire.h>
#include <SoftWire.h>
#include <SHT31.h>
#include <SHT31_SW.h>
#include <SimpleKalmanFilter.h>
#include <EEPROM.h>
#include <PID_v1.h>
#include <OneWire.h>

#include "edh.h"
#include "edhSM.h"
#include "eeprom.h"
#include "compressor.h"

uint16_t loopCnt = LOOP0BIT;
uint8_t seconds = 0;
uint8_t minutes = 0;
uint16_t hours = 0;
uint16_t loopMaxDurMillis = 0;

uint32_t _error = 0;

SoftWire sw1(A11, A10);
uint8_t rxBuf1[32];
uint8_t txBuf1[32];

SHT31_SW shtIn;
SHT31    shtOut;
uint16_t shtInStat = 0;
uint16_t shtOutStat = 0;
int shtError = 0;

OneWire  dsE(ENGINE_TEMP); 
uint8_t dataE[9];
uint8_t addrE[8];

OneWire  dsC(COOLER_TEMP); 
uint8_t dataC[9];
uint8_t addrC[8];


uint32_t shtMaxReadMicros = 0;

bool contDump = false;
char lastInByte = 0;
char inByte;

uint16_t heatCycle = 0;

uint16_t avgCnt = 0;
float inHsum = 0.0f;
float inTsum = 0.0f;
float outHsum = 0.0f;
float outTsum = 0.0f;
float coolTsum = 0.0f;
float engTsum = 0.0f;

float inHavg;
float inTavg;
float outHavg;
float outTavg;
float coolTavg;
float engTavg;

double coolSetpoint, coolInput, coolOutput;

//Define the aggressive and conservative Tuning Parameters
double aggKp=16, aggKi=0.8, aggKd=8;
double consKp=4, consKi=0.2, consKd=2;

PID pid(&coolInput, &coolOutput, &coolSetpoint, consKp, consKi, consKd, REVERSE);

edhSM sm(&getInT, &getInH, &getOutT, &getOutH, &getCoolT, &getEngT, &cooler, &pump, coolSetP);

/*
 SimpleKalmanFilter(e_mea, e_est, q);
 e_mea: Measurement Uncertainty 
 e_est: Estimation Uncertainty 
 q: Process Noise
 */
SimpleKalmanFilter hInKalmanFilter(0.1, 0.1, 0.01);
SimpleKalmanFilter tInKalmanFilter(0.1, 0.1, 0.01);
SimpleKalmanFilter hOutKalmanFilter(0.1, 0.1, 0.01);
SimpleKalmanFilter tOutKalmanFilter(0.1, 0.1, 0.01);
SimpleKalmanFilter tCoolKalmanFilter(0.1, 0.1, 0.01);
SimpleKalmanFilter tEngKalmanFilter(0.1, 0.1, 0.01);

float inH;
float inT;
float outH;
float outT;
float coolT;
float engT;
float coolerTemp;

uint8_t ledStat = LOW;
uint8_t blueLED = LED_ON;
uint8_t yellowLED = LED_BLINK;

uint8_t masterMode = 0;

bool decompDumpRaw = false;

// ===================== Init =====================
void setup() {
  printf("Setting up...\n");

  // Pins etc.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledStat);
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, HIGH);
  pinMode(COOLER_MAINS, OUTPUT);
  digitalWrite(COOLER_MAINS, HIGH);
  pinMode(COOLER, OUTPUT);
  analogWrite(COOLER, 230);
  Serial.begin(115200);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, HIGH);  
  pinMode(LED_YELLOW, OUTPUT);
  digitalWrite(LED_YELLOW, HIGH);  

  // Configure soft and hard I2C
  sw1.setRxBuffer(&rxBuf1, 32);
  sw1.setTxBuffer(&txBuf1, 32);
  sw1.setTimeout_ms(200);
  Wire.setClock(100000);

  // I2C sensors
  bool bIn = false;
  bool bOut = false;
  for (uint8_t i = 0 ; i < 3 && !bIn; i++ ) bIn = shtIn.begin(&sw1);
  for (uint8_t i = 0 ; i < 3 && !bOut; i++ ) bOut = shtOut.begin(&Wire);
  printf("I2C Sensor Init     In: %s Out: %s\n", bIn ? "GOOD  " : "BAD   ", bOut ? "GOOD  " : "BAD   ");
  printf("I2C Sensor Status   In: 0x%X Out: 0x%X\n", shtIn.readStatus(), shtOut.readStatus());
  
  if (!bIn) { 
    error(ERR_SHT_IN);
  } else {
    shtInStat = readSHT(&shtIn);
    if (shtInStat == SHT_ERROR) {
      error(ERR_SHT_IN);
    }
  }    
  if (!bOut) {
    error(ERR_SHT_OUT);
  } else {
    shtOutStat = readSHT(&shtOut);
    if (shtOutStat == SHT_ERROR) {
      error(ERR_SHT_OUT);
    }
  }    
  
  // One wire sensors
  bool bEng = dsE.search(addrE);

  if (bEng && OneWire::crc8(addrE, 7) == addrE[7] && addrE[0] == 0x28) {
    dsE.reset();
    dsE.select(addrE);
    dsE.write(0x44, 1);        // start conversion, with parasite power on at the end
    printf("Engine Sensor Status GOOD\n");
  } else {
     printf("Engine Sensor Status BAD\n");
     error(ERR_SEN_ENG);
  }

  bool bCool = dsC.search(addrC);

  if (bCool && OneWire::crc8(addrC, 7) == addrC[7] && addrC[0] == 0x28) {
    dsC.reset();
    dsC.select(addrC);
    dsC.write(0x44, 1);        // start conversion, with parasite power on at the end
    printf("Cooler Sensor Status GOOD\n");
  } else {
     printf("Cooler Sensor Status BAD\n");
     error(ERR_SEN_COOL);
  }
 
  if (hasError(ERR_ANY)) {
    pump(false);
    cooler(false);
  }

  delay(1000);
  float eT = readTempSen(&dsE, addrE, dataE);
  coolerTemp = readTempSen(&dsC, addrC, dataC);

  // Prime the kalman filters (this is BS)
  for (uint16_t i = 0; i < 120; i++) {
    inH = hInKalmanFilter.updateEstimate(shtIn.getHumidity());
    inT = tInKalmanFilter.updateEstimate(shtIn.getTemperature());
    outH = hInKalmanFilter.updateEstimate(shtOut.getHumidity());
    outT = tInKalmanFilter.updateEstimate(shtOut.getTemperature());
    coolT = tCoolKalmanFilter.updateEstimate(coolerTemp);
    engT = tEngKalmanFilter.updateEstimate(eT);
  }

  // Use current values as averages
  inHavg = shtIn.getHumidity();
  inTavg = shtIn.getTemperature();
  outHavg = shtOut.getHumidity();
  outTavg = shtOut.getTemperature();
  coolTavg = coolerTemp;
  engTavg = eT;

  // Take care of data store
  printf("Data store is %shealthy.\n", eepromFindActiveAndHealthCheck(false) == -1 ? "not " : "");

  // Configure and turn the PID on
  coolInput = coolerTemp;
  coolSetpoint = coolSetP;
  coolOutput = 255.0;
  pid.SetMode(AUTOMATIC);
  pid.SetSampleTime(1000);

  delay(1000);

  printf("Init complete\nEnter h for help\n>");
}


// ===================== Loop =====================
// 
// The loop runs every 100ms. Most tasks run only 
// every tenth iteration, i.e. every second. As reading
// sensors is slow, this is distributed over iterations.
// A few important tasks run with each iteration.
void loop() {
  uint32_t loopStartMillis = millis();

  // In case of an error we switch off pump and cooler
  if (hasError(ERR_ANY)) {
    pump(false);
    cooler(false);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_BLUE, LOW);
    return;
  }

  // 10n+0: blink LEDs and read IN sensor
  if (loop0) {
    if (blueLED == LED_ON || blueLED == LED_BLINK) {
      digitalWrite(LED_BLUE, HIGH);
    } else {
      digitalWrite(LED_BLUE, LOW);
    }
    if (yellowLED == LED_ON || yellowLED == LED_BLINK) {
      digitalWrite(LED_YELLOW, HIGH);
    } else {
      digitalWrite(LED_YELLOW, LOW);
    }    
  }
  if (loop0 && !hasError(ERR_SHT_IN)) {
    shtInStat = readSHT(&shtIn);
    if (shtInStat == SHT_ERROR) {
      error(ERR_SHT_IN);
    }
    inH = hInKalmanFilter.updateEstimate(shtIn.getHumidity());
    inT = tInKalmanFilter.updateEstimate(shtIn.getTemperature());
  }

  // 10n+1: read OUT sensor
  if (loop1 && !hasError(ERR_SHT_OUT)) {
    shtOutStat = readSHT(&shtOut);
    if (shtOutStat == SHT_ERROR) {
      error(ERR_SHT_OUT);
    }
    outH = hOutKalmanFilter.updateEstimate(shtOut.getHumidity());
    outT = tOutKalmanFilter.updateEstimate(shtOut.getTemperature());
  }

  // 10n+2: read cooler sensor
  if (loop2 && !hasError(ERR_SEN_COOL)) {
    coolerTemp = readTempSen(&dsC, addrC, dataC);
    coolT = tCoolKalmanFilter.updateEstimate(coolerTemp);
  }
  
  // 10n+3: read engine sensor
  if (loop3 && !hasError(ERR_SEN_ENG)) {
    float eT = readTempSen(&dsE, addrE, dataE);
    engT = tEngKalmanFilter.updateEstimate(eT);
  }

  // 10n+4: run cooler PID control
  if (loop4) {
    coolInput = coolerTemp;

    double gap = abs(coolSetpoint-coolInput); //distance away from setpoint
    if (gap < 2) {  //we're close to setpoint, use conservative tuning parameters
      pid.SetTunings(consKp, consKi, consKd);
    } else {
    //we're far from setpoint, use aggressive tuning parameters
    pid.SetTunings(aggKp, aggKi, aggKd);
    }

    pid.Compute();
    uint8_t cs = round(coolOutput);
    analogWrite(COOLER, cs);
  }

  // 10n+5: blink LEDs and run transition function of main state machine
  if (loop5) {
    if (blueLED == LED_ON) {
      digitalWrite(LED_BLUE, HIGH);
    } else {
      digitalWrite(LED_BLUE, LOW);
    }
    if (yellowLED == LED_ON) {
      digitalWrite(LED_YELLOW, HIGH);
    } else {
      digitalWrite(LED_YELLOW, LOW);
    }
    if (masterMode > 0) masterMode--;
     sm.transition();
  }
  
  
  // 10n+6: calculate 5 minute average
  if (loop6) {
    ledStat = ledStat == LOW ? HIGH : LOW;
    digitalWrite(LED_BUILTIN, ledStat);

    updateAvg();
  }

  // 10n+7: run the data compressor
  if (loop7) {
    compressor();
  }

  // 10n+8: dump values via RS232
  if (loop8 && contDump) {
    printShtValue(shtInStat, inT, inH, "In", '\t');
    printShtValue(shtOutStat, outT, outH, "Out", '\t');
    printf("Eng Temp: %.2f\t", engT);
    printf("Cooler Temp: %.2f\t", coolT);
    printf("Cooler Pwr: %d\t", uint8_t(round(coolOutput)));
    printf("EDH State: %d\t", sm.state());
    printf("Error: 0x%04X\n", _error);
  }

  // run the SHT heater state machine transition function (not used currently)
  shtHeat();

  // 10n+9: update time counters
  if (loop9) {
    seconds++;
    if (seconds > 59) {
      seconds = 0;
      minutes++;
      if (minutes > 59) {
        minutes = 0;
        hours++;
      }
    }
  }

  // run the RS232 command interpreter
  uint32_t savedLoopStartMillis = loopStartMillis;
  uint32_t commandStartMillis = millis();
  if (readCommand()) {
    loopStartMillis += (millis() - commandStartMillis);
  }

  loopCnt <<= 1;
  if (loopCnt > LOOP9BIT) {
    loopCnt = LOOP0BIT;
  }
  uint16_t loopActDurMillis = millis() - loopStartMillis;
  if (loopActDurMillis > loopMaxDurMillis) {
    loopMaxDurMillis = loopActDurMillis;
  }
  if (loopActDurMillis > loopDurationMillis) {
    printf("Overload! Next: %d This took %d\n", loopCnt, loopActDurMillis);
    error(ERR_OVERLOAD);    
  } else {
    delay(loopDurationMillis - loopActDurMillis);
  }    
}

// ===================== Tasks and helpers =====================

// Update avg. and write to compressor FIFO
void updateAvg() {
  inHsum += inH;
  inTsum += inT;
  outHsum += outH;
  outTsum += outT;
  coolTsum += coolT;
  engTsum += engT;

  if (avgCnt == (AVG_BASE - 1)) {  
    inHavg = inHsum / AVG_BASE;
    inTavg = inTsum / AVG_BASE;
    outHavg = outHsum / AVG_BASE;
    outTavg = outTsum / AVG_BASE;
    coolTavg = coolTsum / AVG_BASE;
    engTavg = engTsum / AVG_BASE;
    uint8_t normCoolT;
    if (coolTavg < 0) {
      normCoolT = 0;
    } else if ((coolTavg * (15.0f / 4.0f)) > 15.0f) {
      normCoolT = 15;
    } else {
      normCoolT = uint8_t(round(coolTavg * (15.0f / 4.0f)));
    }
    // Write to compressor FIFO
    putData(uint8_t(round(inHavg * 2.5f)), uint8_t(round((inTavg + 25.0f) * 3)), uint8_t(round(outHavg * 2.5f)), 
            uint8_t(round((outTavg + 25.0f) * 3)), uint8_t((normCoolT << 4) | sm.state()), uint8_t(round((engTavg + 25.0f) * 3)));
    inHsum = 0.0f;
    inTsum = 0.0f; 
    outHsum = 0.0f;
    outTsum = 0.0f; 
    coolTsum = 0.0f;
    engTsum = 0.0f;    
    avgCnt = 0;
  } else {
    avgCnt++;
  }
}

// Called by compressor
void printOutValues(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t v5, uint8_t v6) {
    if (decompDumpRaw) {
    printf("%d/%d %d/%d %d/%d\n", v1, v2, v3, v4, v5, v6);
  } else {
    printf("In Temp/Humi: %.2f/%.2f\tOut Temp/Humi: %.2f/%.2f\tEngine Temp: %.2f Cooler Temp: %.2f State: 0x%02X\n", (v2 / 3.0f) -25.0f, v1 / 2.5f, 
    (v4 / 3.0f) -25.0f, v3 / 2.5f, (v6 / 3.0f) -25.0f, ((v5 >> 4) & 0x0F) / (15.0f / 4.0f), v5 & 0x0F);
  }
}

// Read one wire sensor
float readTempSen(OneWire* ds, uint8_t* addr, uint8_t* data) {
  if (!ds->reset()) {
    error(ERR_SEN_ENG);
    return;
  }
  ds->select(addr);    
  ds->write(0xBE);         // Read Scratchpad

  for (int i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds->read();  
  }

  if (data[8] != OneWire::crc8(data, 8)) {
    error(ERR_SEN_ENG);
    return;
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  
  uint8_t cfg = (data[4] & 0x60);
  // at lower res, the low bits are undefined, so let's zero them
  if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
  else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  //// default is 12 bit resolution, 750 ms conversion time
    
  ds->reset();
  ds->select(addr);
  ds->write(0x44, 1);        // start conversion, with parasite power on at the end

  return (float)raw / 16.0;
}

// Read SHT31 sensor
uint16_t readSHT(SHT31* sht) {
  uint32_t start = micros();
  bool c = sht->isConnected();
  bool b = false;
  if (c) b = sht->read();
  uint16_t stat = SHT_NOT_CONNECTED;
  if (c) stat = sht->readStatus();
  uint32_t duration = micros() - start;
  if (duration > shtMaxReadMicros) shtMaxReadMicros = duration;

  if(!b) {
    shtError = sht->getError();
    return SHT_ERROR;
  }
  return stat;
}

void printShtValue(uint16_t stat, float t, float h, char* name, char term) {
  if (stat == SHT_NOT_CONNECTED) {
    printf("%s probe not connected%c", name, term);
  } else if (stat == SHT_ERROR) {
    printf("%s probe error%c", name, term);
  } else {
    printf("%s Temp/Humi/DewP: %.2f/%.2f/%.2f%c", name, t, h, dewPoint(h, t), term);
  }
}

// SHT31 heat cycle state machine
void shtHeat() {
  static float inTemp;
  static float inHumi;
  static float outTemp;
  static float outHumi;
  static float auxTemp;
  static float auxHumi;
  if (heatCycle == 1 && loop0) {
    heatCycle = 2;
  }
  if (heatCycle == 2 && loop1) {
    heatCycle = 3;
    inTemp = shtIn.getTemperature();
    inHumi = shtIn.getHumidity();
    shtIn.heatOn();
  }
  if (heatCycle == 3 && loop2) {
    heatCycle = 4;    
    outTemp = shtOut.getTemperature();
    outHumi = shtOut.getHumidity();
    shtOut.heatOn();
  }
  if(heatCycle >= 5 && heatCycle < 605) {
    heatCycle++;
  }
  if (heatCycle == 605 && loop1) {
    heatCycle = 606;
    if ((shtIn.getTemperature() - inTemp) < 3.0) {
      error(ERR_IN_SHT_HEATER);
    } else {
      if ((inHumi - shtIn.getHumidity()) < 5.0) {
        error(ERR_IN_SHT_HUMI);
      }
    }
    shtIn.heatOff();
  }
  if (heatCycle == 606 && loop2) {
    heatCycle = 607;
    if ((shtOut.getTemperature() - outTemp) < 3.0) {
      error(ERR_OUT_SHT_HEATER);
    } else {
      if ((outHumi - shtOut.getHumidity()) < 5.0) {
        error(ERR_OUT_SHT_HUMI);
      }
    }
    shtOut.heatOff();
  }
  if (heatCycle >= 608 && heatCycle < 2410) {
    heatCycle++;
  }
  if (heatCycle >= 2410) {
    heatCycle = 0;
  }
}

// Input/Output called from main state machine
void pump(bool on) {
  if (on) {
    digitalWrite(PUMP, HIGH);
    yellowLED = LED_BLINK;
  } else {
    digitalWrite(PUMP, LOW);
    yellowLED = LED_OFF;
  }
}
void cooler(bool on) {
  if (on) {
    digitalWrite(COOLER_MAINS, HIGH);
    blueLED = LED_ON;
  } else {
    digitalWrite(COOLER_MAINS, LOW);
    blueLED = LED_OFF;
    error(ERR_COOLER);
  }
}
float getInT() { return inT; }
float getInH() { return inH; }
float getOutT() { return outT; }
float getOutH() { return outH; }
float getCoolT() { return coolT; }
float getEngT() { return engT; }

// Calculate dew point
float dewPoint(float RH, float T) {
  double h = (log10(RH) - 2.0) / 0.4343 + (17.62 * T) / (243.12 + T); 
  return 243.12 * h / (17.62 - h);
}

// Commandline interface
bool readCommand() {
    if (Serial.available()) {
    lastInByte = inByte;
    inByte = Serial.read();
    if (inByte == '\r') {
      inByte = '\n';
    }
    printf("%c", inByte);
    if (inByte == '\n') {
      switch (lastInByte) {
        case 'h':
          printf("c: clean eeprom (master mode only)\n");
          printf("d: dump logged values\n");
          printf("e: dump eeprom\n");
          printf("f: force heat cycle (master mode only)\n");
          printf("h: print this\n");
          printf("m: enter master mode for five seconds\n");
          printf("p: print probe data\n");
          printf("r: dump logged raw values\n");
          printf("s: print status\n");
          printf("t: toggle continuous dump\n");
        break;
        case 't':
          contDump = !contDump;
        break;      
        case 'm':
          printf("Master mode!\n");
          masterMode = 5;
        break;      
        case 'c':
          if (masterMode != 0) {
            printf("Cleaning eeprom\n");            
            eepromInitAll();
            printf("Done\n>");
            return true;
          } else {
            printf("Not in master mode!\n");
          }
        break;      
        case 'f':
          if (masterMode != 0) {
            if (heatCycle == 0) {
              heatCycle = 1;
              printf("Starting heat cycle\n");
            } else {
              printf("Heat cycle already in progress\n");
            }
          } else {
            printf("Not in master mode!\n");
          }
        break;      
        case 'p':
          printf("In Stat: 0x%X   Out Stat: 0x%X\n", shtInStat, shtOutStat);
          printShtValue(shtInStat, inT, inH, "In", '\t');
          printShtValue(shtOutStat, outT, outH, "Out", '\t');
        break;
        case 'd':
          printf("Dumping loged values\n");
          eepromInitReadPtr();
          decompDumpRaw = false;
          for (uint16_t i = 0; i < eepromCapacityInRecords(); i++) {
            eepromAdvanceReadPtr();
            uint16_t ptr = eepromGetReadPtr();
            decompressEntry();
            eepromSetReadPtr(ptr);            
          }
          printf("Done\n>");
          return true;
        break;
        case 'r':
          printf("Dumping loged values\n");
          eepromInitReadPtr();
          decompDumpRaw = true;
          for (uint16_t i = 0; i < eepromCapacityInRecords(); i++) {
            eepromAdvanceReadPtr();
            uint16_t ptr = eepromGetReadPtr();
            decompressEntry();
            eepromSetReadPtr(ptr);            
          }
          printf("Done\n>");
          return true;          
        break;
        case 'e':
          printf("Dumping EEPROM:\n");
          for (uint16_t i = 0; i < eepromCapacityInRecords(); i++) {
            printf("%02X %02X %02X %02X %02X %02X\n", EEPROM.read(i * sizeof(compressorRecord)), EEPROM.read(i * sizeof(compressorRecord) + 1),
                   EEPROM.read(i * sizeof(compressorRecord) + 2), EEPROM.read(i * sizeof(compressorRecord) + 3), 
                   EEPROM.read(i * sizeof(compressorRecord) + 4), EEPROM.read(i * sizeof(compressorRecord) + 5));
            uint16_t filler;
            EEPROM.get(i * sizeof(compressorRecord) + 6, filler);
            printf("%04X\n", filler);
            for (uint16_t j = 0; j < 15; j++) {
              uint64_t data;
              EEPROM.get(i * sizeof(compressorRecord) + j * 8 + 8, data);
              printf("%016llX%c", data, ((j + 1) % 5) == 0 ? '\n' : ' ');
            }
          }
          printf("Done\n>");
          return true;
        break;
        case 's':
          printf("Status:\n");
          printf("Uptime %d:%02d:%02d\n", hours, minutes, seconds);
          printf("Max I2C read: %dus\n", shtMaxReadMicros);
          printf("Max loop duration: %dms\n", loopMaxDurMillis);
          printf("SHT error: 0x%X\n", shtError);
          printf("Heat cycle: %d\n", heatCycle);
          uint8_t ne = eepromNumberOfErrors();
          if (ne == 0) {
            printf("No stored errors\n");
          } else {
            printf("Stored errors:\n");
            for (uint8_t i = 0; i < ne; i++) {
              uint8_t code;
              uint16_t hours;
              uint8_t minutes;
              uint8_t seconds;
              eepromGetError(i, &code, &hours, &minutes, &seconds);
              if (code != 0xFF) {
                printf("After %d:%02d:%02d Error 0x%lX\n", hours, minutes, seconds, 1 << code);
              }
            }
          }
        break;
      }
      printf(">");
      return false;
    }
  }
}

// Throw error
void error (uint32_t code) {
  if (code == 0) return;
  _error |= code;
  uint8_t n = 1;
  if ((code & 0x0000FFFF) == 0) {n = n +16; code = code >>16;}
  if ((code & 0x000000FF) == 0) {n = n + 8; code = code >> 8;}
  if ((code & 0x0000000F) == 0) {n = n + 4; code = code >> 4;}
  if ((code & 0x00000003) == 0) {n = n + 2; code = code >> 2;}
  code = n - (code & 1);
  eepromAddError(code, hours, minutes, seconds);
}

// Check for error
bool hasError(uint32_t code) {
  return (_error & code) != 0;
}