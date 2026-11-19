/**
 * @file rerace_track.h
 * @brief ReRace Track Information Library - Public API
 *
 * This header provides track management functionality for ReRace files,
 * including track name extraction, validation, and formatting.
 *
 * @author Alex Giov
 * @date February 9, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 *
 * @par License:
 * This software is proprietary and confidential.
 */

#ifndef RERACE_TRACK_H
#define RERACE_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* INCLUDES                                                                   */
/*============================================================================*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ag_geo_coord/ag_geo_coord.h"

/*============================================================================*/
/* CONSTANTS AND MACROS                                                       */
/*============================================================================*/

/**
 * @brief Maximum length for track name string (including null terminator)
 */
#define RERACE_TRACK_NAME_MAX_LEN 32U

/**
 * @brief Maximum number of intermediate time lines (excluding finish line)
 */
#define RERACE_TRACK_MAX_INTERMEDIATE_TIME_LINES 7

/**
 * @brief Length of UTC time string (including null terminator)
 */
#define RERACE_TRACK_UTC_TIME_LENGTH 11

/**
 * @brief Length of UTC date string (including null terminator)
 */
#define RERACE_TRACK_UTC_DATE_LENGTH 7

/*============================================================================*/
/* TYPE DEFINITIONS                                                           */
/*============================================================================*/

/**
 * @brief Result codes for ReRace track operations
 */
typedef enum {
	RERACE_TRACK_SUCCESS = 0,			  ///< Operation completed successfully
	RERACE_TRACK_ERROR_INVALID_PARAM,	  ///< Invalid parameter passed
	RERACE_TRACK_ERROR_NOT_FOUND,		  ///< Track information not found
	RERACE_TRACK_ERROR_BUFFER_TOO_SMALL,  ///< Output buffer too small
	RERACE_TRACK_ERROR_CORRUPTED		  ///< Track data is corrupted
} rerace_track_result_t;

/**
 * @brief Track information structure
 * @details Contains all information about a race track including boundaries,
 * timing lines, and metadata. Uses WGS84 coordinates from ag_geo_coord library.
 */
typedef struct {
	uint32_t track_id;								  ///< Unique track identifier
	char track_name[RERACE_TRACK_NAME_MAX_LEN];		  ///< Human-readable track name
	char acq_utc_date[RERACE_TRACK_UTC_DATE_LENGTH];  ///< Acquisition date (UTC)
	char acq_utc_time[RERACE_TRACK_UTC_TIME_LENGTH];  ///< Acquisition time (UTC)

	ag_geo_rect_t track_zone;  ///< Track zone bounding box (WGS84)

	bool is_open;  ///< Track open/closed status

	uint8_t intermediate_line_count;  ///< Number of intermediate timing lines (0-7)
									  ///< Note: finish line is always included

	ag_geo_line_t timing_lines[RERACE_TRACK_MAX_INTERMEDIATE_TIME_LINES + 1];
	///< Timing lines array (finish + intermediates)
	///< Index 0 = finish line, 1-7 = intermediate lines
} rerace_track_info_t;

/**
 * @brief Parse track information from payload
 * @param[in] payload Pointer to payload data
 * @param[in] payload_size Size of payload data
 * @param[out] track_info Pointer to track info structure to fill
 * @return RERACE_TRACK_SUCCESS on success, error code otherwise
 * @note The payload pointer must remain valid during the call
 */
rerace_track_result_t rerace_track_parse_payload(const void* payload, uint32_t payload_size,
												 rerace_track_info_t* track_info);

/**
 * @brief Get track name as a formatted string
 * @details Returns a sanitized track name suitable for use in filenames
 *
 * @param[in] track_info Pointer to track info structure
 * @param[out] output_buffer Output buffer for formatted name
 * @param[in] buffer_size Size of output buffer
 * @return RERACE_TRACK_SUCCESS on success, error code otherwise
 *
 * @note The output will be sanitized (spaces replaced with underscores, etc.)
 */
rerace_track_result_t rerace_track_get_formatted_name(const rerace_track_info_t* track_info, char* output_buffer,
													  size_t buffer_size);

/**
 * @brief Validate track information
 * @details Checks if the track information is valid and complete
 *
 * @param[in] track_info Pointer to track info structure
 * @return true if valid, false otherwise
 */
bool rerace_track_is_valid(const rerace_track_info_t* track_info);

/**
 * @brief Initialize track info structure with default values
 * @details Sets all fields to safe default values
 *
 * @param[out] track_info Pointer to track info structure to initialize
 * @return RERACE_TRACK_SUCCESS on success, error code otherwise
 */
rerace_track_result_t rerace_track_init(rerace_track_info_t* track_info);

/**
 * @brief Convert result code to human-readable string
 * @param result Result code to convert
 * @return Pointer to constant string describing the error
 */
const char* rerace_track_get_error_string(rerace_track_result_t result);

/*============================================================================*/
/* INLINE FUNCTIONS                                                           */
/*============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* RERACE_TRACK_H */
