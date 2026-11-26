// Copyright (C) 2025 Vrije Universiteit Brussel. All rights reserved.
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <inttypes.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
 * This is deliberately a compute/front-end workload, not a memory benchmark.
 * The 16 independent accumulator chains expose instruction-level parallelism
 * to a wide out-of-order core.  The branches are driven by a register-only
 * xorshift state, so their outcomes cannot be determined at compile time.
 *
 * A loop body has 59 nominal integer operations: 48 accumulator operations,
 * three PRNG operations, and eight operations in the four branch paths.
 */
#define OPS_PER_LOOP 59
#define LOOPS_PER_TIME_CHECK 64

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static inline uint64_t rotl64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64 - shift));
}

#define MIX(value, addend, shift, multiplier) \
    do {                                      \
        (value) += (addend);                  \
        (value) = rotl64((value), (shift));   \
        (value) *= (multiplier);              \
    } while (0)

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <duration_seconds> <cpu_core_id>\n", argv[0]);
        return 1;
    }

    const double duration = atof(argv[1]);
    const int core_id = atoi(argv[2]);
    if (duration <= 0) {
        fprintf(stderr, "Please provide a positive duration.\n");
        return 1;
    }
    if (core_id < 0) {
        fprintf(stderr, "Please provide a valid (non-negative) CPU core ID.\n");
        return 1;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        perror("sched_setaffinity failed");
        return 1;
    }

    uint64_t a0 = 0x243f6a8885a308d3ULL, a1 = 0x13198a2e03707344ULL;
    uint64_t a2 = 0xa4093822299f31d0ULL, a3 = 0x082efa98ec4e6c89ULL;
    uint64_t a4 = 0x452821e638d01377ULL, a5 = 0xbe5466cf34e90c6cULL;
    uint64_t a6 = 0xc0ac29b7c97c50ddULL, a7 = 0x3f84d5b5b5470917ULL;
    uint64_t a8 = 0x9216d5d98979fb1bULL, a9 = 0xd1310ba698dfb5acULL;
    uint64_t a10 = 0x2ffd72dbd01adfb7ULL, a11 = 0xb8e1afed6a267e96ULL;
    uint64_t a12 = 0xba7c9045f12c7f99ULL, a13 = 0x24a19947b3916cf7ULL;
    uint64_t a14 = 0x0801f2e2858efc16ULL, a15 = 0x636920d871574e69ULL;
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    uint64_t ops = 0;

    const double start = now_sec();
    const double end = start + duration;
    do {
        for (unsigned int batch = 0; batch < LOOPS_PER_TIME_CHECK; ++batch) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;

            MIX(a0, 0x9e3779b97f4a7c15ULL, 13, 3);
            MIX(a1, 0xbf58476d1ce4e5b9ULL, 17, 5);
            MIX(a2, 0x94d049bb133111ebULL, 19, 7);
            MIX(a3, 0xd6e8feb86659fd93ULL, 23, 9);
            MIX(a4, 0xa0761d6478bd642fULL, 29, 11);
            MIX(a5, 0xe7037ed1a0b428dbULL, 31, 13);
            MIX(a6, 0x8ebc6af09c88c6e3ULL, 37, 15);
            MIX(a7, 0x589965cc75374cc3ULL, 41, 17);
            MIX(a8, 0x1d8e4e27c47d124fULL, 43, 19);
            MIX(a9, 0xeb44accab455d165ULL, 47, 21);
            MIX(a10, 0x9ddfea08eb382d69ULL, 7, 23);
            MIX(a11, 0xc2b2ae3d27d4eb4fULL, 11, 25);
            MIX(a12, 0x165667b19e3779f9ULL, 27, 27);
            MIX(a13, 0x85ebca77c2b2ae63ULL, 33, 29);
            MIX(a14, 0x27d4eb2f165667c5ULL, 39, 31);
            MIX(a15, 0x94d049bb133111ebULL, 53, 33);

            if (state & 1) {
                a3 ^= rotl64(a3, 11);
                a7 += 0x27d4eb2f165667c5ULL;
            } else {
                a3 += 0x85ebca77c2b2ae63ULL;
                a7 ^= a7 >> 15;
            }
            if ((state >> 7) & 1) {
                a5 += rotl64(a5, 9);
                a11 ^= 0xd6e8feb86659fd93ULL;
            } else {
                a5 ^= a5 >> 17;
                a11 += 0x9e3779b97f4a7c15ULL;
            }
            if ((state >> 19) & 1) {
                a1 ^= rotl64(a1, 21);
                a13 += 0xbf58476d1ce4e5b9ULL;
            } else {
                a1 += 0x94d049bb133111ebULL;
                a13 ^= a13 >> 13;
            }
            if ((state >> 41) & 1) {
                a9 += rotl64(a9, 25);
                a15 ^= 0xa0761d6478bd642fULL;
            } else {
                a9 ^= a9 >> 19;
                a15 += 0xe7037ed1a0b428dbULL;
            }
            ops += OPS_PER_LOOP;
        }
    } while (now_sec() < end);

    const uint64_t checksum = a0 ^ a1 ^ a2 ^ a3 ^ a4 ^ a5 ^ a6 ^ a7 ^
                              a8 ^ a9 ^ a10 ^ a11 ^ a12 ^ a13 ^ a14 ^ a15 ^ state;
    fprintf(stderr, "Final checksum: %" PRIu64 "\n", checksum);
    printf("Operations performed: %" PRIu64 "\n", ops);
    return 0;
}
