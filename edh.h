#ifndef EDH_H
#define EDH_H

#define loopDurationMillis 100
#define LOOP0BIT 0x0001
#define LOOP1BIT 0x0002
#define LOOP2BIT 0x0004
#define LOOP3BIT 0x0008
#define LOOP4BIT 0x0010
#define LOOP5BIT 0x0020
#define LOOP6BIT 0x0040
#define LOOP7BIT 0x0080
#define LOOP8BIT 0x0100
#define LOOP9BIT 0x0200

#define ERR_OVERLOAD       0x00000001
#define ERR_SHT_IN         0x00000002
#define ERR_SHT_OUT        0x00000004
#define ERR_SEN_COOL       0x00000008
#define ERR_IN_SHT_HEATER  0x00000010
#define ERR_IN_SHT_HUMI    0x00000020
#define ERR_OUT_SHT_HEATER 0x00000040
#define ERR_OUT_SHT_HUMI   0x00000080
#define ERR_AUX_SHT_HEATER 0x00000100
#define ERR_AUX_SHT_HUMI   0x00000200
#define ERR_SEN_ENG        0x00000400
#define ERR_COOLER         0x00000800
#define ERR_ANY            0xFFFFFFFF

#define loop0 (loopCnt & LOOP0BIT)
#define loop1 (loopCnt & LOOP1BIT)
#define loop2 (loopCnt & LOOP2BIT)
#define loop3 (loopCnt & LOOP3BIT)
#define loop4 (loopCnt & LOOP4BIT)
#define loop5 (loopCnt & LOOP5BIT)
#define loop6 (loopCnt & LOOP6BIT)
#define loop7 (loopCnt & LOOP7BIT)
#define loop8 (loopCnt & LOOP8BIT)
#define loop9 (loopCnt & LOOP9BIT)

#define PUMP A8
#define COOLER 9
#define COOLER_MAINS A6
#define ENGINE_TEMP 10
#define COOLER_TEMP 2
#define LED_BLUE 4
#define LED_YELLOW 6

#define LED_OFF 0
#define LED_ON 1
#define LED_BLINK 2

#define SHT_NOT_CONNECTED 0xFFFF
#define SHT_ERROR 0xFFFE

#define AVG_BASE 300

#define coolSetP 1.0

struct compressorRecord {
    uint8_t base[6];     // six values, eight bits each, humidity is 0 to 250 so 251 to 255 are reserved
    uint16_t filler;     // bit 15 for admin, 0 to 14 are vle delta (first six) or delta-delta 
    uint64_t data[15];   // simple-8b encoded delta or delta-delta
};

#endif