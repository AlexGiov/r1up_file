/**
 * @file r1up_to_racecapture.c
 * @brief R1UP to RaceCapture CSV Converter - Implementation
 *
 * @author Alex Giov
 * @date January 23, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 */

/*============================================================================*/
/* INCLUDES                                                                   */
/*============================================================================*/

#include "r1up_to_racecapture.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ag_fixedpoint/ag_fixedpoint.h"
#include "ag_mal.h"
#include "ag_string/ag_string.h"
#include "ag_time/ag_time.h"
#include "r1up_file.h"
#include "racecapture_file/racecapture_file.h"
#include "rerace_track/rerace_track.h"

/*==================================================================================================================*/
/* PRIVATE CONSTANTS AND MACROS																						*/
/*==================================================================================================================*/

/**
 * @defgroup R1UP_Private_Constants Private Constants
 * @brief Internal constants used by the implementation
 * @{
 */

/**
 * @brief Macro to conditionally remove static keyword for unit testing
 */
#ifdef UNIT_TESTING
#define STATIC
#else
#define STATIC static
#endif
/** @} */ /* End of R1UP_Private_Constants */

/*============================================================================*/
/* PRIVATE FUNCTION DECLARATIONS                                             */
/*============================================================================*/

/**
 * @brief Configure all RaceCapture channels from mapping table
 */
static r1up_to_rc_result_t configure_racecapture_channels(racecapture_file_handle_t rc_handle);

/**
 * @brief Convert and write all data samples
 */
static r1up_to_rc_result_t convert_all_samples(r1up_file_handle_t r1up_handle, racecapture_file_handle_t rc_handle,
											   uint64_t total_samples);

/**
 * @brief Convert and write all data samples using streaming mode (low memory)
 */
static r1up_to_rc_result_t convert_all_samples_streaming(r1up_file_handle_t r1up_handle,
														 racecapture_file_handle_t rc_handle, uint64_t total_samples);

/**
 * @brief Calculate Interval value for given sample index
 */
static inline double calculate_interval_ms(uint64_t sample_idx);

/**
 * @brief Convert acquisition date string to filename format
 */
static r1up_to_rc_result_t r1up_to_racecapture_convert_date_for_filename(const char* input_date, char* output_buffer);

/**
 * @brief Build RaceCapture output filename from R1UP file metadata
 */
STATIC r1up_to_rc_result_t r1up_to_racecapture_build_filename(r1up_file_handle_t r1up_handle, char* out_filename,
															  size_t max_len);

/**
 * @brief Parse acquisition date string and convert to UTC milliseconds
 */
static r1up_to_rc_result_t parse_acq_date_to_utc_ms(const char* date_str, uint64_t* out_utc_ms);

/**
 */
static r1up_to_rc_result_t r1up_to_racecapture_resolve_output_filepath(const char* r1up_filepath,
																	   const char* racecapture_filepath,
																	   r1up_file_handle_t r1up_handle,
																	   char* resolved_filepath, size_t buffer_size);

r1up_to_rc_result_t r1up_to_racecapture_convert(const char* r1up_filepath, const char* racecapture_filepath) {
	if (r1up_filepath == NULL) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	r1up_to_rc_result_t result = R1UP_TO_RC_SUCCESS;
	r1up_file_handle_t r1up_handle = NULL;
	racecapture_file_handle_t rc_handle = NULL;

	/* Step 1: Open R1UP file */
	r1up_handle = r1up_file_open(r1up_filepath, R1UP_FILE_MODE_READ_ONLY);
	if (r1up_handle == NULL) {
		result = R1UP_TO_RC_ERROR_R1UP_OPEN;
		goto cleanup;
	}

	/* Step 2: Resolve output file path (use provided or auto-generate) */
	char output_filepath[R1UP_TO_RC_FILEPATH_MAX_LEN];
	result = r1up_to_racecapture_resolve_output_filepath(r1up_filepath, racecapture_filepath, r1up_handle,
														 output_filepath, sizeof(output_filepath));
	if (result != R1UP_TO_RC_SUCCESS) {
		goto cleanup;
	}

	/* Step 3: Read R1UP body */
	if (r1up_file_read_body(r1up_handle) != R1UP_FILE_SUCCESS) {
		result = R1UP_TO_RC_ERROR_R1UP_READ;
		goto cleanup;
	}

	/* Step 4: Get total sample count */
	uint64_t total_samples = r1up_file_get_body_sample_count(r1up_handle);
	if (total_samples == 0) {
		result = R1UP_TO_RC_ERROR_R1UP_READ;
		goto cleanup;
	}

	/* Step 5: Create RaceCapture CSV file */
	rc_handle = racecapture_file_create(output_filepath);
	if (rc_handle == NULL) {
		result = R1UP_TO_RC_ERROR_RC_CREATE;
		goto cleanup;
	}

	/* Step 6: Configure all 26 channels */
	result = configure_racecapture_channels(rc_handle);
	if (result != R1UP_TO_RC_SUCCESS) {
		goto cleanup;
	}

	/* Step 7: Write CSV header */
	if (racecapture_file_write_header(rc_handle) != RACECAPTURE_FILE_SUCCESS) {
		result = R1UP_TO_RC_ERROR_RC_WRITE;
		goto cleanup;
	}

	/* Step 8: Convert and write all samples */
	result = convert_all_samples(r1up_handle, rc_handle, total_samples);

cleanup:
	/* Close files */
	if (rc_handle != NULL) {
		racecapture_file_close(rc_handle);
	}
	if (r1up_handle != NULL) {
		r1up_file_close(r1up_handle);
	}

	return result;
}

r1up_to_rc_result_t r1up_to_racecapture_convert_streaming(const char* r1up_filepath, const char* racecapture_filepath) {
	if (r1up_filepath == NULL) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	r1up_to_rc_result_t result = R1UP_TO_RC_SUCCESS;
	r1up_file_handle_t r1up_handle = NULL;
	racecapture_file_handle_t rc_handle = NULL;

	/* Step 1: Open R1UP file (NO read_body - streaming mode) */
	r1up_handle = r1up_file_open(r1up_filepath, R1UP_FILE_MODE_READ_ONLY);
	if (r1up_handle == NULL) {
		result = R1UP_TO_RC_ERROR_R1UP_OPEN;
		goto cleanup;
	}

	/* Step 2: Resolve output file path (use provided or auto-generate) */
	char output_filepath[R1UP_TO_RC_FILEPATH_MAX_LEN];
	result = r1up_to_racecapture_resolve_output_filepath(r1up_filepath, racecapture_filepath, r1up_handle,
														 output_filepath, sizeof(output_filepath));
	if (result != R1UP_TO_RC_SUCCESS) {
		goto cleanup;
	}

	/* Step 3: Get metadata (variable count and sample count) */
	uint64_t total_samples = r1up_file_get_body_sample_count(r1up_handle);
	if (total_samples == 0) {
		result = R1UP_TO_RC_ERROR_R1UP_READ;
		goto cleanup;
	}

	/* Step 4: Create RaceCapture CSV file */
	rc_handle = racecapture_file_create(output_filepath);
	if (rc_handle == NULL) {
		result = R1UP_TO_RC_ERROR_RC_CREATE;
		goto cleanup;
	}

	/* Step 5: Configure all 26 channels */
	result = configure_racecapture_channels(rc_handle);
	if (result != R1UP_TO_RC_SUCCESS) {
		goto cleanup;
	}

	/* Step 6: Write CSV header */
	if (racecapture_file_write_header(rc_handle) != RACECAPTURE_FILE_SUCCESS) {
		result = R1UP_TO_RC_ERROR_RC_WRITE;
		goto cleanup;
	}

	/* Step 7: Convert and write all samples using streaming mode */
	result = convert_all_samples_streaming(r1up_handle, rc_handle, total_samples);

cleanup:
	/* Close files */
	if (rc_handle != NULL) {
		racecapture_file_close(rc_handle);
	}
	if (r1up_handle != NULL) {
		r1up_file_stream_body_end(r1up_handle); /* Cleanup streaming resources */
		r1up_file_close(r1up_handle);
	}

	return result;
}

const char* r1up_to_rc_get_error_string(r1up_to_rc_result_t result) {
	switch (result) {
		case R1UP_TO_RC_SUCCESS:
			return "Success";
		case R1UP_TO_RC_ERROR_INVALID_PARAM:
			return "Invalid parameter";
		case R1UP_TO_RC_ERROR_R1UP_OPEN:
			return "Failed to open R1UP file";
		case R1UP_TO_RC_ERROR_R1UP_READ:
			return "Failed to read R1UP data";
		case R1UP_TO_RC_ERROR_RC_CREATE:
			return "Failed to create RaceCapture file";
		case R1UP_TO_RC_ERROR_RC_WRITE:
			return "Failed to write RaceCapture data";
		case R1UP_TO_RC_ERROR_MEMORY:
			return "Memory allocation failed";
		case R1UP_TO_RC_ERROR_CONVERSION:
			return "Data conversion error";
		default:
			return "Unknown error";
	}
}

/*============================================================================*/
/* PRIVATE FUNCTION IMPLEMENTATIONS                                          */
/*============================================================================*/

static r1up_to_rc_result_t configure_racecapture_channels(racecapture_file_handle_t rc_handle) {
/* Macro to expand mapping table and add each channel */
#define ADD_CHANNEL_ENTRY(type, var_id, name, units, min, max, hz, prec, scale)         \
	{                                                                                   \
		racecapture_channel_t ch = {name, units, min, max, hz, prec};                   \
		if (racecapture_file_add_channel(rc_handle, &ch) != RACECAPTURE_FILE_SUCCESS) { \
			return R1UP_TO_RC_ERROR_RC_CREATE;                                          \
		}                                                                               \
	}

	/* Expand mapping table to add all 26 channels */
	R1UP_TO_RC_MAPPING_TABLE(ADD_CHANNEL_ENTRY)

#undef ADD_CHANNEL_ENTRY

	return R1UP_TO_RC_SUCCESS;
}

static r1up_to_rc_result_t convert_all_samples(r1up_file_handle_t r1up_handle, racecapture_file_handle_t rc_handle,
											   uint64_t total_samples) {
	/* Get number of variables from file */
	uint32_t var_count = r1up_file_get_variable_number(r1up_handle);
	if (var_count == 0) {
		return R1UP_TO_RC_ERROR_R1UP_READ;
	}

	/* Allocate conversion buffers array dynamically */
	ag_fixedpoint_float_t** float_buffers =
		(ag_fixedpoint_float_t**)AG_MALLOC(var_count * sizeof(ag_fixedpoint_float_t*));
	if (float_buffers == NULL) {
		return R1UP_TO_RC_ERROR_MEMORY;
	}
	memset(float_buffers, 0, var_count * sizeof(ag_fixedpoint_float_t*));

/* Pre-convert all R1UP data to float (batch conversion for performance) */
#define GET_VARIABLES(var_id_symbol, var_id_value, ...)                                                            \
	{                                                                                                              \
		int32_t pos = r1up_file_get_var_position(r1up_handle, var_id_symbol);                                      \
		if (pos >= 0) {                                                                                            \
			float_buffers[pos] = (ag_fixedpoint_float_t*)AG_MALLOC(total_samples * sizeof(ag_fixedpoint_float_t)); \
			if (float_buffers[pos] == NULL) {                                                                      \
				goto cleanup_buffers;                                                                              \
			}                                                                                                      \
			r1up_file_result_t res =                                                                               \
				r1up_file_get_var_float(r1up_handle, var_id_symbol, float_buffers[pos], total_samples);            \
			if (res != R1UP_FILE_SUCCESS) {                                                                        \
				goto cleanup_buffers;                                                                              \
			}                                                                                                      \
		}                                                                                                          \
	}

	R1UP_FILE_VARIABLE_ID_TABLE(GET_VARIABLES)
#undef GET_VARIABLES

	/* Get GPS component arrays for conversion */
	int32_t pos = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LAT_DEG);
	if (pos < 0) {
		goto cleanup_buffers;
	}
	const ag_fixedpoint_float_t* lat_deg_arr = float_buffers[pos];
	pos = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LAT_MIN);
	if (pos < 0) {
		goto cleanup_buffers;
	}
	const ag_fixedpoint_float_t* lat_min_arr = float_buffers[pos];
	pos = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LAT_DECI);
	if (pos < 0) {
		goto cleanup_buffers;
	}
	const ag_fixedpoint_float_t* lat_deci_arr = float_buffers[pos];
	pos = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LONG_DEG);
	if (pos < 0) {
		goto cleanup_buffers;
	}
	const ag_fixedpoint_float_t* lon_deg_arr = float_buffers[pos];
	pos = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LONG_MIN);
	if (pos < 0) {
		goto cleanup_buffers;
	}
	const ag_fixedpoint_float_t* lon_min_arr = float_buffers[pos];
	pos = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LONG_DECI);
	if (pos < 0) {
		goto cleanup_buffers;
	}
	const ag_fixedpoint_float_t* lon_deci_arr = float_buffers[pos];

	/* Get acquisition date from R1UP file and convert to UTC milliseconds */
	char* acq_date_str = (char*)r1up_file_get_payload(r1up_handle, R1UP_FILE_ACQ_DATE_BID);
	uint64_t utc_base_ms_u64 = 0;
	if (parse_acq_date_to_utc_ms(acq_date_str, &utc_base_ms_u64) != R1UP_TO_RC_SUCCESS) {
		goto cleanup_buffers;
	}
	double utc_base_ms = (double)utc_base_ms_u64;

	/* Write all samples row by row */
	for (uint64_t sample_idx = 0; sample_idx < total_samples; sample_idx++) {
		racecapture_file_begin_row(rc_handle, sample_idx);

		/* Channel index counter */
		uint8_t channel_idx = 0;

/* Macro to write each channel value */
#define WRITE_CHANNEL_VALUE(chan_type, r1up_var_id, name, units, min, max, hz, prec, scale)               \
	{                                                                                                     \
		double value = 0.0;                                                                               \
		bool write_value = true;                                                                          \
                                                                                                          \
		if (chan_type == SYSTEM_INTERVAL) {                                                               \
			value = calculate_interval_ms(sample_idx);                                                    \
		} else if (chan_type == SYSTEM_UTC) {                                                             \
			/* UTC = base timestamp + interval */                                                         \
			value = utc_base_ms + calculate_interval_ms(sample_idx);                                      \
		} else if (chan_type == DATA_GPS_LAT) {                                                           \
			/* GPS latitude conversion */                                                                 \
			if (lat_deg_arr != NULL) {                                                                    \
				value = r1up_file_convert_gps_latitude(lat_deg_arr[sample_idx], lat_min_arr[sample_idx],  \
													   lat_deci_arr[sample_idx]);                         \
			} else {                                                                                      \
				write_value = false;                                                                      \
			}                                                                                             \
		} else if (chan_type == DATA_GPS_LON) {                                                           \
			/* GPS longitude conversion */                                                                \
			if (lon_deg_arr != NULL) {                                                                    \
				value = r1up_file_convert_gps_longitude(lon_deg_arr[sample_idx], lon_min_arr[sample_idx], \
														lon_deci_arr[sample_idx]);                        \
			} else {                                                                                      \
				write_value = false;                                                                      \
			}                                                                                             \
		} else if (chan_type == DATA_SIMPLE || chan_type == DATA_SCALED) {                                \
			/* Get from pre-converted float buffer using position lookup */                               \
			int32_t var_pos = r1up_file_get_var_position(r1up_handle, r1up_var_id);                       \
			if (var_pos >= 0 && float_buffers[var_pos] != NULL) {                                         \
				value = (double)float_buffers[var_pos][sample_idx] * scale;                               \
			} else {                                                                                      \
				write_value = false;                                                                      \
			}                                                                                             \
		}                                                                                                 \
                                                                                                          \
		if (write_value) {                                                                                \
			racecapture_file_write_value(rc_handle, value);                                               \
		} else {                                                                                          \
			racecapture_file_write_empty(rc_handle);                                                      \
		}                                                                                                 \
		channel_idx++;                                                                                    \
	}

		/* Expand mapping table to write all channels */
		R1UP_TO_RC_MAPPING_TABLE(WRITE_CHANNEL_VALUE)
#undef WRITE_CHANNEL_VALUE

		racecapture_file_end_row(rc_handle);
	}

/* Free conversion buffers */
cleanup_buffers:
	for (uint32_t i = 0; i < var_count; i++) {
		if (float_buffers[i] != NULL) {
			AG_FREE(float_buffers[i]);
		}
	}
	AG_FREE(float_buffers);

	return R1UP_TO_RC_SUCCESS;
}

static inline double calculate_interval_ms(uint64_t sample_idx) {
	/* Interval = sample_idx * base_period (100 ms for 10 Hz) */
	return (double)(sample_idx * R1UP_TO_RC_BASE_PERIOD_MS);
}

static r1up_to_rc_result_t convert_all_samples_streaming(r1up_file_handle_t r1up_handle,
														 racecapture_file_handle_t rc_handle, uint64_t total_samples) {
	/* Get number of variables from file */
	uint32_t var_count = r1up_file_get_variable_number(r1up_handle);
	if (var_count == 0) {
		return R1UP_TO_RC_ERROR_R1UP_READ;
	}

	/* Allocate row buffer dynamically based on actual variable count */
	ag_fixedpoint_float_t* r1up_row = (ag_fixedpoint_float_t*)AG_MALLOC(var_count * sizeof(ag_fixedpoint_float_t));
	if (r1up_row == NULL) {
		return R1UP_TO_RC_ERROR_MEMORY;
	}

	/* Initialize streaming mode */
	if (r1up_file_stream_body_begin(r1up_handle) != R1UP_FILE_SUCCESS) {
		AG_FREE(r1up_row);
		return R1UP_TO_RC_ERROR_R1UP_READ;
	}

	/* Get acquisition date from R1UP file and convert to UTC milliseconds */
	char* acq_date_str = (char*)r1up_file_get_payload(r1up_handle, R1UP_FILE_ACQ_DATE_BID);
	uint64_t utc_base_ms_u64 = 0;
	if (parse_acq_date_to_utc_ms(acq_date_str, &utc_base_ms_u64) != R1UP_TO_RC_SUCCESS) {
		AG_FREE(r1up_row);
		return R1UP_TO_RC_ERROR_CONVERSION;
	}
	double utc_base_ms = (double)utc_base_ms_u64;

	/* Read and convert samples row by row */
	r1up_to_rc_result_t result = R1UP_TO_RC_SUCCESS;

	for (uint64_t sample_idx = 0; sample_idx < total_samples; sample_idx++) {
		/* Read next row from R1UP file (with automatic sample-and-hold) */
		if (r1up_file_stream_body_next_row(r1up_handle, r1up_row, var_count) != R1UP_FILE_SUCCESS) {
			result = R1UP_TO_RC_ERROR_R1UP_READ;
			break;
		}

		/* Begin RaceCapture CSV row */
		racecapture_file_begin_row(rc_handle, sample_idx);

		/* Channel index counter */
		uint8_t channel_idx = 0;

/* Macro to write each channel value from streaming row */
#define WRITE_CHANNEL_STREAMING(chan_type, r1up_var_id, name, units, min, max, hz, prec, scale)     \
	{                                                                                               \
		double value = 0.0;                                                                         \
		bool write_value = true;                                                                    \
                                                                                                    \
		if (chan_type == SYSTEM_INTERVAL) {                                                         \
			value = calculate_interval_ms(sample_idx);                                              \
		} else if (chan_type == SYSTEM_UTC) {                                                       \
			/* UTC = base timestamp + interval */                                                   \
			value = utc_base_ms + calculate_interval_ms(sample_idx);                                \
		} else if (chan_type == DATA_GPS_LAT) {                                                     \
			/* GPS latitude: extract components using position lookup */                            \
			int32_t pos_deg = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LAT_DEG);    \
			int32_t pos_min = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LAT_MIN);    \
			int32_t pos_deci = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LAT_DECI);  \
			uint8_t lat_deg = (pos_deg >= 0) ? (uint8_t)r1up_row[pos_deg] : 0;                      \
			int8_t lat_min = (pos_min >= 0) ? (int8_t)r1up_row[pos_min] : 0;                        \
			uint16_t lat_deci = (pos_deci >= 0) ? (uint16_t)r1up_row[pos_deci] : 0;                 \
			value = r1up_file_convert_gps_latitude(lat_deg, lat_min, lat_deci);                     \
		} else if (chan_type == DATA_GPS_LON) {                                                     \
			/* GPS longitude: extract components using position lookup */                           \
			int32_t pos_deg = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LONG_DEG);   \
			int32_t pos_min = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LONG_MIN);   \
			int32_t pos_deci = r1up_file_get_var_position(r1up_handle, R1UP_FILE_VAR_ID_LONG_DECI); \
			uint8_t lon_deg = (pos_deg >= 0) ? (uint8_t)r1up_row[pos_deg] : 0;                      \
			int8_t lon_min = (pos_min >= 0) ? (int8_t)r1up_row[pos_min] : 0;                        \
			uint16_t lon_deci = (pos_deci >= 0) ? (uint16_t)r1up_row[pos_deci] : 0;                 \
			value = r1up_file_convert_gps_longitude(lon_deg, lon_min, lon_deci);                    \
		} else if (chan_type == DATA_SIMPLE || chan_type == DATA_SCALED) {                          \
			/* Read from streaming row buffer using position lookup */                              \
			int32_t var_pos = r1up_file_get_var_position(r1up_handle, r1up_var_id);                 \
			value = (var_pos >= 0) ? (r1up_row[var_pos] * scale) : 0.0;                             \
		}                                                                                           \
                                                                                                    \
		if (write_value) {                                                                          \
			racecapture_file_write_value(rc_handle, value);                                         \
		} else {                                                                                    \
			racecapture_file_write_empty(rc_handle);                                                \
		}                                                                                           \
		channel_idx++;                                                                              \
	}

		/* Expand mapping table to write all channels */
		R1UP_TO_RC_MAPPING_TABLE(WRITE_CHANNEL_STREAMING)
#undef WRITE_CHANNEL_STREAMING

		/* End RaceCapture CSV row */
		racecapture_file_end_row(rc_handle);
	}

	/* Cleanup */
	AG_FREE(r1up_row);

	return result;
}

static r1up_to_rc_result_t r1up_to_racecapture_convert_date_for_filename(const char* input_date, char* output_buffer) {
	if (input_date == NULL || output_buffer == NULL) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	struct tm t = {0};
	uint32_t ns = 0;

	/* Parse "DD/MM/YYYY - HH:MM:SS.cc" using ag_time portable parser */
	if (ag_time_strptime(input_date, "%d/%m/%Y - %H:%M:%S.%f", &t, &ns) != AG_TIME_OK) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	/* Format "YYYYMMDD-HHMMSS" (15 chars) */
	size_t len = ag_time_strftime(output_buffer, 32, "%Y%m%d-%H%M%S", &t);
	if (len == 0) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	/* Append centiseconds: nanoseconds → centiseconds (÷ 10,000,000) */
	uint8_t centisec = (uint8_t)(ns / 10000000u);
	snprintf(output_buffer + len, 32 - len, "%02u", centisec);

	return R1UP_TO_RC_SUCCESS;
}

STATIC r1up_to_rc_result_t r1up_to_racecapture_build_filename(r1up_file_handle_t r1up_handle, char* out_filename,
															  size_t max_len) {
	if (r1up_handle == NULL || out_filename == NULL || max_len == 0) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	// Initialize to zero
	memset(out_filename, 0, max_len);

	// Get track name from track feature block
	rerace_track_info_t* track_info = (rerace_track_info_t*)r1up_file_get_payload(r1up_handle, R1UP_FILE_TRACK_FTR_BID);

	// Get acquisition date from acquisition date block
	char* payload = (char*)r1up_file_get_payload(r1up_handle, R1UP_FILE_ACQ_DATE_BID);

	// Convert date to filename format
	char filename_date[20];
	r1up_to_rc_result_t date_result = r1up_to_racecapture_convert_date_for_filename(payload, filename_date);
	if (date_result != R1UP_TO_RC_SUCCESS) {
		return date_result;
	}

	// Get basename
	const char* opened_filename = r1up_file_get_basename(r1up_handle);

	// Remove extension from basename
	char basename_without_ext[64];
	strncpy(basename_without_ext, opened_filename, sizeof(basename_without_ext) - 1);
	basename_without_ext[sizeof(basename_without_ext) - 1] = '\0';	// Ensure null termination
	char* dot = strrchr(basename_without_ext, '.');					// Find last '.'
	if (dot != NULL) {
		*dot = '\0';  // Truncate at the dot
	}

	// Build complete RaceCapture filename: "TrackName_YYYYMMDD-HHMMSScc_basename"
	// Use snprintf for deterministic output across platforms
	snprintf(out_filename, max_len, "%s_%s_%s" R1UP_TO_RC_OUTPUT_FILE_EXTENSION, track_info->track_name, filename_date,
			 basename_without_ext);
	return R1UP_TO_RC_SUCCESS;
}

/**
 * @brief Parse acquisition date string and convert to UTC milliseconds
 * @details Converts R1UP acquisition date format ("DD/MM/YYYY - HH:MM:SS.cc") to UTC timestamp
 *          in milliseconds since Unix epoch (1 Jan 1970 00:00:00). Uses the ag_time portable
 *          library for parsing (ag_time_strptime) and UTC conversion (ag_time_mktime_utc),
 *          ensuring consistent behavior across Windows, Linux, and embedded Newlib targets.
 *
 * @param[in]  date_str   Date string in format "DD/MM/YYYY - HH:MM:SS.cc" (22+ chars)
 * @param[out] out_utc_ms Pointer to store resulting UTC timestamp in milliseconds
 *
 * @return R1UP_TO_RC_SUCCESS on success
 * @return R1UP_TO_RC_ERROR_INVALID_PARAM if parameters are NULL or date format is invalid
 * @return R1UP_TO_RC_ERROR_CONVERSION if struct tm to epoch conversion fails
 *
 * @note This function assumes the input date is already in UTC timezone.
 */
static r1up_to_rc_result_t parse_acq_date_to_utc_ms(const char* date_str, uint64_t* out_utc_ms) {
	if (date_str == NULL || out_utc_ms == NULL) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	struct tm t = {0};
	uint32_t ns = 0;

	/* Step 1: parsing */
	if (ag_time_strptime(date_str, "%d/%m/%Y - %H:%M:%S.%f", &t, &ns) != AG_TIME_OK) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	/* Step 2: struct tm → epoch (UTC) */
	time_t epoch;
	if (ag_time_mktime_utc(&t, &epoch) != AG_TIME_OK) {
		return R1UP_TO_RC_ERROR_CONVERSION;
	}

	/* Step 3: compose timespec and convert to milliseconds */
	ag_time_timespec_t ts = {.tv_sec = epoch, .tv_nsec = ns};
	*out_utc_ms = ag_time_to_ms(&ts);

	return R1UP_TO_RC_SUCCESS;
}

static r1up_to_rc_result_t r1up_to_racecapture_resolve_output_filepath(const char* r1up_filepath,
																	   const char* racecapture_filepath,
																	   r1up_file_handle_t r1up_handle,
																	   char* resolved_filepath, size_t buffer_size) {
	if (r1up_filepath == NULL || resolved_filepath == NULL || buffer_size == 0) {
		return R1UP_TO_RC_ERROR_INVALID_PARAM;
	}

	/* If user provided explicit path, use it */
	if (racecapture_filepath != NULL) {
		strncpy(resolved_filepath, racecapture_filepath, buffer_size - 1);
		resolved_filepath[buffer_size - 1] = '\0';
		return R1UP_TO_RC_SUCCESS;
	}

	/* Auto-generate filename in same directory as input file */

	/* Extract directory from r1up_filepath */
	const char* last_slash = strrchr(r1up_filepath, '/');
	const char* last_backslash = strrchr(r1up_filepath, '\\');
	const char* separator = (last_slash > last_backslash) ? last_slash : last_backslash;

	/* Generate filename */
	char racecapture_filename[R1UP_TO_RC_FILENAME_MAX_LEN];
	r1up_to_rc_result_t result =
		r1up_to_racecapture_build_filename(r1up_handle, racecapture_filename, sizeof(racecapture_filename));
	if (result != R1UP_TO_RC_SUCCESS) {
		return result;
	}

	/* Build complete path */
	if (separator != NULL) {
		/* Input has directory path - extract it */
		size_t dir_len = separator - r1up_filepath + 1; /* Include separator */
		if (dir_len >= buffer_size) {
			return R1UP_TO_RC_ERROR_INVALID_PARAM;
		}
		memcpy(resolved_filepath, r1up_filepath, dir_len);
		resolved_filepath[dir_len] = '\0';
		strncat(resolved_filepath, racecapture_filename, buffer_size - strlen(resolved_filepath) - 1);
	} else {
		/* No directory path - use current directory */
		strncpy(resolved_filepath, racecapture_filename, buffer_size - 1);
		resolved_filepath[buffer_size - 1] = '\0';
	}

	return R1UP_TO_RC_SUCCESS;
}