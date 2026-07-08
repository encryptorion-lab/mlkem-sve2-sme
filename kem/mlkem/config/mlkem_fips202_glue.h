#ifndef MLK_SVE2_SME_FIPS202_GLUE_H
#define MLK_SVE2_SME_FIPS202_GLUE_H

#include "fips202.h"

#define mlk_shake128ctx shake128ctx
#define mlk_shake128_init(STATE) ((void)(STATE))
#define mlk_shake128_absorb_once shake128_absorb
#define mlk_shake128_squeezeblocks shake128_squeezeblocks
#define mlk_shake128_release shake128_ctx_release

#define mlk_shake256 shake256
#define mlk_sha3_256 sha3_256
#define mlk_sha3_512 sha3_512

#endif
