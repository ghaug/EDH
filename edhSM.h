#ifndef edhSM_H
#define edhSM_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "unitTests/unitTest.h"
#endif

class edhSM {
private:
  enum states {
    STATE_INIT                 = 0,
    STATE_NORMAL_OP            = 1,
    STATE_PAUSE_TEMP_LOW       = 2,
    STATE_PAUSE_ENG_BELOW_DEWP = 3,
    STATE_PAUSE_HUMI_HI        = 4,
    STATE_COOLER_FAIL          = 5
  };

public:
  edhSM(float (*inT)(), float (*inH)(), float (*outT)(), float (*outH)(), float (*cT)(), float (*eT)(), void (*cooler)(bool b), void (*pump)(bool b), float coolerSet) :
    _inT(inT), _inH(inH), _outT(outT), _outH(outH), _cT(cT), _eT(eT),
    _cooler(cooler), _pump(pump), _state(STATE_INIT), _count(0), 
    _coolerSet(coolerSet), _countLimit(3600) {}

  void transition();
  uint8_t state() { return _state; }

private:
  const float minTempDelta = 5.0f;
  const float maxHumi = 70.0f;
  
  float _minTemp;
  float _tempSum;

  uint8_t  _state;
  uint16_t _count;
  uint16_t _countLimit;
  
  float    _coolerSet;
  float    (*_inT)();
  float    (*_inH)();
  float    (*_outT)();
  float    (*_outH)();
  float    (*_cT)();
  float    (*_eT)();  
  void     (*_cooler)(bool b);
  void     (*_pump)(bool b);
};

#endif
