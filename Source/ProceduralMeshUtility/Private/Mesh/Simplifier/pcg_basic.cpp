/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

/*
 * Copyright 2019 Nuraga Wiswakarma
 *
 * Unreal Engine 4 Port
 */

/*
 * This code is derived from the full C implementation, which is in turn
 * derived from the canonical C++ PCG implementation. The C++ version
 * has many additional features and is preferable if you can use C++ in
 * your project.
 */

#include "pcg_basic.h"

#if PCG_ENABLE_GLOBAL_STATE
// state for global RNGs
static pcg32_random_t pcg32_global = PCG32_INITIALIZER;
#endif

void FPMUPCG::pcg32_srandom_r(pcg32_random_t* rng, uint64 initstate, uint64 initseq)
{
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng->state += initstate;
    pcg32_random_r(rng);
}

#pragma warning( push )
#pragma warning( disable : 4146 )

uint32 FPMUPCG::pcg32_random_r(pcg32_random_t* rng)
{
    uint64 oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32 rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

uint32 FPMUPCG::pcg32_boundedrand_r(pcg32_random_t* rng, uint32 bound)
{
    // To avoid bias, we need to make the range of the RNG a multiple of
    // bound, which we do by dropping output less than a threshold.
    // A naive scheme to calculate the threshold would be to do
    //
    //     uint32_t threshold = 0x100000000ull % bound;
    //
    // but 64-bit div/mod is slower than 32-bit div/mod (especially on
    // 32-bit platforms).  In essence, we do
    //
    //     uint32_t threshold = (0x100000000ull-bound) % bound;
    //
    // because this version will calculate the same modulus, but the LHS
    // value is less than 2^32.

    uint32 threshold = -bound % bound;

    // Uniformity guarantees that this loop will terminate.  In practice, it
    // should usually terminate quickly; on average (assuming all bounds are
    // equally likely), 82.25% of the time, we can expect it to require just
    // one iteration.  In the worst case, someone passes a bound of 2^31 + 1
    // (i.e., 2147483649), which invalidates almost 50% of the range.  In 
    // practice, bounds are typically small and only a tiny amount of the range
    // is eliminated.
    for (;;) {
        uint32 r = pcg32_random_r(rng);
        if (r >= threshold)
            return r % bound;
    }
}

#pragma warning( pop ) // warning( disable : 4146 )

#if PCG_ENABLE_GLOBAL_STATE

void pcg32_srandom(uint64 seed, uint64 seq)
{
    pcg32_srandom_r(&pcg32_global, seed, seq);
}

uint32 pcg32_random()
{
    return pcg32_random_r(&pcg32_global);
}

uint32 pcg32_boundedrand(uint32 bound)
{
    return pcg32_boundedrand_r(&pcg32_global, bound);
}

#endif // PCG_ENABLE_GLOBAL_STATE
