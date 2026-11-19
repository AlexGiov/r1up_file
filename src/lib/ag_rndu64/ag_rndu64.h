/**
 * @file ag_rndu64.h
 * @brief 64-bit PRNG (xoshiro256++) with API for all types up to 64-bit
 * @version 1.0
 * @author Alessandro Giovannini (aka AlexGiov)
 * @date 2025
 * 
 * @details 
 * This header-only library was developed with the help of ChatGPT, here the link to the conversation:
 *  https://chatgpt.com/share/68cefca5-b390-8005-96a1-1718408fe26d
 * 
 * High-performance pseudo-random number generator library based on the xoshiro256++ algorithm.
 * Provides unbiased random number generation for all integer types up to 64-bit with excellent
 * statistical properties and very fast execution.
 * 
 * Features:
 * - xoshiro256++ core algorithm (256-bit state, 64-bit output)
 * - SplitMix64 seeding for high-quality initialization
 * - Lemire's unbiased bounded random generation
 * - Type-generic macros using C11 _Generic
 * - Full range and bounded APIs for all integer types
 * - Optimized implementations for different platforms
 * 
 * Requirements: C11 compiler. Not cryptographically secure.
 * Thread-safety: State must be managed by user (not thread-safe by default).
 * 
 * @par Algorithm References:
 * - xoshiro256++ by David Blackman and Sebastiano Vigna
 * - SplitMix64 by Guy L. Steele Jr., Doug Lea, and Christine H. Flood
 * - Fast Random Integer Generation in an Interval by Daniel Lemire
 * 
 * @par Example Usage:
 * @code
 * #include "ag_rndu64.h"
 * 
 * rng64_t rng;
 * rng64_seed(&rng, 12345);  // Initialize with seed
 * 
 * uint32_t random_u32 = rand_u32(&rng);           // Full range [0, 2^32-1]
 * uint32_t bounded = rand_u32_bounded(&rng, 100); // Range [0, 100)
 * int32_t range = rand_i32_closed(&rng, -10, 10); // Range [-10, 10]
 * 
 * // Type-generic macros
 * uint16_t val = RAND_FOR_TYPE(&rng, uint16_t);
 * @endcode
 * 
 * @par Other Example Usage by chatGPT:
 * @code
 * #include "rng_all_64.h"
 * #include <stdio.h>
 *
 * int main(void){
 *     rng64_t rng;
 *     rng64_seed(&rng, 123456789ULL);
 *
 *     uint8_t  a = RAND_FOR_TYPE(&rng, uint8_t);
 *     int32_t  b = RAND_FOR_TYPE(&rng, int32_t);
 *     uint64_t c = RAND_FOR_TYPE(&rng, uint64_t);
 *
 *     uint16_t u = RAND_BOUNDED_UNSIGNED(&rng, 1000u, uint16_t); // [0,1000)
 *     int64_t  s = RAND_CLOSED_SIGNED(&rng, -10, 10, int64_t);   // [-10,10]
 *
 *     printf("a=%u b=%d c=%llu u=%u s=%lld\n",
 *            (unsigned)a, b, (unsigned long long)c,
 *            (unsigned)u, (long long)s);
 *     return 0;
 * }
 * @endcode
 */

#ifndef AG_RNDU64_H
#define AG_RNDU64_H

#include <stdint.h>
#include <limits.h>

/**
 * @defgroup xoshiro256 xoshiro256++ Algorithm
 * @brief High-quality 64-bit PRNG based on xoshiro256++ algorithm
 * @{
 */

/**
 * @struct rng64_t
 * @brief State structure for the xoshiro256++ random number generator
 * 
 * @details This structure holds the internal state for the xoshiro256++ algorithm.
 * The state consists of 4 64-bit values that are updated with each call to rng64_next().
 */
typedef struct { 
    uint64_t s[4]; /**< Internal state array (256 bits total) */
} rng64_t;

/**
 * @brief Rotate left operation for 64-bit values
 * @param x Value to rotate
 * @param k Number of bits to rotate left
 * @return Rotated value
 */
static inline uint64_t rotl64(uint64_t x, int k){
    return (x << k) | (x >> (64 - k));
}

/**
 * @brief SplitMix64 algorithm for seeding
 * @param x Pointer to the seed value (will be modified)
 * @return Next 64-bit pseudo-random value
 * 
 * @details This function implements the SplitMix64 algorithm, used to generate
 * high-quality initial state values from a single seed. The seed is modified
 * in place with each call.
 */
static inline uint64_t splitmix64(uint64_t *x){
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/**
 * @brief Initialize the RNG state with a seed
 * @param r Pointer to the RNG state structure
 * @param seed Seed value (seed != 0 recommended, but 0 works too)
 * 
 * @details Initializes the 4-element state array using SplitMix64 to ensure
 * good distribution even from poor seed values. For truly random behavior,
 * provide a seed based on system time or entropy source.
 * 
 * @note This function provides deterministic seeding. For non-deterministic
 * random numbers, use time/system entropy to generate the seed value.
 */
static inline void rng64_seed(rng64_t *r, uint64_t seed){
    /* Note: deterministic seed. For random seed, use system time/entropy outside this function. */
    for (int i=0; i<4; ++i) r->s[i] = splitmix64(&seed);
}

/**
 * @brief Generate next 64-bit random value
 * @param r Pointer to the RNG state structure
 * @return 64-bit pseudo-random value
 * 
 * @details Implements the xoshiro256++ algorithm to generate the next random value.
 * Updates the internal state and returns a high-quality 64-bit random number.
 * This is the core function that all other random generation functions use.
 */
static inline uint64_t rng64_next(rng64_t *r){
    const uint64_t result = rotl64(r->s[0] + r->s[3], 23) + r->s[0];
    const uint64_t t = r->s[1] << 17;

    r->s[2] ^= r->s[0];
    r->s[3] ^= r->s[1];
    r->s[1] ^= r->s[2];
    r->s[0] ^= r->s[3];

    r->s[2] ^= t;
    r->s[3] = rotl64(r->s[3], 45);

    return result;
}

/** @} */ // end of xoshiro256 group

/**
 * @defgroup bounded Bounded Random Generation
 * @brief Unbiased bounded random number generation using Lemire's algorithm
 * @{
 */

/**
 * @brief Generate unbiased uniform random number in range [0, bound)
 * @param r Pointer to the RNG state structure
 * @param bound Upper bound (exclusive), must be > 0
 * @return Random value uniformly distributed in [0, bound)
 * 
 * @details Uses Lemire's fast unbiased bounded random algorithm.
 * Utilizes 128-bit arithmetic when available for maximum efficiency.
 * Falls back to classic rejection sampling on platforms without 128-bit support.
 * 
 * @warning If bound is 0, returns 0
 */
static inline uint64_t rng64_bounded(rng64_t *r, uint64_t bound){
    if (bound == 0) return 0;
#if defined(__SIZEOF_INT128__)
    for(;;){
        uint64_t x = rng64_next(r);
        __uint128_t m = (__uint128_t)x * bound;
        uint64_t l = (uint64_t)m;
        uint64_t t = (-bound) % bound;    /* threshold */
        if (l >= t) return (uint64_t)(m >> 64);
    }
#elif defined(_MSC_VER) && defined(_M_X64)
    /* MSVC on x64: use _umul128 */
    for(;;){
        uint64_t x = rng64_next(r), hi, lo;
        lo = _umul128(x, bound, &hi);
        uint64_t t = (-bound) % bound;
        if (lo >= t) return hi;
    }
#else
    /* Fallback: classic rejection (unbiased, slightly less efficient) */
    uint64_t lim = UINT64_MAX - (UINT64_MAX % bound);
    uint64_t x;
    do { x = rng64_next(r); } while (x > lim);
    return x % bound;
#endif
}

/** @} */ // end of bounded group

/**
 * @defgroup full_type_api Full Type Range API
 * @brief Functions that return random values using the full range of each type
 * @details Uniform distribution over full integer type T: casting from 64 bits.
 * For smaller/32-bit types, the cast preserves uniformity.
 * @{
 */

/**
 * @brief Generate random 8-bit unsigned integer using full range
 * @param r Pointer to the RNG state
 * @return Random uint8_t value [0, 255]
 */
static inline uint8_t  rand_u8 (rng64_t *r){ return (uint8_t)  rng64_next(r); }

/**
 * @brief Generate random 8-bit signed integer using full range  
 * @param r Pointer to the RNG state
 * @return Random int8_t value [-128, 127]
 */
static inline int8_t   rand_i8 (rng64_t *r){ return (int8_t)   rng64_next(r); }

/**
 * @brief Generate random 16-bit unsigned integer using full range
 * @param r Pointer to the RNG state  
 * @return Random uint16_t value [0, 65535]
 */
static inline uint16_t rand_u16(rng64_t *r){ return (uint16_t) rng64_next(r); }

/**
 * @brief Generate random 16-bit signed integer using full range
 * @param r Pointer to the RNG state
 * @return Random int16_t value [-32768, 32767]
 */
static inline int16_t  rand_i16(rng64_t *r){ return (int16_t)  rng64_next(r); }

/**
 * @brief Generate random 32-bit unsigned integer using full range
 * @param r Pointer to the RNG state
 * @return Random uint32_t value [0, 4294967295]
 */
static inline uint32_t rand_u32(rng64_t *r){ return (uint32_t) rng64_next(r); }

/**
 * @brief Generate random 32-bit signed integer using full range
 * @param r Pointer to the RNG state  
 * @return Random int32_t value [-2147483648, 2147483647]
 */
static inline int32_t  rand_i32(rng64_t *r){ return (int32_t)  rng64_next(r); }

/**
 * @brief Generate random 64-bit unsigned integer using full range
 * @param r Pointer to the RNG state
 * @return Random uint64_t value [0, 18446744073709551615]
 */
static inline uint64_t rand_u64(rng64_t *r){ return            rng64_next(r); }

/**
 * @brief Generate random 64-bit signed integer using full range
 * @param r Pointer to the RNG state
 * @return Random int64_t value [-9223372036854775808, 9223372036854775807]
 */
static inline int64_t  rand_i64(rng64_t *r){ return (int64_t)  rng64_next(r); }

/** @} */ /* end of full_type_api group */

/**
 * @defgroup bounded_api Bounded Range API
 * @brief Unbiased random number generation within specified ranges
 * @details Functions that generate random numbers within bounded ranges without bias.
 * Unsigned functions generate values uniformly distributed in [0, n).
 * @{
 */

/**
 * @brief Generate bounded random 8-bit unsigned integer
 * @param r Pointer to the RNG state
 * @param n Upper bound (exclusive), range [0, n)
 * @return Random uint8_t value in [0, n)
 */
static inline uint8_t  rand_u8_bounded (rng64_t *r, uint8_t  n){ return (uint8_t)  rng64_bounded(r, (uint64_t)n); }

/**
 * @brief Generate bounded random 16-bit unsigned integer
 * @param r Pointer to the RNG state
 * @param n Upper bound (exclusive), range [0, n)
 * @return Random uint16_t value in [0, n)
 */
static inline uint16_t rand_u16_bounded(rng64_t *r, uint16_t n){ return (uint16_t) rng64_bounded(r, (uint64_t)n); }

/**
 * @brief Generate bounded random 32-bit unsigned integer
 * @param r Pointer to the RNG state
 * @param n Upper bound (exclusive), range [0, n)
 * @return Random uint32_t value in [0, n)
 */
static inline uint32_t rand_u32_bounded(rng64_t *r, uint32_t n){ return (uint32_t) rng64_bounded(r, (uint64_t)n); }

/**
 * @brief Generate bounded random 64-bit unsigned integer
 * @param r Pointer to the RNG state
 * @param n Upper bound (exclusive), range [0, n)
 * @return Random uint64_t value in [0, n)
 */
static inline uint64_t rand_u64_bounded(rng64_t *r, uint64_t n){ return            rng64_bounded(r, n); }

/**
 * @brief Generate random signed integer in closed range [a, b]
 * @details Calculate width in 128 bits to avoid overflow, then apply Lemire's algorithm.
 * Both bounds are inclusive (a <= b required).
 */

/**
 * @brief Generate random 8-bit signed integer in closed range [a, b]
 * @param r Pointer to the RNG state
 * @param a Lower bound (inclusive)
 * @param b Upper bound (inclusive)
 * @return Random int8_t value in [a, b]
 */
static inline int8_t  rand_i8_closed (rng64_t *r, int8_t  a, int8_t  b){
#if defined(__SIZEOF_INT128__)
    __int128 range = (__int128)b - (__int128)a + 1;
    return (int8_t)(a + (int8_t)rng64_bounded(r, (uint64_t)range));
#else
    /* safe version without __int128: promote to int32_t */
    int32_t range = (int32_t)b - (int32_t)a + 1;
    return (int8_t)(a + (int8_t)rng64_bounded(r, (uint64_t)(uint32_t)range));
#endif
}

/**
 * @brief Generate random 16-bit signed integer in closed range [a, b]
 * @param r Pointer to the RNG state
 * @param a Lower bound (inclusive)
 * @param b Upper bound (inclusive)
 * @return Random int16_t value in [a, b]
 */
static inline int16_t rand_i16_closed(rng64_t *r, int16_t a, int16_t b){
#if defined(__SIZEOF_INT128__)
    __int128 range = (__int128)b - (__int128)a + 1;
    return (int16_t)(a + (int16_t)rng64_bounded(r, (uint64_t)range));
#else
    int32_t range = (int32_t)b - (int32_t)a + 1;
    return (int16_t)(a + (int16_t)rng64_bounded(r, (uint64_t)(uint32_t)range));
#endif
}

/**
 * @brief Generate random 32-bit signed integer in closed range [a, b]
 * @param r Pointer to the RNG state
 * @param a Lower bound (inclusive)
 * @param b Upper bound (inclusive)
 * @return Random int32_t value in [a, b]
 */
static inline int32_t rand_i32_closed(rng64_t *r, int32_t a, int32_t b){
#if defined(__SIZEOF_INT128__)
    __int128 range = (__int128)b - (__int128)a + 1;
    return (int32_t)(a + (int32_t)rng64_bounded(r, (uint64_t)range));
#else
    /* Warning: range can be 2^32; cast to uint64_t is ok. */
    uint64_t range = (uint64_t)((uint32_t)(b - a) + 1u);
    return (int32_t)(a + (int32_t)rng64_bounded(r, range));
#endif
}

/**
 * @brief Generate random 64-bit signed integer in closed range [a, b]
 * @param r Pointer to the RNG state
 * @param a Lower bound (inclusive)
 * @param b Upper bound (inclusive)
 * @return Random int64_t value in [a, b]
 */
static inline int64_t rand_i64_closed(rng64_t *r, int64_t a, int64_t b){
#if defined(__SIZEOF_INT128__)
    __int128 range = (__int128)b - (__int128)a + 1;
    return a + (int64_t)rng64_bounded(r, (uint64_t)range);
#elif defined(_MSC_VER) && defined(_M_X64)
    /* Without __int128 on MSVC: if b==INT64_MAX and a==INT64_MIN range would overflow in 64 bits.
       Here we handle the full case separately. */
    if (a == INT64_MIN && b == INT64_MAX){
        /* range = 2^64 -> just return a full cast */
        return (int64_t)rng64_next(r);
    } else {
        uint64_t range = (uint64_t)((__int64)b - (__int64)a) + 1u;
        return a + (int64_t)rng64_bounded(r, range);
    }
#else
    /* Conservative fallback: avoid overflow for full range */
    if (a == INT64_MIN && b == INT64_MAX) {
        return (int64_t)rng64_next(r);
    } else {
        uint64_t range = (uint64_t)((int64_t)(b - a)) + 1u;
        return a + (int64_t)rng64_bounded(r, range);
    }
#endif
}

/** @} */ /* end of bounded_api group */

/**
 * @defgroup generic_macros Type-Generic Macros
 * @brief Convenient type-generic macros for easy API usage
 * @details C11 _Generic macros for type-safe random number generation without explicit type specification.
 * These macros provide a convenient interface that automatically selects the appropriate function
 * based on the target type.
 * @{
 */

/**
 * @brief Type-generic macro for generating random values using full type range
 * @param state Pointer to the RNG state
 * @param type The desired integer type (uint8_t, int8_t, uint16_t, etc.)
 * @return Random value of the specified type using its full range
 * @details Uses _Generic to select the appropriate rand_* function based on type.
 * Falls back to casting rng64_next() for unsupported types.
 */
#define RAND_FOR_TYPE(state, type) \
    _Generic(((type)0), \
        uint8_t:  rand_u8(state),  int8_t:  rand_i8(state), \
        uint16_t: rand_u16(state), int16_t: rand_i16(state), \
        uint32_t: rand_u32(state), int32_t: rand_i32(state), \
        uint64_t: rand_u64(state), int64_t: rand_i64(state), \
        default:  (type)rng64_next((state)) /* fallback: use low 64 bits */ \
    )

/**
 * @brief Type-generic macro for generating bounded random values for unsigned types
 * @param state Pointer to the RNG state
 * @param bound Upper bound (exclusive) - must be > 0
 * @param type The desired unsigned integer type (uint8_t, uint16_t, etc.)
 * @return Random value in range [0, bound) of the specified type
 * @details Uses _Generic to select the appropriate bounded function for unsigned types.
 * Falls back to rng64_bounded() for unsupported types.
 */
#define RAND_BOUNDED_UNSIGNED(state, bound, type) \
    _Generic(((type)0), \
        uint8_t:  (type)rand_u8_bounded (state, (uint8_t)(bound)),  \
        uint16_t: (type)rand_u16_bounded(state, (uint16_t)(bound)), \
        uint32_t: (type)rand_u32_bounded(state, (uint32_t)(bound)), \
        uint64_t: (type)rand_u64_bounded(state, (uint64_t)(bound)), \
        default:  (type)rng64_bounded((state), (uint64_t)(bound)) \
    )

/**
 * @brief Type-generic macro for generating closed-range random values for signed types
 * @param state Pointer to the RNG state
 * @param a Lower bound (inclusive)
 * @param b Upper bound (inclusive)
 * @param type The desired signed integer type (int8_t, int16_t, etc.)
 * @return Random value in range [a, b] of the specified type
 * @details Uses _Generic to select the appropriate closed-range function for signed types.
 * Both bounds are inclusive. Requires a <= b.
 */
#define RAND_CLOSED_SIGNED(state, a, b, type) \
    _Generic(((type)0), \
        int8_t:   (type)rand_i8_closed (state, (int8_t)(a),  (int8_t)(b)),  \
        int16_t:  (type)rand_i16_closed(state, (int16_t)(a), (int16_t)(b)), \
        int32_t:  (type)rand_i32_closed(state, (int32_t)(a), (int32_t)(b)), \
        int64_t:  (type)rand_i64_closed(state, (int64_t)(a), (int64_t)(b))  \
    )

/** @} */ /* end of generic_macros group */

#endif /* RNG_ALL_64_H */
