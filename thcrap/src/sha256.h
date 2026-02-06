/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * SHA-256 hash implementation
  * From https://web.archive.org/web/20120415202539/http://bradconte.com:80/sha256_c.html
  */

#pragma once
#include <stdint.h>
#include <immintrin.h>

typedef union {
	uint32_t dwords[8];
	uint64_t qwords[4];
	uint8_t bytes[32];
	struct {
		__m128i lower_simd;
		__m128i upper_simd;
	};
} SHA256_HASH;
typedef char sha256_str_t[65];

SHA256_HASH sha256_calc(const uint8_t data[], size_t length);
void sha256_to_string(SHA256_HASH hash, sha256_str_t hash_str);
