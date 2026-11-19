// --------------------------------------------------------------------------------------------------------------------
/// \file ag_string.c
///
/// \brief String parsing utilities optimized for embedded systems - implementation.
///
/// Copyright (c) 2026, ASK Industries GmbH, Niederwinkling.
/// All Rights Reserved.
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Includes
// --------------------------------------------------------------------------------------------------------------------

#include "ag_string.h"

// --------------------------------------------------------------------------------------------------------------------
//	Local Definitions
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Local Type Definitions
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Local Function Prototypes
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Local Variables
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Global Variables
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Local Functions
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
//	Global Functions
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
/// \brief Parse exactly n digits from a string into a uint8_t value.
// --------------------------------------------------------------------------------------------------------------------
bool ag_string_parse_uint8_n(const char* str, uint8_t n_digits, uint8_t* result) {
	// Validate input parameters
	if (str == NULL || result == NULL) {
		return false;
	}

	if (n_digits == 0U || n_digits > AG_STRING_MAX_DIGITS) {
		return false;
	}

	// Parse digits with overflow detection
	uint32_t value = 0U;  // Use larger type for intermediate calculations

	for (uint8_t i = 0U; i < n_digits; i++) {
		// Validate that character is an ASCII digit
		if (!ag_string_is_digit(str[i])) {
			return false;
		}

		// Multiply by 10 and add new digit
		value = (value * 10U) + ag_string_digit_to_value(str[i]);

		// Check for overflow (uint8_t max value is 255)
		if (value > 255U) {
			return false;
		}
	}

	// Success - write result
	*result = (uint8_t)value;
	return true;
}

// --------------------------------------------------------------------------------------------------------------------
/// \brief Parse exactly n digits from a string into a uint16_t value.
// --------------------------------------------------------------------------------------------------------------------
bool ag_string_parse_uint16_n(const char* str, uint8_t n_digits, uint16_t* result) {
	// Validate input parameters
	if (str == NULL || result == NULL) {
		return false;
	}

	if (n_digits == 0U || n_digits > AG_STRING_MAX_DIGITS) {
		return false;
	}

	// Parse digits with overflow detection
	uint32_t value = 0U;  // Use larger type for intermediate calculations

	for (uint8_t i = 0U; i < n_digits; i++) {
		// Validate that character is an ASCII digit
		if (!ag_string_is_digit(str[i])) {
			return false;
		}

		// Multiply by 10 and add new digit
		value = (value * 10U) + ag_string_digit_to_value(str[i]);

		// Check for overflow (uint16_t max value is 65535)
		if (value > 65535U) {
			return false;
		}
	}

	// Success - write result
	*result = (uint16_t)value;
	return true;
}

// --------------------------------------------------------------------------------------------------------------------
/// \brief Parse exactly n digits from a string into a uint32_t value.
// --------------------------------------------------------------------------------------------------------------------
bool ag_string_parse_uint32_n(const char* str, uint8_t n_digits, uint32_t* result) {
	// Validate input parameters
	if (str == NULL || result == NULL) {
		return false;
	}

	if (n_digits == 0U || n_digits > AG_STRING_MAX_DIGITS) {
		return false;
	}

	// Parse digits with overflow detection
	uint64_t value = 0U;  // Use larger type for intermediate calculations

	for (uint8_t i = 0U; i < n_digits; i++) {
		// Validate that character is an ASCII digit
		if (!ag_string_is_digit(str[i])) {
			return false;
		}

		// Multiply by 10 and add new digit
		value = (value * 10U) + ag_string_digit_to_value(str[i]);

		// Check for overflow (uint32_t max value is 4294967295)
		if (value > 4294967295U) {
			return false;
		}
	}

	// Success - write result
	*result = (uint32_t)value;
	return true;
}
