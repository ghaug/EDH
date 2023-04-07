#include <string.h>
#include "unitTest.h"

#include "../edhSM.h"

bool pump = true;
bool cooler = true;
float csp = 1.0f;
float iT, iH, oT, oH, aT, aH;
float fiT() { return iT; }
float fiH() { return iH; }
float foT() { return oT; }
float foH() { return oH; }
float faT() { return aT; }
float faH() { return aH; }
void  pf(bool b) {pump = b; }
void  cf(bool b) {cooler = b; }

edhSM sm( fiT, fiH, foT, foH, faT, cf, pf, csp );

int main(int argn, char** argv) {
  FILE* f = fopen(argv[1], "r");
  if (f == NULL) {
    return 1;
  }

  int i = 0;
  int lastState = 0;
  bool lastPump = true;
  bool lastCooler = true;
  while(EOF != fscanf(f, "In Temp/Humi: %f/%f\tOut Temp/Humi: %f/%f\tAux Temp/Humi: %f/%f\tError: 0x0000\n", &iT, &iH, &oT, &oH, &aT, &aH)) {
    i++;
    sm.transition();
    if (sm.state() != lastState) {
      printf("%d: %d -> %d\n", i, lastState, sm.state());
    }
    if (lastPump && !pump) {
      printf("%d: pump off", i);
    }
    if (!lastPump && pump) {
      printf("%d: pump on", i);
    }
    if (lastCooler && !cooler) {
      printf("%d: cooler off", i);
    }
    if (!lastCooler && cooler) {
      printf("%d: cooler on", i);
    }
    lastPump = pump;
    lastCooler = cooler;
    lastState = sm.state();
  }
  fclose(f);
  return 0;
}
