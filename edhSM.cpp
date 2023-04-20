#ifdef ARDUINO
#include <Arduino.h>
#else
#include "unitTests/unitTest.h"
#endif
#include "edhSM.h"

extern float dewPoint(float RH, float T);

void edhSM::transition() {
  float inT;
  switch(_state) {
    case STATE_INIT:
      _pump(true);
      if (_count < _countLimit) {
        _count++;
      } else {
        _count = 0;
        _state = STATE_NORMAL_OP;
      }
    break;
    case STATE_NORMAL_OP:
      _pump(true);
      inT = _inT();
      if (((inT - _cT()) < 10.0f && inT >= 15.0f) || (_cT() > 4.0f && inT < 15.0f)) {
        // Failure if cooler can't keep delta above 15C or min. below 15C
        _state = STATE_COOLER_FAIL;
      } else if (inT < (_coolerSet + minTempDelta)) {
        // Pause if in temp is below cut off temp (set point of cooler set point plus a minimum delta)
        _state = STATE_PAUSE_TEMP_LOW;
        _minTemp = inT;
        _count = 0;
      } else if (_eT() < (dewPoint(_outH(), _outT()) + minTempDelta)) {
        // Pause if engine temp is below out dew point plus minimum delta
        _state = STATE_PAUSE_ENG_BELOW_DEWP;
        _minTemp = _eT();
      } else if (_outH() > maxHumi) {
        // Pause if out humi > in humi or out humi is above maximum
        _state = STATE_PAUSE_HUMI_HI;
        _minTemp = inT;
        _count = 0;
      }
    break;
    case STATE_PAUSE_TEMP_LOW:
      _pump(false);
      inT = _inT();
      if (inT <= _minTemp) {
        _minTemp = inT;
        _count = 0;
        _tempSum = 0.0f;
      } else {
        _count++;
        _tempSum += (inT >= (minTempDelta + _coolerSet) ? inT : 0.0f);
        // Resume if: integ. temp since minimum is above 5h * cut off temp && one hour since temp minimum && temp above cooler set point + 1.5 times min. delta
        if (_tempSum > (5 * 3600 * (minTempDelta + _coolerSet)) && _count > 3600 && inT > (1.5f * minTempDelta + _coolerSet)) {
          _countLimit = 600; // let it settle for ten minutes
          _count = 0;
          _state = STATE_INIT;
        }
      }
    break;
    case STATE_PAUSE_ENG_BELOW_DEWP:
      _pump(false);
      // Resume if engine temp. is greater than cooler temp. plus min. temp. delta or has increased by min. temp. delta
      if (_eT() > (_cT() + minTempDelta) || _eT() > (_minTemp + minTempDelta)) {
        _count = 0;
        _countLimit = 600;
        _state = STATE_INIT;        
      }
    break;    
    case STATE_PAUSE_HUMI_HI:
      _pump(false);
      _count++;
      // Resume after five hours or after one hour if temp has increased more than min. temp. delta
      if (_count > (5 * 3600) || (_count > 3600 && _inT() > (_minTemp + minTempDelta))) {
        _count = 0;
        _countLimit = 600;
        _state = STATE_INIT;
      }
    break;
    case STATE_COOLER_FAIL:
      // Cooler seems to be faulty nothing we can do
      _pump(false);
      _cooler(false);
  }
}
