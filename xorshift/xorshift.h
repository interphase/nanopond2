/* optimized xorshift+ generator */

/* adapted from https://en.wikipedia.org/wiki/Xorshift */
#include <stdint.h>

// xorshift*
/* vebatim https://en.wikipedia.org/wiki/Xorshift */
uint64_t xorshift64star(uint64_t state) {
  uint64_t x = state;
  for (int i = 0; i < 32; i++) {
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    x *= UINT64_C(2685821657736338717);
  }
  return x;
}
void init_xorgen(uint64_t sd);
static inline uint32_t xor_genrand_uint32();
static inline uint64_t xor_genrand_uint64();

#define genrand_uint32 xor_genrand_uint32
#define genrand_uint64 xor_genrand_uint64

#include <immintrin.h>
#ifdef __AVX2__
#define XNGEN 2 // value of 2 here seems to allow rdata to stay in cache
#include "xorshift_avx2.h"
#else // __AVX2__
#ifdef __SSE4_1__
#define XNGEN 1 // AVX1 and lower are faster with 1 because of smaller state
#include "xorshift_sse4.h"
#endif // __SSE4__
#endif // __AVX2__
