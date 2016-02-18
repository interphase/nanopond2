__m128i xorstate[2];

#ifdef __AES__
void init_xorgen(uint64_t sd) {
  register __m128i e;
  e = _mm_insert_epi64(e, sd, 0);
  // spending a bit more time here seems to increase the quality of the
  // randomness slightly
  for (int i = 0; i < 16; i++) { e = _mm_aesenc_si128(e, e); }
  _mm_store_si128(&(xorstate[0]), e);
  for (int i = 0; i < 16; i++) { e = _mm_aesenc_si128(e, e); }
  _mm_store_si128(&(xorstate[1]), e);
}
#else // __AES__
void init_xorgen(uint64_t sd) {
  register __m128i e;

  for (int i = 0; i < 1024; i++) {
    sd = xorshift64star(sd);
  }

  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 1);
  _mm_store_si128(&(xorstate[0]), e);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 0);
  sd = xorshift64star(sd); e = _mm_insert_epi64(e, sd, 1);
  _mm_store_si128(&(xorstate[1]), e);
}
#endif // __AES__
inline __m128i xor_gen() {
  register __m128i x = _mm_load_si128(&(xorstate[0]));
  register __m128i y = _mm_load_si128(&(xorstate[1]));
  register __m128i z, w;

  _mm_store_si128(&(xorstate[0]), y);

  // x ^= x << 23; // a
  z = _mm_slli_epi64(x, 23);
  x = _mm_xor_si128(x, z);

  // s[1] = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
  z = _mm_srli_epi64 (y, 26);
  w = _mm_srli_epi64 (x, 17);
  z = _mm_xor_si128(w, z);
  z = _mm_xor_si128(y, z);
  z = _mm_xor_si128(x, z);
  _mm_store_si128(&(xorstate[1]), z);

  // return s[1] + y;
  return _mm_add_epi64(z, y);
}

uint32_t xor_genrand_uint32() {
#define XNGEN 1
  static char i = 0;
  static union {
    uint32_t i[XNGEN*8];
    __m128i mm[XNGEN*2];
  } rdata;

  if (i == 0 || i >= XNGEN*8) {
    i = 0;
    for (int j = 0; j < XNGEN*2; j++) {
      rdata.mm[j] = xor_gen();
    }
  }
  return rdata.i[i++];
}
