#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#undef DEBUG_DATABUFFER

#ifdef DEBUG_DATABUFFER
#define DBG_DB_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_DB_PRINT(...)
#endif

#undef DEBUG_COMPBUFFER

#ifdef DEBUG_COMPBUFFER
#define DBG_COMP_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_COMP_PRINT(...)
#endif

#undef DEBUG_DECOMPBUFFER

#ifdef DEBUG_DECOMPBUFFER
#define DBG_DECOMP_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_DECOMP_PRINT(...)
#endif

void compressor();
void decompressEntry();
void putData(uint8_t h1, uint8_t t1, uint8_t h2, uint8_t t2, uint8_t h3, uint8_t t3);
extern bool decompDumpRaw;

#endif // COMPRESSOR_H
