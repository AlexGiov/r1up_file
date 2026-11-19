/**
 * @file ag_time.h
 * @brief Portable, thread-safe wrapper around <time.h>.
 *
 * ag_time provides a uniform API that works on:
 *   - Embedded systems with Newlib (FreeRTOS, bare-metal)
 *   - Linux (glibc)
 *   - Windows (MSVC / MinGW)
 *
 * Design principles:
 *   - All conversion functions use caller-supplied buffers (re-entrant).
 *   - No static storage is ever exposed.
 *   - Time source is injectable for deterministic unit testing.
 *   - Timezone is a simple UTC offset set by the user (no DST database).
 *
 * Usage: include "ag_time/ag_time.h" instead of <time.h>.
 */

#ifndef AG_TIME_H
#define AG_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Dependencies
 * ========================================================================= */

#include <stddef.h> /* size_t  */
#include <stdint.h> /* int32_t, uint32_t */
#include <time.h>	/* time_t, struct tm */

#include "ag_time_cfg.h"

/* =========================================================================
 * Error codes
 * ========================================================================= */

/**
 * @brief Return codes for ag_time functions.
 */
typedef enum {
	AG_TIME_OK = 0,				   /**< Success                                */
	AG_TIME_ERR_NULL_PTR = -1,	   /**< A required pointer argument was NULL   */
	AG_TIME_ERR_INVALID = -2,	   /**< time_t value not representable         */
	AG_TIME_ERR_OVERFLOW = -3,	   /**< Arithmetic overflow during conversion  */
	AG_TIME_ERR_NOT_SUPPORTED = -4 /**< Function not available on platform  */
} ag_time_err_t;

/* =========================================================================
 * Injectable time source
 * ========================================================================= */

/**
 * @brief Signature for a custom time source callback.
 *
 * Must behave like time(): return calendar time and, if @p timer is not NULL,
 * also store it in *timer.
 */
typedef time_t (*ag_time_source_fn)(time_t* timer);

/**
 * @brief Install a custom time source (or NULL to restore the default).
 *
 * @warning Call only during initialisation or between test cases.
 *          Not safe to call concurrently with ag_time_time().
 */
void ag_time_set_time_source(ag_time_source_fn source);

/* =========================================================================
 * Timezone configuration
 * ========================================================================= */

/**
 * @brief Set the UTC offset used by ag_time_localtime().
 *
 * @param utc_offset_seconds  Offset from UTC in seconds.
 *        Example: +3600 for CET (UTC+1), -18000 for EST (UTC-5).
 *
 * ag_time_localtime() computes local time as UTC + offset.
 * No automatic DST handling — caller is responsible for adjusting
 * the offset when DST changes.
 *
 * @warning Call only during initialisation.  Not safe to call
 *          concurrently with ag_time_localtime().
 */
void ag_time_tzset(int32_t utc_offset_seconds);

/* =========================================================================
 * Core time functions
 * ========================================================================= */

/**
 * @brief Get the current calendar time.
 *
 * Uses the injected time source if one has been set, otherwise
 * delegates to the standard time() function.
 *
 * @param timer  If not NULL, the result is also stored in *timer.
 * @return Current calendar time, or (time_t)-1 on error.
 */
time_t ag_time_time(time_t* timer);

/**
 * @brief Compute the difference between two calendar times.
 *
 * Thin inline wrapper around difftime().
 */
static inline double ag_time_difftime(time_t time1, time_t time0) { return difftime(time1, time0); }

/**
 * @brief Convert broken-down local time to calendar time.
 *
 * Thin inline wrapper around mktime().
 */
static inline time_t ag_time_mktime(struct tm* timeptr) { return mktime(timeptr); }

/**
 * @brief Format a broken-down time into a string.
 *
 * Thin inline wrapper around strftime().
 */
static inline size_t ag_time_strftime(char* s, size_t maxsize, const char* format, const struct tm* timeptr) {
	return strftime(s, maxsize, format, timeptr);
}

/* =========================================================================
 * Thread-safe conversion functions
 * ========================================================================= */

/**
 * @brief Convert calendar time to broken-down UTC time (thread-safe).
 *
 * @param[in]  timer   Pointer to calendar time.
 * @param[out] result  Caller-supplied struct tm to fill.
 * @return AG_TIME_OK on success, or an error code.
 */
ag_time_err_t ag_time_gmtime(const time_t* timer, struct tm* result);

/**
 * @brief Convert calendar time to broken-down local time (thread-safe).
 *
 * Local time = UTC + offset set by ag_time_tzset().
 *
 * @param[in]  timer   Pointer to calendar time.
 * @param[out] result  Caller-supplied struct tm to fill.
 * @return AG_TIME_OK on success, or an error code.
 */
ag_time_err_t ag_time_localtime(const time_t* timer, struct tm* result);

/**
 * @brief Convert broken-down UTC time to calendar time.
 *
 * Portable replacement for timegm() / _mkgmtime().
 *
 * @param[in]  tp      Pointer to broken-down time interpreted as UTC.
 * @param[out] result  Pointer to store the resulting time_t.
 * @return AG_TIME_OK on success, or an error code.
 */
ag_time_err_t ag_time_mktime_utc(const struct tm* tp, time_t* result);

/* =========================================================================
 * Parsing
 * ========================================================================= */

/**
 * @brief Parse a date/time string according to a format.
 *
 * Simplified, fully portable implementation (no locale support).
 * Uses only character arithmetic — no extra includes required.
 *
 * Supported format specifiers:
 *
 *   %%   — literal '%'
 *   %d   — day of month       (01-31)
 *   %m   — month              (01-12)
 *   %Y   — 4-digit year
 *   %H   — hour, 24-h clock   (00-23)
 *   %M   — minute             (00-59)
 *   %S   — second             (00-60, 60 for leap second)
 *   %f   — fractional seconds (1-9 digits, converted to nanoseconds)
 *
 * Any other character in @p format is treated as a literal that must
 * match the corresponding character in @p s.
 *
 * The %f specifier is an ag_time extension (same letter as Python's
 * strptime).  It parses 1 to 9 digits and writes the result, scaled
 * to nanoseconds, into @p subsec_ns.  If %f is not present in the
 * format string, *subsec_ns is set to 0.
 *
 * Only the struct tm fields corresponding to the specifiers actually
 * present in @p format are written; all other fields are left
 * untouched.  The caller should zero-initialise the struct tm before
 * calling this function if deterministic behaviour is desired.
 *
 * @param[in]  s          String to parse.
 * @param[in]  format     Format string with specifiers listed above.
 * @param[out] tp         Caller-supplied struct tm to fill.
 * @param[out] subsec_ns  Fractional seconds in nanoseconds [0..999999999].
 *                        May be NULL if sub-second precision is not needed.
 * @return AG_TIME_OK on success, AG_TIME_ERR_NULL_PTR if s/format/tp is
 *         NULL, AG_TIME_ERR_INVALID if the input does not match the format.
 */
ag_time_err_t ag_time_strptime(const char* s, const char* format, struct tm* tp, uint32_t* subsec_ns);

/* =========================================================================
 * High-resolution time
 * ========================================================================= */

/**
 * @brief High-resolution time representation (seconds + nanoseconds).
 *
 * Modelled after POSIX struct timespec.  The tv_nsec field is always
 * in the range [0, 999 999 999].
 */
typedef struct {
	time_t tv_sec;	  /**< Seconds since Unix epoch (1970-01-01T00:00:00Z) */
	uint32_t tv_nsec; /**< Nanoseconds [0 .. 999 999 999]                 */
} ag_time_timespec_t;

/**
 * @brief Signature for a custom high-resolution time source callback.
 *
 * The callback must fill @p tp and return 0 on success, non-zero on error.
 * This is the application-level override; on embedded targets the BSP-level
 * override is the _gettimeofday() syscall that Newlib requires.
 */
typedef int (*ag_time_hires_source_fn)(ag_time_timespec_t* tp);

/**
 * @brief Install a custom high-resolution time source (or NULL to restore
 *        the platform default).
 *
 * @warning Call only during initialisation or between test cases.
 *          Not safe to call concurrently with ag_time_clock_gettime().
 */
void ag_time_set_hires_source(ag_time_hires_source_fn source);

/**
 * @brief Get the current time with sub-second resolution.
 *
 * Resolution depends on the platform or injected source:
 *
 *  Source (in priority order)        | Resolution
 *  -------------------------------- | --------------------------
 *  Injected hires_source            | defined by the callback
 *  clock_gettime (POSIX)            | nanoseconds (platform dep.)
 *  gettimeofday (POSIX / Newlib)    | microseconds (BSP dep.)
 *  GetSystemTimeAsFileTime (Win32)  | 100 ns units
 *  Injected time_source / time()    | seconds only (tv_nsec = 0)
 *
 * @param[out] tp  Caller-supplied struct to fill.
 * @return AG_TIME_OK on success, or an error code.
 */
ag_time_err_t ag_time_clock_gettime(ag_time_timespec_t* tp);

/* ---- Inline conversion helpers ----------------------------------------- */

/**
 * @brief Convert ag_time_timespec_t to milliseconds since epoch.
 */
static inline uint64_t ag_time_to_ms(const ag_time_timespec_t* tp) {
	return (uint64_t)tp->tv_sec * 1000ULL + (uint64_t)tp->tv_nsec / 1000000ULL;
}

/**
 * @brief Convert ag_time_timespec_t to microseconds since epoch.
 */
static inline uint64_t ag_time_to_us(const ag_time_timespec_t* tp) {
	return (uint64_t)tp->tv_sec * 1000000ULL + (uint64_t)tp->tv_nsec / 1000ULL;
}

/* =========================================================================
 * Debug / diagnostics
 * ========================================================================= */

/**
 * @brief Print the library configuration to stdout.
 *
 * Shows detected platform, feature flags, compiler info, and current
 * runtime settings (time source, UTC offset).  Useful during bring-up
 * to verify that the auto-detection matches expectations.
 *
 * Output is human-readable, one line per item.  No-op if NDEBUG is
 * defined (release builds).
 */
void ag_time_print_config(void);

#ifdef __cplusplus
}
#endif

#endif /* AG_TIME_H */
