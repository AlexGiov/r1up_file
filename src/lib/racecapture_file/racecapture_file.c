/**
 * @file racecapture_file.c
 * @brief RaceCapture CSV log file writer library - Implementation
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

#include "racecapture_file.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ag_fsal.h"
#include "ag_mal.h"
#include "racecapture_file_cfg.h"

/*============================================================================*/
/* PRIVATE TYPES                                                              */
/*============================================================================*/

/**
 * @brief File state enumeration
 */
typedef enum {
	RACECAPTURE_STATE_CREATED,			///< File created, adding channels
	RACECAPTURE_STATE_HEADER_WRITTEN,	///< Header written, ready for data
	RACECAPTURE_STATE_ROW_IN_PROGRESS,	///< Currently writing a row
	RACECAPTURE_STATE_CLOSED			///< File closed
} racecapture_file_state_t;

/**
 * @brief Internal file structure
 */
typedef struct racecapture_file_t {
	fsal_handle_t* fsal_handle;										///< FSAL file handle
	racecapture_channel_t channels[RACECAPTURE_FILE_MAX_CHANNELS];	///< Channel definitions
	uint8_t num_channels;											///< Number of configured channels
	uint8_t base_sample_rate_hz;									///< Base sample rate (highest Hz in file)
	racecapture_file_state_t state;									///< Current state

	/* Row writing state */
	uint64_t current_sample_idx;  ///< Current sample index being written
	uint8_t current_column;		  ///< Current column being written (0..num_channels-1)
} racecapture_file_t;

/*============================================================================*/
/* PRIVATE FUNCTION DECLARATIONS                                             */
/*============================================================================*/

/**
 * @brief Determine if a sample should be written for a given channel
 * @param sample_idx Current sample index (0, 1, 2, ...)
 * @param channel_hz Channel sample rate in Hz
 * @param base_hz Base sample rate in Hz
 * @return true if sample should be written, false if field should be empty
 */
static bool racecapture_should_write_sample(uint64_t sample_idx, uint8_t channel_hz, uint8_t base_hz);

/**
 * @brief Write a formatted value to file
 * @param file Internal file structure
 * @param value Value to write
 * @param precision Number of decimal places
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 */
static racecapture_file_result_t racecapture_write_formatted_value(racecapture_file_t* file, double value,
																   uint8_t precision);

/*============================================================================*/
/* PUBLIC FUNCTION IMPLEMENTATIONS                                           */
/*============================================================================*/

racecapture_file_handle_t racecapture_file_create(const char* filepath) {
	if (filepath == NULL) {
		return NULL;
	}

	/* Allocate file structure */
	racecapture_file_t* file = (racecapture_file_t*)AG_MALLOC(sizeof(racecapture_file_t));
	if (file == NULL) {
		return NULL;
	}

	/* Initialize structure */
	memset(file, 0, sizeof(racecapture_file_t));
	file->state = RACECAPTURE_STATE_CREATED;
	file->base_sample_rate_hz = RACECAPTURE_FILE_DEFAULT_BASE_RATE_HZ;

	/* Allocate FSAL handle */
	file->fsal_handle = (fsal_handle_t*)AG_MALLOC(sizeof(fsal_handle_t));
	if (file->fsal_handle == NULL) {
		AG_FREE(file);
		return NULL;
	}

	/* Open file via FSAL */
	fsal_result_t fsal_res = fsal_open(filepath, FSAL_MODE_WRITE, file->fsal_handle);
	if (fsal_res != FSAL_SUCCESS) {
		AG_FREE(file->fsal_handle);
		AG_FREE(file);
		return NULL;
	}

	return (racecapture_file_handle_t)file;
}

racecapture_file_result_t racecapture_file_close(racecapture_file_handle_t handle) {
	if (handle == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Close FSAL handle */
	if (file->fsal_handle != NULL) {
		fsal_close(file->fsal_handle);
		AG_FREE(file->fsal_handle);
		file->fsal_handle = NULL;
	}

	/* Mark as closed */
	file->state = RACECAPTURE_STATE_CLOSED;

	/* Free structure */
	AG_FREE(file);

	return RACECAPTURE_FILE_SUCCESS;
}

racecapture_file_result_t racecapture_file_add_channel(racecapture_file_handle_t handle,
													   const racecapture_channel_t* channel) {
	if (handle == NULL || channel == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Check state */
	if (file->state != RACECAPTURE_STATE_CREATED) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* Check channel limit */
	if (file->num_channels >= RACECAPTURE_FILE_MAX_CHANNELS) {
		return RACECAPTURE_FILE_ERROR_MAX_CHANNELS;
	}

	/* Copy channel definition */
	memcpy(&file->channels[file->num_channels], channel, sizeof(racecapture_channel_t));
	file->num_channels++;

	/* Update base rate if this channel has higher Hz */
	if (channel->sample_rate_hz > file->base_sample_rate_hz) {
		file->base_sample_rate_hz = channel->sample_rate_hz;
	}

	return RACECAPTURE_FILE_SUCCESS;
}

racecapture_file_result_t racecapture_file_write_header(racecapture_file_handle_t handle) {
	if (handle == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Check state */
	if (file->state != RACECAPTURE_STATE_CREATED) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* Check that at least one channel is configured */
	if (file->num_channels == 0) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	/* Write header line */
	for (uint8_t i = 0; i < file->num_channels; i++) {
		racecapture_channel_t* ch = &file->channels[i];

		/* Format: "<Name>"|"<Units>"|min|max|Hz|precision */
		char header_buf[256];
		int len;

		if (ch->precision > 0) {
			// Use nanoprintf for deterministic output
			len = snprintf(header_buf, sizeof(header_buf), "\"%s\"|\"%s\"|%d|%d|%d|%d", ch->name, ch->units, ch->min,
						   ch->max, ch->sample_rate_hz, ch->precision);
		} else {
			// Use snprintf for deterministic output
			len = snprintf(header_buf, sizeof(header_buf), "\"%s\"|\"%s\"|%d|%d|%d", ch->name, ch->units, ch->min,
						   ch->max, ch->sample_rate_hz);
		}

		if (len < 0 || len >= (int)sizeof(header_buf)) {
			return RACECAPTURE_FILE_ERROR_FILE_IO;
		}

		/* Write to file */
		size_t written = 0;
		fsal_result_t fsal_res = fsal_write(header_buf, 1, (size_t)len, file->fsal_handle, &written);
		if (fsal_res != FSAL_SUCCESS || written != (size_t)len) {
			return RACECAPTURE_FILE_ERROR_FILE_IO;
		}

		/* Write comma separator (except for last channel) */
		if (i < file->num_channels - 1) {
			size_t comma_written = 0;
			if (fsal_write(",", 1, 1, file->fsal_handle, &comma_written) != FSAL_SUCCESS || comma_written != 1) {
				return RACECAPTURE_FILE_ERROR_FILE_IO;
			}
		}
	}

	/* Write newline */
	size_t newline_written = 0;
	if (fsal_write("\n", 1, 1, file->fsal_handle, &newline_written) != FSAL_SUCCESS || newline_written != 1) {
		return RACECAPTURE_FILE_ERROR_FILE_IO;
	}

	/* Update state */
	file->state = RACECAPTURE_STATE_HEADER_WRITTEN;

	return RACECAPTURE_FILE_SUCCESS;
}

racecapture_file_result_t racecapture_file_begin_row(racecapture_file_handle_t handle, uint64_t sample_idx) {
	if (handle == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Check state */
	if (file->state != RACECAPTURE_STATE_HEADER_WRITTEN && file->state != RACECAPTURE_STATE_ROW_IN_PROGRESS) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* If previous row was in progress, that's an error */
	if (file->state == RACECAPTURE_STATE_ROW_IN_PROGRESS && file->current_column != 0) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* Initialize row state */
	file->current_sample_idx = sample_idx;
	file->current_column = 0;
	file->state = RACECAPTURE_STATE_ROW_IN_PROGRESS;

	return RACECAPTURE_FILE_SUCCESS;
}

racecapture_file_result_t racecapture_file_write_value(racecapture_file_handle_t handle, double value) {
	if (handle == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Check state */
	if (file->state != RACECAPTURE_STATE_ROW_IN_PROGRESS) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* Check column bounds */
	if (file->current_column >= file->num_channels) {
		return RACECAPTURE_FILE_ERROR_CHANNEL_MISMATCH;
	}

	/* Get current channel */
	racecapture_channel_t* ch = &file->channels[file->current_column];

	/* Determine if we should write this sample (sparse logic)
	 * Exception: First two channels (Interval, Utc) are ALWAYS written per RaceCapture spec */
	bool should_write =
		(file->current_column < 2) ||
		racecapture_should_write_sample(file->current_sample_idx, ch->sample_rate_hz, file->base_sample_rate_hz);

	if (should_write) {
		/* Write formatted value */
		racecapture_file_result_t res = racecapture_write_formatted_value(file, value, ch->precision);
		if (res != RACECAPTURE_FILE_SUCCESS) {
			return res;
		}
	}
	/* else: write nothing (empty field for sparse CSV) */

	/* Advance to next column */
	file->current_column++;

	/* Write comma separator (except for last column) */
	if (file->current_column < file->num_channels) {
		size_t comma_written = 0;
		if (fsal_write(",", 1, 1, file->fsal_handle, &comma_written) != FSAL_SUCCESS || comma_written != 1) {
			return RACECAPTURE_FILE_ERROR_FILE_IO;
		}
	}

	return RACECAPTURE_FILE_SUCCESS;
}

racecapture_file_result_t racecapture_file_write_empty(racecapture_file_handle_t handle) {
	if (handle == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Check state */
	if (file->state != RACECAPTURE_STATE_ROW_IN_PROGRESS) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* Check column bounds */
	if (file->current_column >= file->num_channels) {
		return RACECAPTURE_FILE_ERROR_CHANNEL_MISMATCH;
	}

	/* Write nothing (empty field) */

	/* Advance to next column */
	file->current_column++;

	/* Write comma separator (except for last column) */
	if (file->current_column < file->num_channels) {
		size_t comma_written = 0;
		if (fsal_write(",", 1, 1, file->fsal_handle, &comma_written) != FSAL_SUCCESS || comma_written != 1) {
			return RACECAPTURE_FILE_ERROR_FILE_IO;
		}
	}

	return RACECAPTURE_FILE_SUCCESS;
}

racecapture_file_result_t racecapture_file_end_row(racecapture_file_handle_t handle) {
	if (handle == NULL) {
		return RACECAPTURE_FILE_ERROR_INVALID_PARAM;
	}

	racecapture_file_t* file = (racecapture_file_t*)handle;

	/* Check state */
	if (file->state != RACECAPTURE_STATE_ROW_IN_PROGRESS) {
		return RACECAPTURE_FILE_ERROR_STATE;
	}

	/* Verify all columns were written */
	if (file->current_column != file->num_channels) {
		return RACECAPTURE_FILE_ERROR_CHANNEL_MISMATCH;
	}

	/* Write newline */
	size_t newline_written = 0;
	if (fsal_write("\n", 1, 1, file->fsal_handle, &newline_written) != FSAL_SUCCESS || newline_written != 1) {
		return RACECAPTURE_FILE_ERROR_FILE_IO;
	}

	/* Reset column for next row */
	file->current_column = 0;

	return RACECAPTURE_FILE_SUCCESS;
}

/*============================================================================*/
/* PRIVATE FUNCTION IMPLEMENTATIONS                                          */
/*============================================================================*/

static bool racecapture_should_write_sample(uint64_t sample_idx, uint8_t channel_hz, uint8_t base_hz) {
	/* Edge case: avoid division by zero */
	if (channel_hz == 0 || base_hz == 0) {
		return false;
	}

	/* Special case: channel rate equals base rate -> always write */
	if (channel_hz >= base_hz) {
		return true;
	}

	/* Calculate frequency divisor */
	uint8_t divisor = base_hz / channel_hz;

	/* Write if sample_idx is a multiple of the divisor */
	return (sample_idx % divisor) == 0;
}

static racecapture_file_result_t racecapture_write_formatted_value(racecapture_file_t* file, double value,
																   uint8_t precision) {
	char value_buf[RACECAPTURE_FILE_VALUE_BUFFER_SIZE];
	int len;

	/* Format value with specified precision */
	/* Use snprintf for deterministic floating-point formatting across platforms */
	if (precision > 0) {
		len = snprintf(value_buf, sizeof(value_buf), "%.*f", precision, value);
	} else {
		/* Integer formatting */
		len = snprintf(value_buf, sizeof(value_buf), "%.0f", value);
	}

	if (len < 0 || len >= (int)sizeof(value_buf)) {
		return RACECAPTURE_FILE_ERROR_FILE_IO;
	}

	/* Write to file */
	size_t written = 0;
	fsal_result_t fsal_res = fsal_write(value_buf, 1, (size_t)len, file->fsal_handle, &written);
	if (fsal_res != FSAL_SUCCESS || written != (size_t)len) {
		return RACECAPTURE_FILE_ERROR_FILE_IO;
	}

	return RACECAPTURE_FILE_SUCCESS;
}
