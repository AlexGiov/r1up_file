/**
 * @file ag_time.c
 * @brief Implementation of the ag_time portable time wrapper.
 */

#include "ag_time/ag_time.h"

#include <stdio.h> /* printf (for ag_time_print_config) */

/* =========================================================================
 * Platform-specific includes
 * ========================================================================= */

#if AG_TIME_HAS_CLOCK_GETTIME
/* POSIX clock_gettime — needs <time.h> already included via ag_time.h */
#endif

#if AG_TIME_HAS_GETTIMEOFDAY
#include <sys/time.h> /* struct timeval, gettimeofday() */
#endif

#if AG_TIME_HAS_GMTIME_S
/* MSVC "secure" functions — need <time.h> already included via ag_time.h */
#endif

#if defined(AG_TIME_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> /* GetSystemTimeAsFileTime */
#endif

/* =========================================================================
 * Module-level state
 *
 * These variables are set once at init time.  They are NOT protected by a
 * mutex — the caller must ensure ag_time_set_time_source() and
 * ag_time_tzset() are not called concurrently with the functions that
 * read them.
 * ========================================================================= */

/** Custom time source (NULL = use standard time()). */
static ag_time_source_fn s_time_source = NULL;

/** UTC offset in seconds for ag_time_localtime(). */
static int32_t s_utc_offset_seconds = 0;

/** Custom high-resolution time source (NULL = use platform default). */
static ag_time_hires_source_fn s_hires_source = NULL;

/* =========================================================================
 * Injectable time source
 * ========================================================================= */

void ag_time_set_time_source(ag_time_source_fn source) { s_time_source = source; }

/* =========================================================================
 * Timezone configuration
 * ========================================================================= */

void ag_time_tzset(int32_t utc_offset_seconds) { s_utc_offset_seconds = utc_offset_seconds; }

/* =========================================================================
 * Core: ag_time_time
 * ========================================================================= */

time_t ag_time_time(time_t* timer) {
	if (s_time_source != NULL) {
		return s_time_source(timer);
	}
	return time(timer);
}

/* =========================================================================
 * Thread-safe conversions
 * ========================================================================= */

ag_time_err_t ag_time_gmtime(const time_t* timer, struct tm* result) {
	if (timer == NULL || result == NULL) {
		return AG_TIME_ERR_NULL_PTR;
	}

#if AG_TIME_HAS_GMTIME_R
	/* POSIX / Newlib */
	if (gmtime_r(timer, result) == NULL) {
		return AG_TIME_ERR_INVALID;
	}
#elif AG_TIME_HAS_GMTIME_S
	/* MSVC — note reversed parameter order vs. C11 Annex K */
	if (gmtime_s(result, timer) != 0) {
		return AG_TIME_ERR_INVALID;
	}
#else
/* Fallback: ISO C gmtime() — NOT thread-safe.
 * Acceptable only on single-threaded targets. */
#warning \
	"ag_time: using gmtime() fallback which is NOT thread-safe. " \
	         "Define AG_TIME_HAS_GMTIME_R=1 or AG_TIME_HAS_GMTIME_S=1 if your platform supports them."
	{
		const struct tm* tmp = gmtime(timer);
		if (tmp == NULL) {
			return AG_TIME_ERR_INVALID;
		}
		*result = *tmp;
	}
#endif

	return AG_TIME_OK;
}

/* ------------------------------------------------------------------------- */

ag_time_err_t ag_time_localtime(const time_t* timer, struct tm* result) {
	if (timer == NULL || result == NULL) {
		return AG_TIME_ERR_NULL_PTR;
	}

	/* local_time = UTC + offset */
	time_t local = *timer + (time_t)s_utc_offset_seconds;

	/* Re-use gmtime (UTC) on the offset-adjusted value */
	return ag_time_gmtime(&local, result);
}

/* =========================================================================
 * UTC mktime  (struct tm UTC -> time_t)
 * ========================================================================= */

ag_time_err_t ag_time_mktime_utc(const struct tm* tp, time_t* result) {
	if (tp == NULL || result == NULL) {
		return AG_TIME_ERR_NULL_PTR;
	}

#if AG_TIME_HAS_TIMEGM
	/* Linux / glibc — BSD/GNU extension */
	{
		struct tm tmp = *tp; /* timegm may modify the struct */
		time_t t = timegm(&tmp);
		if (t == (time_t)-1) {
			return AG_TIME_ERR_INVALID;
		}
		*result = t;
	}
#elif AG_TIME_HAS_MKGMTIME
	/* Windows (MSVC / MinGW with MSVCRT) */
	{
		struct tm tmp = *tp;
		time_t t = _mkgmtime(&tmp);
		if (t == (time_t)-1) {
			return AG_TIME_ERR_INVALID;
		}
		*result = t;
	}
#else
	/* Portable fallback — pure arithmetic, no timezone dependency.
	 *
	 * Algorithm: count days from the Unix epoch (1970-01-01) to the
	 * date described by tp, then convert to seconds and add the
	 * time-of-day fields.
	 *
	 * Handles leap years correctly.  Does NOT handle leap seconds
	 * (same as mktime / timegm on virtually all platforms).
	 */
	{
		int year = tp->tm_year + 1900;
		int month = tp->tm_mon + 1; /* 1-12 */
		int day = tp->tm_mday;		/* 1-31 */

		/* Days in each month (non-leap year) */
		static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

		/* --- days from epoch to start of 'year' --- */
		long long total_days = 0;

		if (year >= 1970) {
			for (int y = 1970; y < year; y++) {
				total_days += 365;
				if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) {
					total_days++;
				}
			}
		} else {
			for (int y = year; y < 1970; y++) {
				total_days -= 365;
				if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) {
					total_days--;
				}
			}
		}

		/* --- add days for months elapsed in current year --- */
		int is_leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
		for (int m = 1; m < month; m++) {
			total_days += mdays[m - 1];
			if (m == 2 && is_leap) {
				total_days++;
			}
		}

		/* --- add day-of-month (1-based) --- */
		total_days += (day - 1);

		/* --- convert to seconds --- */
		long long epoch = total_days * 86400LL + (long long)tp->tm_hour * 3600LL + (long long)tp->tm_min * 60LL +
						  (long long)tp->tm_sec;

		*result = (time_t)epoch;
	}
#endif

	return AG_TIME_OK;
}

/* =========================================================================
 * Parsing
 * ========================================================================= */

/**
 * @brief Parse exactly @p n decimal digits from @p s into *value.
 * @return Pointer past the last consumed digit, or NULL on failure.
 */
static const char* parse_digits(const char* s, int n, int* value) {
	int v = 0;
	for (int i = 0; i < n; i++) {
		if (s[i] < '0' || s[i] > '9') {
			return NULL;
		}
		v = v * 10 + (s[i] - '0');
	}
	*value = v;
	return s + n;
}

ag_time_err_t ag_time_strptime(const char* s, const char* format, struct tm* tp, uint32_t* subsec_ns) {
	if (s == NULL || format == NULL || tp == NULL) {
		return AG_TIME_ERR_NULL_PTR;
	}

	/* Default: no fractional seconds parsed */
	int has_frac = 0;
	uint32_t frac_ns = 0;

	const char* sp = s;
	const char* fp = format;

	while (*fp != '\0') {
		if (*fp == '%') {
			fp++; /* skip '%' */
			switch (*fp) {
				case 'd': { /* day of month: 2 digits */
					int val;
					sp = parse_digits(sp, 2, &val);
					if (sp == NULL || val < 1 || val > 31) {
						return AG_TIME_ERR_INVALID;
					}
					tp->tm_mday = val;
					break;
				}
				case 'm': { /* month: 2 digits */
					int val;
					sp = parse_digits(sp, 2, &val);
					if (sp == NULL || val < 1 || val > 12) {
						return AG_TIME_ERR_INVALID;
					}
					tp->tm_mon = val - 1; /* struct tm: 0-11 */
					break;
				}
				case 'Y': { /* 4-digit year */
					int val;
					sp = parse_digits(sp, 4, &val);
					if (sp == NULL) {
						return AG_TIME_ERR_INVALID;
					}
					tp->tm_year = val - 1900; /* struct tm: years since 1900 */
					break;
				}
				case 'H': { /* hour 00-23 */
					int val;
					sp = parse_digits(sp, 2, &val);
					if (sp == NULL || val > 23) {
						return AG_TIME_ERR_INVALID;
					}
					tp->tm_hour = val;
					break;
				}
				case 'M': { /* minute 00-59 */
					int val;
					sp = parse_digits(sp, 2, &val);
					if (sp == NULL || val > 59) {
						return AG_TIME_ERR_INVALID;
					}
					tp->tm_min = val;
					break;
				}
				case 'S': { /* second 00-60 (60 = leap) */
					int val;
					sp = parse_digits(sp, 2, &val);
					if (sp == NULL || val > 60) {
						return AG_TIME_ERR_INVALID;
					}
					tp->tm_sec = val;
					break;
				}
				case 'f': {
					/* Fractional seconds: 1-9 digits, result in nanoseconds.
					 * E.g. "12" (2 digits) → 12 * 10^7 = 120 000 000 ns
					 *      "123456789"      → 123 456 789 ns               */
					if (*sp < '0' || *sp > '9') {
						return AG_TIME_ERR_INVALID; /* at least 1 digit required */
					}
					uint32_t frac = 0;
					int ndigits = 0;
					while (*sp >= '0' && *sp <= '9' && ndigits < 9) {
						frac = frac * 10u + (uint32_t)(*sp - '0');
						sp++;
						ndigits++;
					}
					/* Scale to 9-digit nanoseconds */
					static const uint32_t scale[9] = {
						100000000u, /* 1 digit  → * 10^8 */
						10000000u,	/* 2 digits → * 10^7 */
						1000000u,	/* 3 digits → * 10^6 */
						100000u,	/* 4 digits → * 10^5 */
						10000u,		/* 5 digits → * 10^4 */
						1000u,		/* 6 digits → * 10^3 */
						100u,		/* 7 digits → * 10^2 */
						10u,		/* 8 digits → * 10^1 */
						1u			/* 9 digits → * 10^0 */
					};
					frac_ns = frac * scale[ndigits - 1];
					has_frac = 1;
					break;
				}
				case '%': { /* literal '%' */
					if (*sp != '%') {
						return AG_TIME_ERR_INVALID;
					}
					sp++;
					break;
				}
				default:
					/* Unknown specifier */
					return AG_TIME_ERR_INVALID;
			}
			fp++;
		} else {
			/* Literal character — must match exactly */
			if (*sp != *fp) {
				return AG_TIME_ERR_INVALID;
			}
			sp++;
			fp++;
		}
	}

	/* Reject trailing characters — the entire input must be consumed */
	if (*sp != '\0') {
		return AG_TIME_ERR_INVALID;
	}

	/* Write fractional seconds output */
	if (subsec_ns != NULL) {
		*subsec_ns = has_frac ? frac_ns : 0;
	}

	return AG_TIME_OK;
}

/* =========================================================================
 * High-resolution time
 * ========================================================================= */

void ag_time_set_hires_source(ag_time_hires_source_fn source) { s_hires_source = source; }

ag_time_err_t ag_time_clock_gettime(ag_time_timespec_t* tp) {
	if (tp == NULL) {
		return AG_TIME_ERR_NULL_PTR;
	}

	/* Priority 1: injected high-resolution source */
	if (s_hires_source != NULL) {
		if (s_hires_source(tp) != 0) {
			return AG_TIME_ERR_INVALID;
		}
		return AG_TIME_OK;
	}

	/* Priority 2: platform-specific high-resolution clock */

#if AG_TIME_HAS_CLOCK_GETTIME
	/* POSIX.1-2001: clock_gettime(CLOCK_REALTIME)
	 * On Linux this is native.  On Newlib it is only available if
	 * _POSIX_TIMERS is defined (e.g. when using FreeRTOS-POSIX). */
	{
		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
			return AG_TIME_ERR_INVALID;
		}
		tp->tv_sec = ts.tv_sec;
		tp->tv_nsec = (uint32_t)ts.tv_nsec;
		return AG_TIME_OK;
	}

#elif AG_TIME_HAS_GETTIMEOFDAY
	/* POSIX (obsolete since 2008 but universally available).
	 * On Newlib this calls the BSP syscall _gettimeofday().
	 * Resolution depends on BSP: tv_usec may always be 0. */
	{
		struct timeval tv;
		if (gettimeofday(&tv, NULL) != 0) {
			return AG_TIME_ERR_INVALID;
		}
		tp->tv_sec = tv.tv_sec;
		tp->tv_nsec = (uint32_t)tv.tv_usec * 1000u; /* usec -> nsec */
		return AG_TIME_OK;
	}

#elif defined(AG_TIME_PLATFORM_WINDOWS)
	/* Win32: GetSystemTimeAsFileTime gives 100-ns intervals since
	 * 1601-01-01.  We convert to Unix epoch. */
	{
		FILETIME ft;
		ULARGE_INTEGER uli;

		GetSystemTimeAsFileTime(&ft);
		uli.LowPart = ft.dwLowDateTime;
		uli.HighPart = ft.dwHighDateTime;

		/* FILETIME epoch (1601-01-01) to Unix epoch (1970-01-01)
		 * difference: 11644473600 seconds = 116444736000000000 * 100ns */
		static const uint64_t EPOCH_DIFF = 116444736000000000ULL;
		uint64_t ft100ns = uli.QuadPart - EPOCH_DIFF;

		tp->tv_sec = (time_t)(ft100ns / 10000000ULL);
		tp->tv_nsec = (uint32_t)((ft100ns % 10000000ULL) * 100u);
		return AG_TIME_OK;
	}

#else
	/* Fallback: use the injectable time source or standard time().
	 * Sub-second resolution is not available (tv_nsec = 0). */
	{
		time_t now = ag_time_time(NULL);
		if (now == (time_t)-1) {
			return AG_TIME_ERR_INVALID;
		}
		tp->tv_sec = now;
		tp->tv_nsec = 0;
		return AG_TIME_OK;
	}
#endif
}

/* =========================================================================
 * Debug / diagnostics
 * ========================================================================= */

/* Helper: stringify a macro value as "yes" / "no" */
#define AG_TIME_YESNO(x) ((x) ? "yes" : "no")

void ag_time_print_config(void) {
#ifdef NDEBUG
	/* Silenced in release builds */
	return;
#else
	printf("\n");
	printf("=== ag_time library configuration ===\n");
	printf("\n");

	/* --- Compiler --- */
	printf("[Compiler]\n");
#if defined(__GNUC__) && !defined(__clang__)
	printf("  Compiler       : GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
	printf("  Compiler       : Clang %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
	printf("  Compiler       : MSVC %d\n", _MSC_VER);
#else
	printf("  Compiler       : unknown\n");
#endif

#ifdef __STDC_VERSION__
	printf("  C standard     : %ldL\n", __STDC_VERSION__);
#else
	printf("  C standard     : pre-C99 (no __STDC_VERSION__)\n");
#endif

	printf("  sizeof(time_t) : %u bytes\n", (unsigned)sizeof(time_t));
	printf("\n");

	/* --- Platform detection --- */
	printf("[Platform]\n");
#if defined(AG_TIME_PLATFORM_WINDOWS)
	printf("  Detected       : WINDOWS");
#if defined(__MINGW32__) || defined(__MINGW64__)
	printf(" (MinGW)");
#elif defined(_MSC_VER)
	printf(" (MSVC)");
#endif
	printf("\n");
#elif defined(AG_TIME_PLATFORM_LINUX)
	printf("  Detected       : LINUX\n");
#elif defined(AG_TIME_PLATFORM_NEWLIB)
	printf("  Detected       : NEWLIB (embedded)\n");
#elif defined(AG_TIME_PLATFORM_GENERIC)
	printf("  Detected       : GENERIC (ISO C fallback)\n");
#else
	printf("  Detected       : NONE (should not happen)\n");
#endif
	printf("\n");

	/* --- Feature flags --- */
	printf("[Feature flags]\n");
	printf("  gmtime_r()     : %s  (AG_TIME_HAS_GMTIME_R=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_GMTIME_R),
		   AG_TIME_HAS_GMTIME_R);
	printf("  gmtime_s()     : %s  (AG_TIME_HAS_GMTIME_S=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_GMTIME_S),
		   AG_TIME_HAS_GMTIME_S);
	printf("  timegm()       : %s  (AG_TIME_HAS_TIMEGM=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_TIMEGM), AG_TIME_HAS_TIMEGM);
	printf("  _mkgmtime()    : %s  (AG_TIME_HAS_MKGMTIME=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_MKGMTIME),
		   AG_TIME_HAS_MKGMTIME);
	printf("  strptime()     : %s  (AG_TIME_HAS_STRPTIME=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_STRPTIME),
		   AG_TIME_HAS_STRPTIME);
	printf("  clock_gettime(): %s  (AG_TIME_HAS_CLOCK_GETTIME=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_CLOCK_GETTIME),
		   AG_TIME_HAS_CLOCK_GETTIME);
	printf("  gettimeofday() : %s  (AG_TIME_HAS_GETTIMEOFDAY=%d)\n", AG_TIME_YESNO(AG_TIME_HAS_GETTIMEOFDAY),
		   AG_TIME_HAS_GETTIMEOFDAY);

	/* Thread-safety strategy actually in use */
	printf("\n");
	printf("[Thread-safe gmtime strategy]\n");
#if AG_TIME_HAS_GMTIME_R
	printf("  Using          : gmtime_r()  (POSIX)\n");
#elif AG_TIME_HAS_GMTIME_S
	printf("  Using          : gmtime_s()  (MSVC)\n");
#else
	printf("  Using          : gmtime() + copy  (NOT thread-safe!)\n");
#endif

	/* mktime_utc strategy */
	printf("\n");
	printf("[mktime_utc strategy]\n");
#if AG_TIME_HAS_TIMEGM
	printf("  Using          : timegm()  (BSD/GNU)\n");
#elif AG_TIME_HAS_MKGMTIME
	printf("  Using          : _mkgmtime()  (MSVC)\n");
#else
	printf("  Using          : portable arithmetic  (pure ISO C)\n");
#endif

	/* gettime strategy */
	printf("\n");
	printf("[gettime strategy]\n");
#if AG_TIME_HAS_CLOCK_GETTIME
	printf("  Using          : clock_gettime()  (POSIX)\n");
#elif AG_TIME_HAS_GETTIMEOFDAY
	printf("  Using          : gettimeofday()  (POSIX/BSP)\n");
#elif defined(AG_TIME_PLATFORM_WINDOWS)
	printf("  Using          : GetSystemTimeAsFileTime()  (Win32)\n");
#else
	printf("  Using          : time() fallback  (seconds only)\n");
#endif

	/* --- Runtime state --- */
	printf("\n");
	printf("[Runtime state]\n");
	printf("  Time source    : %s\n", s_time_source != NULL ? "custom (injected)" : "system time()");
	printf("  Hires source   : %s\n", s_hires_source != NULL ? "custom (injected)" : "platform default");
	printf("  UTC offset     : %+d s  (%+.1f h)\n", (int)s_utc_offset_seconds, (double)s_utc_offset_seconds / 3600.0);

	printf("\n");
	printf("=========================================\n");
	printf("\n");
#endif /* NDEBUG */
}
