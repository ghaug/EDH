extern uint8_t _eeprom[4096];

class EEPROMclass {
public:
  static uint16_t length() { return 4096; }
  
  uint8_t read( int idx )              { return _eeprom[idx]; }
  void write( int idx, uint8_t val )   { _eeprom[idx] = val; }
  void update( int idx, uint8_t val )  { _eeprom[idx] = val;; }

  
  //Functionality to 'get' and 'put' objects to and from EEPROM.
  template< typename T > T &get( int idx, T &t ){
      uint8_t* e = _eeprom + idx;
      uint8_t *ptr = (uint8_t*) &t;
      for( int count = sizeof(T) ; count ; --count, ++e )  *ptr++ = *e;
      return t;
  }
  
  template< typename T > const T &put( int idx, const T &t ){
      uint8_t* e = _eeprom + idx;
      const uint8_t *ptr = (const uint8_t*) &t;
      for( int count = sizeof(T) ; count ; --count, ++e )  *e = ( *ptr++ );
      return t;
  }
};

extern EEPROMclass EEPROM;
