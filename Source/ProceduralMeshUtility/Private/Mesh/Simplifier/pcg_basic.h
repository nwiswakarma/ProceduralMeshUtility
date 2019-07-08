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
 *     http://www.pcg-random.org
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

#pragma once

#include "CoreMinimal.h"

#ifndef PCG_ENABLE_GLOBAL_STATE
#define PCG_ENABLE_GLOBAL_STATE 0
#endif

// If you *must* statically initialize it, here's one.

#if PCG_ENABLE_GLOBAL_STATE
#define PCG32_INITIALIZER   { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
#endif

class FPMUPCG
{
public:

    // Internals are *Private*.
    // RNG state.  All values are possible.
    // Controls which RNG sequence (stream) is
    // selected. Must *always* be odd.
    struct pcg_state_setseq_64
    {
        uint64 state;
        uint64 inc;
    };

    typedef struct pcg_state_setseq_64 pcg32_random_t;
    typedef struct pcg_state_setseq_64 FRNG;

    // pcg32_srandom(initstate, initseq)
    // pcg32_srandom_r(rng, initstate, initseq):
    //     Seed the rng.  Specified in two parts, state initializer and a
    //     sequence selection constant (a.k.a. stream id)

    static void pcg32_srandom_r(pcg32_random_t* rng, uint64 initstate,
                         uint64 initseq);

    // pcg32_random()
    // pcg32_random_r(rng)
    //     Generate a uniformly distributed 32-bit random number

    static uint32 pcg32_random_r(pcg32_random_t* rng);

    // pcg32_boundedrand(bound):
    // pcg32_boundedrand_r(rng, bound):
    //     Generate a uniformly distributed number, r, where 0 <= r < bound

    static uint32 pcg32_boundedrand_r(pcg32_random_t* rng, uint32 bound);

#if PCG_ENABLE_GLOBAL_STATE
    static void pcg32_srandom(uint64 initstate, uint64 initseq);
    static uint32 pcg32_random(void);
    static uint32 pcg32_boundedrand(uint32 bound);
#endif // PCG_ENABLE_GLOBAL_STATE
};
