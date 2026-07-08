/*
 * Standalone randombytes declaration for the kyber-neon benchmark build.
 *
 * The upstream PQClean sources include "randombytes.h" and call randombytes();
 * liboqs ships a shim that redirects to OQS_randombytes (needs the full liboqs
 * runtime). For this self-contained benchmark we provide our own deterministic
 * randombytes() in bench_kyber_neon.c, so this header only declares it.
 */
#ifndef KYBER_NEON_RANDOMBYTES_H
#define KYBER_NEON_RANDOMBYTES_H

#include <stddef.h>
#include <stdint.h>

void randombytes(uint8_t *out, size_t outlen);

#endif
