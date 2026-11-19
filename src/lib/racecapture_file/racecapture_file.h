/**
 * @file racecapture_file.h
 * @brief RaceCapture CSV log file writer library (Track Attack compatible)
 * @details Generic library for writing RaceCapture sparse CSV format.
 *          Channels are fully configurable at runtime.
 *          Only Interval and Utc are mandatory system channels.
 *
 * @author Alex Giov
 * @date January 23, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 *
 * @par Format Reference:
 * Official RaceCapture format: https://wiki.autosportlabs.com/RaceCapturePro2_logfile_reference
 * This library generates Track Attack compatible CSV files.
 *
 * @par Usage Example:
 * @code
 * // 1. Create file
 * racecapture_file_handle_t rc = racecapture_file_create("output.csv");
 *
 * // 2. Configure channels
 * racecapture_channel_t interval = {"Interval", "ms", 0, 0, 1, 0};
 * racecapture_file_add_channel(rc, &interval);
 *
 * racecapture_channel_t rpm = {"rpm", "RPM", 0, 12000, 10, 0};
 * racecapture_file_add_channel(rc, &rpm);
 *
 * // 3. Write header
 * racecapture_file_write_header(rc);
 *
 * // 4. Write data rows
 * for (uint64_t i = 0; i < 1000; i++) {
 *     racecapture_file_begin_row(rc, i);
 *     racecapture_file_write_value(rc, i * 100.0);  // Interval
 *     racecapture_file_write_value(rc, 3000.0);     // rpm
 *     racecapture_file_end_row(rc);
 * }
 *
 * // 5. Close
 * racecapture_file_close(rc);
 * @endcode
 */

#ifndef RACECAPTURE_FILE_H
#define RACECAPTURE_FILE_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* PUBLIC TYPES                                                               */
/*============================================================================*/

/**
 * @brief Opaque handle for RaceCapture file operations
 */
typedef struct racecapture_file_t* racecapture_file_handle_t;

/**
 * @brief Result codes for RaceCapture file operations
 */
typedef enum {
	RACECAPTURE_FILE_SUCCESS = 0,			  ///< Operation successful
	RACECAPTURE_FILE_ERROR_INVALID_PARAM,	  ///< Invalid parameter (NULL pointer, etc.)
	RACECAPTURE_FILE_ERROR_FILE_IO,			  ///< File I/O error (fopen, fwrite failed)
	RACECAPTURE_FILE_ERROR_MEMORY,			  ///< Memory allocation failed
	RACECAPTURE_FILE_ERROR_STATE,			  ///< Called in wrong order (e.g. write_value before begin_row)
	RACECAPTURE_FILE_ERROR_CHANNEL_MISMATCH,  ///< Wrong number of values in row
	RACECAPTURE_FILE_ERROR_MAX_CHANNELS		  ///< Exceeded maximum number of channels
} racecapture_file_result_t;

/**
 * @brief Channel metadata structure
 * @details Defines a single channel in the RaceCapture log file.
 *          All fields are written to the CSV header.
 */
typedef struct {
	const char* name;		 ///< Channel name (e.g. "rpm", "latitude")
	const char* units;		 ///< Units label (e.g. "RPM", "Deg") - can be "" for unitless
	int32_t min;			 ///< Minimum expected value (metadata only, not enforced)
	int32_t max;			 ///< Maximum expected value (metadata only, not enforced)
	uint8_t sample_rate_hz;	 ///< Nominal sample rate in Hz (used for sparse logic)
	uint8_t precision;		 ///< Decimal places for formatting (0-9)
} racecapture_channel_t;

/*============================================================================*/
/* LIFECYCLE FUNCTIONS                                                        */
/*============================================================================*/

/**
 * @brief Create a new RaceCapture log file
 * @details Opens a new CSV file for writing. The file is created/truncated if it exists.
 *
 * @param filepath Path to the output CSV file (e.g. "data.csv")
 * @return Handle to the file, or NULL on error
 *
 * @note Call racecapture_file_close() when done to finalize and close the file
 * @warning If file already exists, it will be overwritten
 */
racecapture_file_handle_t racecapture_file_create(const char* filepath);

/**
 * @brief Close and finalize the RaceCapture log file
 * @details Flushes any pending data and closes the file handle.
 *          The handle becomes invalid after this call.
 *
 * @param handle File handle (obtained from racecapture_file_create)
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note Safe to call with NULL handle (returns error)
 * @note After this call, the handle must not be used
 */
racecapture_file_result_t racecapture_file_close(racecapture_file_handle_t handle);

/*============================================================================*/
/* CONFIGURATION FUNCTIONS (must be called before write_header)              */
/*============================================================================*/

/**
 * @brief Add a channel to the log file
 * @details Channels are written in the order they are added.
 *          Typically, Interval and Utc should be added first (system channels).
 *
 * @param handle File handle
 * @param channel Pointer to channel metadata structure
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note Call this function once for each channel before write_header()
 * @note Maximum number of channels is defined in racecapture_file_cfg.h
 * @warning After write_header(), no more channels can be added
 * @warning channel pointer content is copied; original can be freed after this call
 */
racecapture_file_result_t racecapture_file_add_channel(racecapture_file_handle_t handle,
													   const racecapture_channel_t* channel);

/**
 * @brief Write the CSV header line
 * @details Writes the header based on configured channels.
 *          After this, begin writing data rows with begin_row().
 *          Header format: "<Name>"|"<Units>"|min|max|Hz|precision,...
 *
 * @param handle File handle
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note Must be called exactly once, after all add_channel() calls
 * @note After this, no more channels can be added
 * @warning Calling this twice will fail with RACECAPTURE_FILE_ERROR_STATE
 */
racecapture_file_result_t racecapture_file_write_header(racecapture_file_handle_t handle);

/*============================================================================*/
/* DATA WRITING FUNCTIONS (call after write_header)                          */
/*============================================================================*/

/**
 * @brief Begin a new data row
 * @details Prepares for writing channel values for this time step.
 *          After calling this, call write_value() or write_empty() for EACH channel.
 *
 * @param handle File handle
 * @param sample_idx Sequential sample index (0, 1, 2, ...) used for sparse logic
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note sample_idx is used internally to determine if a channel should be written
 *       based on its sample_rate_hz and the base sample rate
 * @note After calling this, you MUST call write_value/write_empty for ALL channels
 * @note Call end_row() when all channel values are written
 *
 * @par Example:
 * @code
 * racecapture_file_begin_row(rc, 0);  // Sample 0
 * racecapture_file_write_value(rc, 0.0);      // Interval
 * racecapture_file_write_value(rc, 1234567.0); // Utc
 * racecapture_file_write_value(rc, 3000.0);   // rpm (10 Hz)
 * racecapture_file_end_row(rc);
 *
 * racecapture_file_begin_row(rc, 1);  // Sample 1
 * racecapture_file_write_value(rc, 100.0);    // Interval
 * racecapture_file_write_value(rc, 1234667.0); // Utc
 * racecapture_file_write_value(rc, 3100.0);   // rpm (written, 10 Hz)
 * racecapture_file_end_row(rc);
 * @endcode
 */
racecapture_file_result_t racecapture_file_begin_row(racecapture_file_handle_t handle, uint64_t sample_idx);

/**
 * @brief Write a value for the current channel
 * @details Writes the value to the current column.
 *          Library decides if field should be empty based on sparse logic.
 *
 * @param handle File handle
 * @param value Channel value (will be formatted according to channel precision)
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note Call this once for each channel, in the same order as add_channel()
 * @note Library automatically applies sparse logic:
 *       - If sample_idx doesn't match channel's Hz, field is written as empty
 *       - System channels (Interval, Utc) are ALWAYS written
 * @warning Must be called between begin_row() and end_row()
 *
 * @par Sparse Logic Example:
 * Base rate: 10 Hz (100 ms period)
 * Channel at 1 Hz: written every 10 samples (0, 10, 20, ...)
 * Channel at 10 Hz: written every sample (0, 1, 2, ...)
 */
racecapture_file_result_t racecapture_file_write_value(racecapture_file_handle_t handle, double value);

/**
 * @brief Write an explicit empty field for the current channel
 * @details Forces an empty field regardless of sparse logic.
 *          Useful for missing data or invalid values.
 *
 * @param handle File handle
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note Use this when you want to explicitly skip a value (e.g. sensor failure)
 * @note This overrides the automatic sparse logic
 * @warning Must be called between begin_row() and end_row()
 */
racecapture_file_result_t racecapture_file_write_empty(racecapture_file_handle_t handle);

/**
 * @brief End the current data row
 * @details Finalizes the row and writes newline.
 *          Validates that all channels have been written.
 *
 * @param handle File handle
 * @return RACECAPTURE_FILE_SUCCESS on success, error code on failure
 *
 * @note Must have called write_value() or write_empty() for ALL channels
 * @warning If not all channels were written, returns RACECAPTURE_FILE_ERROR_CHANNEL_MISMATCH
 */
racecapture_file_result_t racecapture_file_end_row(racecapture_file_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* RACECAPTURE_FILE_H */
