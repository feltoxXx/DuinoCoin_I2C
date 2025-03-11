/*
  This is a modified version of the duco_hash.cpp to include DUCOO-S1A Hasher to works both,
  via single core USB and dual core I2C.
  Modified by feltoxxx
  01/03/2025

*/

#include "duco_hash.h"

#pragma GCC optimize ("-Ofast")

#define sha1_rotl(bits,word)     (((word) << (bits)) | ((word) >> (32 - (bits))))

void duco_hash_block(duco_hash_state_t * hasher) {
    // Usamos el buffer de trabajo propio del estado
    uint32_t *w = hasher->workBuffer;

    for (uint8_t i = 0, i4 = 0; i < 16; i++, i4 += 4) {
        w[i] = (uint32_t(hasher->buffer[i4]) << 24) |
               (uint32_t(hasher->buffer[i4 + 1]) << 16) |
               (uint32_t(hasher->buffer[i4 + 2]) << 8) |
               (uint32_t(hasher->buffer[i4 + 3]));
    }
    
    uint32_t a = hasher->tempState[0];
    uint32_t b = hasher->tempState[1];
    uint32_t c = hasher->tempState[2];
    uint32_t d = hasher->tempState[3];
    uint32_t e = hasher->tempState[4];

    for (uint8_t i = 10; i < 80; i++) {
        if (i >= 16) {
            w[i & 15] = sha1_rotl(1, w[(i-3) & 15] ^ w[(i-8) & 15] ^ w[(i-14) & 15] ^ w[(i-16) & 15]);
        }

        uint32_t temp = sha1_rotl(5, a) + e + w[i & 15];
        if (i < 20) {
            temp += (b & c) | ((~b) & d);
            temp += 0x5a827999;
        } else if (i < 40) {
            temp += b ^ c ^ d;
            temp += 0x6ed9eba1;
        } else if (i < 60) {
            temp += (b & c) | (b & d) | (c & d);
            temp += 0x8f1bbcdc;
        } else {
            temp += b ^ c ^ d;
            temp += 0xca62c1d6;
        }

        e = d;
        d = c;
        c = sha1_rotl(30, b);
        b = a;
        a = temp;
    }

    a += 0x67452301;
    b += 0xefcdab89;
    c += 0x98badcfe;
    d += 0x10325476;
    e += 0xc3d2e1f0;

    hasher->result[0] = a >> 24;
    hasher->result[1] = a >> 16;
    hasher->result[2] = a >> 8;
    hasher->result[3] = a;
    hasher->result[4] = b >> 24;
    hasher->result[5] = b >> 16;
    hasher->result[6] = b >> 8;
    hasher->result[7] = b;
    hasher->result[8] = c >> 24;
    hasher->result[9] = c >> 16;
    hasher->result[10] = c >> 8;
    hasher->result[11] = c;
    hasher->result[12] = d >> 24;
    hasher->result[13] = d >> 16;
    hasher->result[14] = d >> 8;
    hasher->result[15] = d;
    hasher->result[16] = e >> 24;
    hasher->result[17] = e >> 16;
    hasher->result[18] = e >> 8;
    hasher->result[19] = e;
}

void duco_hash_init(duco_hash_state_t * hasher, char const * prevHash) {
    memcpy(hasher->buffer, prevHash, 40);

    if (prevHash == (void*)(0xffffffff)) {
        // Esto se conserva solo para evitar que el compilador haga inline de la función
        duco_hash_block(hasher);
    }

    // Inicialización del estado SHA1
    uint32_t a = 0x67452301;
    uint32_t b = 0xefcdab89;
    uint32_t c = 0x98badcfe;
    uint32_t d = 0x10325476;
    uint32_t e = 0xc3d2e1f0;

    // Usamos un arreglo local para los primeros 10 rounds
    uint32_t w[10];

    for (uint8_t i = 0, i4 = 0; i < 10; i++, i4 += 4) {
        w[i] = (uint32_t(hasher->buffer[i4]) << 24) |
               (uint32_t(hasher->buffer[i4 + 1]) << 16) |
               (uint32_t(hasher->buffer[i4 + 2]) << 8) |
               (uint32_t(hasher->buffer[i4 + 3]));
    }

    for (uint8_t i = 0; i < 10; i++) {
        uint32_t temp = sha1_rotl(5, a) + e + w[i];
        temp += (b & c) | ((~b) & d);
        temp += 0x5a827999;

        e = d;
        d = c;
        c = sha1_rotl(30, b);
        b = a;
        a = temp;
    }

    hasher->tempState[0] = a;
    hasher->tempState[1] = b;
    hasher->tempState[2] = c;
    hasher->tempState[3] = d;
    hasher->tempState[4] = e;
}

void duco_hash_set_nonce(duco_hash_state_t * hasher, char const * nonce) {
	uint8_t * b = hasher->buffer;

	uint8_t off = SHA1_HASH_LEN * 2;
	for (uint8_t i = 0; i < 10 && nonce[i] != 0; i++) {
		b[off++] = nonce[i];
	}

	uint8_t total_bytes = off;

	b[off++] = 0x80;
	while (off < 62) {
		b[off++] = 0;
	}

	b[62] = total_bytes >> 5;
	b[63] = total_bytes << 3;
}

uint8_t const * duco_hash_try_nonce(duco_hash_state_t * hasher, char const * nonce) {
	duco_hash_set_nonce(hasher, nonce);
	duco_hash_block(hasher);

	return hasher->result;
}

// // DUCO-S1A hasher
uintDiff ducos1a(duco_hash_state_t *hash, char const * prevBlockHash, char const * targetBlockHash, uintDiff difficulty) {

    uint8_t target[SHA1_HASH_LEN];
    lowercase_hex_to_bytes(targetBlockHash, target);
    uintDiff const maxNonce = difficulty * 100 + 1;
    return ducos1a_mine(hash, prevBlockHash, target, maxNonce);

}

void lowercase_hex_to_bytes(char const * hexDigest, uint8_t * rawDigest) {
  for (uint8_t i = 0, j = 0; j < SHA1_HASH_LEN; i += 2, j += 1) {
    uint8_t x = hexDigest[i];
    uint8_t b = x >> 6;
    uint8_t r = ((x & 0xf) | (b << 3)) + b;
    x = hexDigest[i + 1];
    b = x >> 6;

    rawDigest[j] = (r << 4) | (((x & 0xf) | (b << 3)) + b);
  }
}

uintDiff ducos1a_mine(duco_hash_state_t *hash, char const * prevBlockHash, uint8_t const * target, uintDiff maxNonce) {

  duco_hash_init(hash, prevBlockHash);
  char nonceStr[11];  // 10 + terminador nulo

  for (uintDiff nonce = 0; nonce < maxNonce; nonce++) {

    ultoa(nonce, nonceStr, 10);
    uint8_t const * hash_bytes = duco_hash_try_nonce(hash, nonceStr);

    if (memcmp(hash_bytes, target, SHA1_HASH_LEN) == 0) {
      return nonce;
    }
  }
  
  return 0;
}