// --------------------------------------------------------------------------------------------------------------------
/// \file ag_string.h
///
/// \brief String parsing utilities optimized for embedded systems.
///
/// \details Provides safe, efficient string-to-integer conversion functions without relying on
/// standard library functions like atoi() or strtoul(). These functions are designed for embedded
/// systems with the following characteristics:
/// - No heap allocations
/// - No division operations (only multiplications by 10)
/// - Explicit validation of input characters
/// - Clear error reporting via return value
/// - Overflow detection for target types
/// - Works with fixed-length numeric strings (common in protocols and file formats)
///
/// Copyright (c) 2026, ASK Industries GmbH, Niederwinkling.
/// All Rights Reserved.
// --------------------------------------------------------------------------------------------------------------------

#ifndef AG_STRING_H
#define AG_STRING_H

// --------------------------------------------------------------------------------------------------------------------
//	Includes
// --------------------------------------------------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

// --------------------------------------------------------------------------------------------------------------------
//	Definitions
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Maximum supported number of digits for parsing
 * @details This limit prevents potential overflow issues and ensures predictable performance
 */
#define AG_STRING_MAX_DIGITS 10U

// --------------------------------------------------------------------------------------------------------------------
//	Type Definitions
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Function Prototypes
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
/// \brief Parse exactly n digits from a string into a uint8_t value.
///
/// \details Converts exactly n consecutive ASCII digit characters ('0'-'9') starting from the
/// given string position into an unsigned 8-bit integer. The function validates each character
/// and checks for overflow before writing the result.
///
/// Example usage:
/// \code
/// uint8_t value;
/// if (ag_string_parse_uint8_n("42", 2, &value)) {
///     // value == 42
/// }
/// \endcode
///
/// \param[in] str Pointer to string starting with digit characters (must not be NULL)
/// \param[in] n_digits Number of digits to parse (1-3 for uint8_t, max value 255)
/// \param[out] result Pointer to store parsed value (written only on success)
///
/// \return true if parsing succeeded, false if:
///         - str is NULL
///         - result is NULL
///         - n_digits is 0 or > AG_STRING_MAX_DIGITS
///         - Any character in range [0, n_digits) is not an ASCII digit
///         - Resulting value would overflow uint8_t (> 255)
// --------------------------------------------------------------------------------------------------------------------
bool ag_string_parse_uint8_n(const char* str, uint8_t n_digits, uint8_t* result);

// --------------------------------------------------------------------------------------------------------------------
/// \brief Parse exactly n digits from a string into a uint16_t value.
///
/// \details Converts exactly n consecutive ASCII digit characters ('0'-'9') starting from the
/// given string position into an unsigned 16-bit integer. The function validates each character
/// and checks for overflow before writing the result.
///
/// Example usage:
/// \code
/// uint16_t year;
/// if (ag_string_parse_uint16_n("2026", 4, &year)) {
///     // year == 2026
/// }
/// \endcode
///
/// \param[in] str Pointer to string starting with digit characters (must not be NULL)
/// \param[in] n_digits Number of digits to parse (1-5 for uint16_t, max value 65535)
/// \param[out] result Pointer to store parsed value (written only on success)
///
/// \return true if parsing succeeded, false if:
///         - str is NULL
///         - result is NULL
///         - n_digits is 0 or > AG_STRING_MAX_DIGITS
///         - Any character in range [0, n_digits) is not an ASCII digit
///         - Resulting value would overflow uint16_t (> 65535)
// --------------------------------------------------------------------------------------------------------------------
bool ag_string_parse_uint16_n(const char* str, uint8_t n_digits, uint16_t* result);

// --------------------------------------------------------------------------------------------------------------------
/// \brief Parse exactly n digits from a string into a uint32_t value.
///
/// \details Converts exactly n consecutive ASCII digit characters ('0'-'9') starting from the
/// given string position into an unsigned 32-bit integer. The function validates each character
/// and checks for overflow before writing the result.
///
/// Example usage:
/// \code
/// uint32_t timestamp;
/// if (ag_string_parse_uint32_n("1234567890", 10, &timestamp)) {
///     // timestamp == 1234567890
/// }
/// \endcode
///
/// \param[in] str Pointer to string starting with digit characters (must not be NULL)
/// \param[in] n_digits Number of digits to parse (1-10 for uint32_t, max value 4294967295)
/// \param[out] result Pointer to store parsed value (written only on success)
///
/// \return true if parsing succeeded, false if:
///         - str is NULL
///         - result is NULL
///         - n_digits is 0 or > AG_STRING_MAX_DIGITS
///         - Any character in range [0, n_digits) is not an ASCII digit
///         - Resulting value would overflow uint32_t (> 4294967295)
// --------------------------------------------------------------------------------------------------------------------
bool ag_string_parse_uint32_n(const char* str, uint8_t n_digits, uint32_t* result);

// --------------------------------------------------------------------------------------------------------------------
/// \brief Check if a character is an ASCII decimal digit ('0'-'9').
///
/// \details Fast inline check for digit validation. This is more efficient than isdigit() from
/// ctype.h and doesn't depend on locale settings.
///
/// \param[in] c Character to check
///
/// \return true if c is '0'-'9', false otherwise
// --------------------------------------------------------------------------------------------------------------------
static inline bool ag_string_is_digit(char c) { return (c >= '0') && (c <= '9'); }

// --------------------------------------------------------------------------------------------------------------------
/// \brief Convert ASCII digit character to its numeric value.
///
/// \details Converts '0'-'9' to 0-9. Behavior is undefined if the character is not a digit.
/// Use ag_string_is_digit() to validate first.
///
/// \param[in] c ASCII digit character ('0'-'9')
///
/// \return Numeric value (0-9)
///
/// \warning This function does NOT validate input. Calling with non-digit characters produces
///          undefined results. Always validate with ag_string_is_digit() first.
// --------------------------------------------------------------------------------------------------------------------
static inline uint8_t ag_string_digit_to_value(char c) { return (uint8_t)(c - '0'); }

#endif	// AG_STRING_H
