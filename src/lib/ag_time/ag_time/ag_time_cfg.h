/**
 * @file ag_time_cfg.h
 * @brief Platform detection and feature flags for ag_time library.
 *
 * This header auto-detects the target platform and available C library
 * features. All flags can be overridden by the user before including
 * ag_time.h (e.g. via compiler -D flags or a project-level config header).
 *
 * Supported platforms:
 *   - AG_TIME_PLATFORM_WINDOWS : Win32 (MSVC, MinGW)
 *   - AG_TIME_PLATFORM_LINUX   : Linux (glibc)
 *   - AG_TIME_PLATFORM_NEWLIB  : Newlib (embedded GCC toolchains)
 *   - AG_TIME_PLATFORM_GENERIC : Anything else (pure ISO C fallback)
 */

#ifndef AG_TIME_CFG_H
#define AG_TIME_CFG_H

/* =========================================================================
 * 1) Platform detection
 * ========================================================================= */

/*
 * __NEWLIB__ is defined by Newlib's own headers (e.g. <sys/features.h>),
 * NOT by the compiler.  We must include a lightweight Newlib header before
 * testing for it.  <sys/cdefs.h> or <_newlib_version.h> are good candidates;
 * we try both for maximum compatibility across Newlib versions.
 *
 * Alternatively, the user can bypass auto-detection entirely by defining
 * AG_TIME_PLATFORM_NEWLIB via a compiler flag (e.g. -DAG_TIME_PLATFORM_NEWLIB
 * in CMake's target_compile_definitions).
 */
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__linux__)
#if __has_include(<_newlib_version.h>)
#include <_newlib_version.h> /* Newlib >= 3.x */
#elif __has_include(<sys/features.h>)
#include <sys/features.h> /* Newlib 2.x and older */
#endif
#endif

#if !defined(AG_TIME_PLATFORM_WINDOWS) && !defined(AG_TIME_PLATFORM_LINUX) && !defined(AG_TIME_PLATFORM_NEWLIB) && \
	!defined(AG_TIME_PLATFORM_GENERIC)

#if defined(_WIN32) || defined(_WIN64)
#define AG_TIME_PLATFORM_WINDOWS
#elif defined(__linux__)
#define AG_TIME_PLATFORM_LINUX
#elif defined(__NEWLIB__)
#define AG_TIME_PLATFORM_NEWLIB
#else
#define AG_TIME_PLATFORM_GENERIC
#endif

#endif /* platform auto-detection */

/* =========================================================================
 * 2) Feature flags (derived from platform, user-overridable)
 * ========================================================================= */

/*
 * AG_TIME_HAS_GMTIME_R
 *   gmtime_r() / localtime_r()  — POSIX re-entrant variants.
 *   Present on Linux (glibc) and Newlib; absent on MSVC.
 *   MinGW: may or may not expose them depending on runtime; we default to
 *   "no" on _WIN32 and verify at build time.
 */
#ifndef AG_TIME_HAS_GMTIME_R
#if defined(AG_TIME_PLATFORM_LINUX) || defined(AG_TIME_PLATFORM_NEWLIB)
#define AG_TIME_HAS_GMTIME_R 1
#else
#define AG_TIME_HAS_GMTIME_R 0
#endif
#endif

/*
 * AG_TIME_HAS_GMTIME_S
 *   gmtime_s() / localtime_s()  — MSVC "secure" variants.
 *   NOTE: MSVC signature is (struct tm *result, const time_t *timer),
 *   i.e. parameters are REVERSED compared to the C11 Annex K version.
 */
#ifndef AG_TIME_HAS_GMTIME_S
#if defined(AG_TIME_PLATFORM_WINDOWS)
#define AG_TIME_HAS_GMTIME_S 1
#else
#define AG_TIME_HAS_GMTIME_S 0
#endif
#endif

/*
 * AG_TIME_HAS_TIMEGM
 *   timegm()  — BSD/GNU extension: struct tm (UTC) -> time_t.
 */
#ifndef AG_TIME_HAS_TIMEGM
#if defined(AG_TIME_PLATFORM_LINUX)
#define AG_TIME_HAS_TIMEGM 1
#else
#define AG_TIME_HAS_TIMEGM 0
#endif
#endif

/*
 * AG_TIME_HAS_MKGMTIME
 *   _mkgmtime()  — MSVC equivalent of timegm().
 */
#ifndef AG_TIME_HAS_MKGMTIME
#if defined(AG_TIME_PLATFORM_WINDOWS)
#define AG_TIME_HAS_MKGMTIME 1
#else
#define AG_TIME_HAS_MKGMTIME 0
#endif
#endif

/*
 * AG_TIME_HAS_STRPTIME
 *   strptime()  — XSI / Single UNIX parsing function.
 */
#ifndef AG_TIME_HAS_STRPTIME
#if defined(AG_TIME_PLATFORM_LINUX) || defined(AG_TIME_PLATFORM_NEWLIB)
#define AG_TIME_HAS_STRPTIME 1
#else
#define AG_TIME_HAS_STRPTIME 0
#endif
#endif

/*
 * AG_TIME_HAS_CLOCK_GETTIME
 *   clock_gettime() — POSIX.1-2001 high-resolution clock.
 *   Available on Linux natively.
 *   Newlib: only if _POSIX_TIMERS is defined (e.g. via FreeRTOS-POSIX layer).
 *   Windows: not available.
 */
#ifndef AG_TIME_HAS_CLOCK_GETTIME
#if defined(AG_TIME_PLATFORM_LINUX)
#define AG_TIME_HAS_CLOCK_GETTIME 1
#elif defined(AG_TIME_PLATFORM_NEWLIB) && defined(_POSIX_TIMERS)
#define AG_TIME_HAS_CLOCK_GETTIME 1
#else
#define AG_TIME_HAS_CLOCK_GETTIME 0
#endif
#endif

/*
 * AG_TIME_HAS_GETTIMEOFDAY
 *   gettimeofday() — POSIX (now obsolete, but universally available).
 *   On Newlib it maps to the BSP syscall _gettimeofday() which the user
 *   must implement.  Resolution depends on the BSP (tv_usec may be 0).
 *   Linux: always available.
 *   Windows: not available (we use GetSystemTimeAsFileTime instead).
 */
#ifndef AG_TIME_HAS_GETTIMEOFDAY
#if defined(AG_TIME_PLATFORM_LINUX) || defined(AG_TIME_PLATFORM_NEWLIB)
#define AG_TIME_HAS_GETTIMEOFDAY 1
#else
#define AG_TIME_HAS_GETTIMEOFDAY 0
#endif
#endif

#endif /* AG_TIME_CFG_H */
