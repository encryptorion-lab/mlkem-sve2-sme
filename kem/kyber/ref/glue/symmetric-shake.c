/*
 * Glue SHAKE wiring for the imported pqcrystals Kyber (round 3) reference.
 *
 * The upstream third_party symmetric-shake.c implements kyber_shake128_absorb /
 * kyber_shake256_prf against the PQClean incremental FIPS202 API
 * (shake128_absorb_once). This project ships a one-shot FIPS202 in common/, so
 * we compile this file in place of the upstream one and reimplement just those
 * two helpers on top of common/fips202.
 *
 * All other XOF/PRF wiring is satisfied by the pristine upstream symmetric.h
 * (xof_init -> shake128_inc_init, xof_squeezeblocks -> shake128_squeezeblocks,
 * xof_release -> shake128_inc_ctx_release, kdf/hash_* -> shake256/sha3_*), all
 * of which exist in common/fips202. The upstream header types the XOF state as
 * shake128incctx whereas common's one-shot squeezeblocks takes shake128ctx;
 * the two structs are layout-identical, hence the build allows the aliasing
 * (-Wno-incompatible-pointer-types).
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "params.h"
#include "symmetric.h"
#include "fips202.h"

/* SHAKE128 absorb specialised for Kyber matrix generation. */
void kyber_shake128_absorb(xof_state *state, const uint8_t seed[KYBER_SYMBYTES],
                           uint8_t x, uint8_t y)
{
  uint8_t extseed[KYBER_SYMBYTES + 2];

  memcpy(extseed, seed, KYBER_SYMBYTES);
  extseed[KYBER_SYMBYTES + 0] = x;
  extseed[KYBER_SYMBYTES + 1] = y;

  /* One-shot absorb (common/fips202): fully (re)initialises the state, so the
   * preceding xof_init / shake128_inc_init is harmless. */
  shake128_absorb((shake128ctx *)state, extseed, sizeof(extseed));
}

/* SHAKE256 as a PRF over (key || nonce). */
void kyber_shake256_prf(uint8_t *out, size_t outlen,
                        const uint8_t key[KYBER_SYMBYTES], uint8_t nonce)
{
  uint8_t extkey[KYBER_SYMBYTES + 1];

  memcpy(extkey, key, KYBER_SYMBYTES);
  extkey[KYBER_SYMBYTES] = nonce;

  shake256(out, outlen, extkey, sizeof(extkey));
}
