/*
  This is a modified version of the duco_hash.cpp to include DUCOO-S1A Hasher to works both,
  via single core USB and dual core I2C.
  Modified by feltoxxx
  01/03/2025

*/

#pragma once

#include <Arduino.h>

#define SHA1_BLOCK_LEN 64
#define SHA1_HASH_LEN 20

struct duco_hash_state_t {
    uint8_t buffer[SHA1_BLOCK_LEN];
    uint8_t result[SHA1_HASH_LEN];
    uint32_t tempState[5];

    uint8_t block_offset;
    uint8_t total_bytes;

    // Agregado: buffer de trabajo para SHA1 (16 enteros de 32 bits)
    uint32_t workBuffer[16];
};

/* For 8-bit microcontrollers we should use 16 bit variables since the
difficulty is low, for all the other cases should be 32 bits. */
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_MEGAAVR)
  typedef uint32_t uintDiff;
#else
  typedef uint32_t uintDiff;
#endif

void duco_hash_init(duco_hash_state_t * hasher, char const * prevHash);

uint8_t const * duco_hash_try_nonce(duco_hash_state_t * hasher, char const * nonce);

void lowercase_hex_to_bytes(char const * hexDigest, uint8_t * rawDigest);

uintDiff ducos1a(duco_hash_state_t *hash, char const * prevBlockHash, char const * targetBlockHash, uintDiff difficulty);

uintDiff ducos1a_mine(duco_hash_state_t *hash, char const * prevBlockHash, uint8_t const * target, uintDiff maxNonce);
