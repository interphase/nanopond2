__m256i xorstate[2];

#ifdef __AES__
void init_xorgen(uint64_t sd) {
  register __m128i e = {0, 0};
  register __m256i x = {0, 0, 0, 0};
  register __m256i y = {0, 0, 0, 0};
  e = _mm_insert_epi64(e, sd, 0);

  // spending a bit more time here seems to increase the quality of the
  // randomness slightly
  for (int i = 0; i < 8; i++) { e = _mm_aesenc_si128(e, e); }
  x = _mm256_inserti128_si256(x, e, 0);
  for (int i = 0; i < 8; i++) { e = _mm_aesenc_si128(e, e); }
  x = _mm256_inserti128_si256(x, e, 1);

  for (int i = 0; i < 8; i++) { e = _mm_aesenc_si128(e, e); }
  y = _mm256_inserti128_si256(y, e, 0);
  for (int i = 0; i < 8; i++) { e = _mm_aesenc_si128(e, e); }
  y = _mm256_inserti128_si256(y, e, 1);


  _mm256_store_si256(&(xorstate[0]), x);
  _mm256_store_si256(&(xorstate[1]), y);
}
#else // __AES__
void init_xorgen(uint64_t sd) {
  register __m128i e = {0, 0};
  register __m256i x = {0, 0, 0, 0};
  register __m256i y = {0, 0, 0, 0};
  for (int i = 0; i < 1024; i++) {
    sd = xorshift64star(sd);
  }

  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 1);
  x = _mm256_inserti128_si256(x, e, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 1);
  x = _mm256_inserti128_si256(x, e, 1);

  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 1);
  y = _mm256_inserti128_si256(y, e, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 1);
  y = _mm256_inserti128_si256(y, e, 1);

  _mm256_store_si256(&(xorstate[0]), x);
  _mm256_store_si256(&(xorstate[1]), y);
}
#endif // __AES__
static inline __m256i xor_gen() {
  register __m256i x = _mm256_load_si256(&(xorstate[0]));
  register __m256i y = _mm256_load_si256(&(xorstate[1]));
  register __m256i z, w;

  _mm256_store_si256(&(xorstate[0]), y);

  // x ^= x << 23; // a
  z = _mm256_slli_epi64(x, 23);
  x = _mm256_xor_si256(x, z);

  // s[1] = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
  z = _mm256_srli_epi64(y, 26);
  w = _mm256_srli_epi64(x, 17);
  z = _mm256_xor_si256(w, z);
  z = _mm256_xor_si256(y, z);
  z = _mm256_xor_si256(x, z);
  _mm256_store_si256(&(xorstate[1]), z);

  // return s[1] + y;
  return _mm256_add_epi64(z, y);
}

struct rdata {
  union {
    uint32_t i32[XNGEN*8];
    uint64_t i64[XNGEN*4];
    __m256i  mm[XNGEN];
  } bits;
  uint16_t idx;
};


#define XOR_GENERATOR(bufsize) \
static struct rdata rd;\
if (rd.idx == 0 || rd.idx >= XNGEN*bufsize) {\
  rd.idx = 0;\
  for (int j = 0; j < XNGEN; j++) {\
    rd.bits.mm[j] = xor_gen();\
  }\
}\

uint32_t xor_genrand_uint32() {
  XOR_GENERATOR(8)
  return rd.bits.i32[rd.idx++];
}

uint64_t xor_genrand_uint64() {
  XOR_GENERATOR(4)
  return rd.bits.i64[rd.idx++];
}
