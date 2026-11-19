/**
 * @file r1up_file.h
 * @brief R1UP File Format Reader Library - Public API
 *
 * This header file provides the public interface for reading and parsing
 * proprietary .r1up file format. The library offers a comprehensive set
 * of functions for file operations, data extraction, and format validation.
 *
 * @author Alex Giov
 * @date September 27, 2025
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2025, Race1Up Project.
 * All rights reserved.
 *
 * @par License:
 * This software is proprietary and confidential. Unauthorized copying
 * of this file, via any medium is strictly prohibited.
 *
 * @par Usage Example:
 * @code
 * #include "r1up_file.h"
 *
 * R1upHandle_t handle;
 * R1upResult_t result = R1up_Open("data.r1up", &handle);
 * if (result == R1UP_SUCCESS) {
 *     // Use the handle for file operations
 *     R1up_Close(handle);
 * }
 * @endcode
 */

#ifndef R1UP_FILE_H
#define R1UP_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* INCLUDES                                                                   */
/*============================================================================*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Fixed-point arithmetic library
#include "ag_fixedpoint/ag_fixedpoint.h"

// r1up_file variable and type database
#include "r1up_file/r1up_file_variable_type_database.h"

// r1up_file library configuration
#include "r1up_file_cfg.h"

/*============================================================================*/
/* CONSTANTS AND MACROS                                                       */
/*============================================================================*/

/**
 * @defgroup R1UP_Constants R1UP File Constants
 * @brief Constants and macros for R1UP file operations
 * @{
 */

/** @brief Library version string */
#define R1UP_FILE_VERSION_STRING "1.0.0"

/** @brief Maximum file path length supported */
#define R1UP_FILE_MAX_PATH_LENGTH 260U

/** @brief R1UP file format magic number */
#define R1UP_FILE_MAGIC_NUMBER 0x52315550U /* "R1UP" in hex */

/**
 * @brief Alternative: Generate extern declarations for string constants
 * @details Use this in header files to declare the strings, define them in .c files
 */
#define R1UP_BLOCKS_AS_EXTERN_STRINGS(id_symbol, payload_len_type, payload_len, payload, editable) \
	extern const char id_symbol[R1UP_FILE_BLCK_ID_LENGTH];

R1UP_FILE_BLOCKS_TABLE(R1UP_BLOCKS_AS_EXTERN_STRINGS)

#define R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_ENUM(variable_id, type_id, type, f_divisor, var_name, id_value, ...) \
	variable_id = id_value,
/** @} */ /* End of R1UP_Constants group */

/*============================================================================*/
/* TYPE DEFINITIONS                                                           */
/*============================================================================*/

/**
 * @defgroup R1UP_Types R1UP File Types
 * @brief Type definitions for R1UP file operations
 * @{
 */

/**
 * @brief Domain-specific size type for the R1UP file format.
 *
 * Analogous to size_t for memory, r1up_file_size_t is THE canonical type
 * for every quantity that refers to a position, offset, count, or length
 * within an R1UP file: sample counts, byte offsets, section lengths,
 * buffer sizes.
 *
 * All public API functions that accept or return file-related sizes
 * use this type exclusively. This means:
 *
 *  - Changing uint32_t -> uint64_t here upgrades the ENTIRE library
 *    (internal structs, public API, serialization) in ONE line.
 *  - User code written against r1up_file_size_t recompiles without edits.
 *  - The API contract never changes; only the capacity grows.
 *
 * Current: uint32_t  (format v1, max body ~4 GB)
 * Future:  uint64_t  (format v2, endurance sessions > 4 GB)
 */
typedef uint32_t r1up_file_size_t;

/**
 * @brief Operation result codes for r1up_file library functions
 *
 * This enumeration defines all possible return values from r1up_file library functions.
 * Always check the return value to ensure proper error handling.
 */
typedef enum {
	R1UP_FILE_SUCCESS = 0,			 /**< Operation completed successfully */
	R1UP_FILE_ERROR_INVALID_PARAM,	 /**< Invalid parameter passed to function */
	R1UP_FILE_ERROR_NOT_FOUND,		 /**< Specified file does not exist */
	R1UP_FILE_ERROR_ACCESS,			 /**< File access permissions denied */
	R1UP_FILE_ERROR_INVALID_FORMAT,	 /**< File format is not valid R1UP */
	R1UP_FILE_ERROR_MEMORY,			 /**< Memory allocation failed */
	R1UP_FILE_ERROR_IO,				 /**< Input/output operation failed */
	R1UP_FILE_ERROR_CORRUPTED,		 /**< File data is corrupted */
	R1UP_FILE_ERROR_NOT_INITIALIZED, /**< File structure is not initialized */
	R1UP_FILE_ERROR_INVALID_STATE,	 /**< Operation not allowed in current state */
	R1UP_FILE_ERROR_UNSUPPORTED,	 /**< Unsupported file version or feature */
	R1UP_FILE_ERROR_ALREADY_EXISTS	 /**< Block with same ID already exists */
} r1up_file_result_t;

/**
 * @brief R1UP file access modes
 *
 * These modes define the access level for opening R1UP files.
 */
typedef enum {
	R1UP_FILE_MODE_READ_ONLY = 0, /**< Read-only access mode */
	R1UP_FILE_MODE_WRITE_ONLY,	  /**< Write-only access mode */
	R1UP_FILE_MODE_READ_WRITE,	  /**< Read-write access mode */
	R1UP_FILE_MODE_APPEND		  /**< Append mode */
} r1up_file_mode_t;

/**
 * @brief Opaque handle for R1UP file operations
 *
 * This handle represents an open R1UP file and contains internal state
 * information. Never access the contents directly - use provided API functions.
 */
typedef void* r1up_file_handle_t;

/**
 * @brief Tagged union for raw variable data with runtime type information
 * @details This structure contains a pointer to raw fixed-point buffer and its associated type ID,
 *          enabling type-safe access to variable values without float conversion.
 *
 *          Memory footprint: 12-16 bytes total (platform-dependent)
 *          - 4 bytes: type_id (runtime type discriminator)
 *          - 8 bytes: union pointer (const pointer to buffer)
 *          - 0-4 bytes: padding (compiler-dependent alignment)
 *
 * @note This pattern follows SQLite's Mem structure and Lua's TValue design
 * @note All union members are named as_<type_token> (e.g., as_uq16_0, as_q3_28)
 * @note Access the correct union member based on type_id to avoid undefined behavior
 * @note Pointers reference vsd_read_buffers[] arrays with total_sample_number elements
 *
 * @par Usage Example:
 * @code{.c}
 * r1up_var_raw_t raw;
 * r1up_file_get_var_raw(handle, R1UP_FILE_VAR_ID_RPM, &raw);
 * uint64_t sample_count = r1up_file_get_body_sample_count(handle);
 *
 * switch(raw.type_id) {
 *     case R1UP_FILE_TYPE_ID_UQ16_0:
 *         for (uint64_t i = 0; i < sample_count; i++) {
 *             printf("RPM[%llu] raw: %u\n", i, raw.value.as_uq16_0[i]);
 *             float rpm = ag_fixedpoint_uq16_0_to_float(raw.value.as_uq16_0[i]);
 *         }
 *         break;
 *     case R1UP_FILE_TYPE_ID_UQ32_0:
 *         for (uint64_t i = 0; i < sample_count; i++) {
 *             printf("RPM[%llu] raw: %u\n", i, raw.value.as_uq32_0[i]);
 *             float rpm = ag_fixedpoint_uq32_0_to_float(raw.value.as_uq32_0[i]);
 *         }
 *         break;
 *     default:
 *         return R1UP_FILE_ERROR_INVALID_TYPE;
 * }
 * @endcode
 */
typedef struct {
	r1up_type_id_t type_id; /**< Runtime type discriminator from VSF */
	union {
		/* X-Macro expansion: Generate union members for all 13 fixed-point types */
#define R1UP_TYPE_AS_UNION_MEMBER(id_symbol, id_value, type_token, ...) \
	const R1UP_FILE_TYPE(type_token) * R1UP_FILE_TYPE_AS_UNION_MEMBER(type_token);
		R1UP_FILE_TYPE_ID_TABLE(R1UP_TYPE_AS_UNION_MEMBER)
#undef R1UP_TYPE_AS_UNION_MEMBER
	} value; /**< Pointer to raw fixed-point buffer (all samples) */
} r1up_var_raw_t;

/**
 * @brief Body section metadata with unpacked VSF (Variable Stream Format) information
 * @details Provides read-only access to file body metadata. Memory is managed internally
 *          by the library and remains valid until r1up_file_close() is called.
 *
 *          This structure exposes VSF arrays in unpacked uint32_t format for stability and
 *          future-proofing (current VSF fields are uint8_t). Uses lazy initialization:
 *          first call to r1up_file_get_body_info() allocates internal cache, subsequent
 *          calls return cached data in O(1) time.
 *
 * @note Do not free any pointers in this structure - memory is owned by the library
 * @note All pointers remain valid until r1up_file_close() is called
 * @note First access triggers internal allocation, subsequent accesses are O(1)
 * @note Thread-safe for same handle (idempotent initialization)
 *
 * @par Field Descriptions:
 * - total_samples: Number of data samples in body section
 * - variable_count: Number of variables in VSF (length of all arrays below)
 * - max_variable_id: @deprecated Always 0 — use r1up_file_get_var_position() instead
 * - id_to_position: @deprecated Always NULL — use r1up_file_get_var_position() instead
 * - variable_ids: Array of variable IDs from VSF (length = variable_count)
 * - variable_types: Array of type IDs from VSF (length = variable_count)
 * - data_type_sizes: Array of data sizes in bytes from VSF (length = variable_count)
 * - freq_divisors: Array of frequency divisors from VSF (length = variable_count)
 *
 * @par Usage Example:
 * @code{.c}
 * r1up_file_body_info_t info;
 * if (r1up_file_get_body_info(handle, &info) == R1UP_FILE_SUCCESS) {
 *     printf("Total samples: %llu\n", info.total_samples);
 *     printf("Variable count: %u\n", info.variable_count);
 *
 *     for (uint32_t i = 0; i < info.variable_count; i++) {
 *         printf("Var[%u]: ID=%u Type=%u Size=%u FD=%u\n",
 *                i, info.variable_ids[i], info.variable_types[i],
 *                info.data_type_sizes[i], info.freq_divisors[i]);
 *     }
 *
 *     // Use r1up_file_get_var_position() for ID→position lookup:
 *     int32_t pos = r1up_file_get_var_position(handle, R1UP_FILE_VAR_ID_RPM);
 *     if (pos >= 0) {
 *         printf("RPM is at position %d\n", pos);
 *     }
 * }
 * // No cleanup needed - library manages memory
 * @endcode
 */
typedef struct {
	r1up_file_size_t total_samples; /**< Total number of samples in body section */
	uint32_t variable_count;		/**< Number of variables in VSF array */

	const uint32_t* variable_ids;	 /**< Variable IDs from VSF (length = variable_count) */
	const uint32_t* variable_types;	 /**< Type IDs from VSF (length = variable_count) */
	const uint32_t* data_type_sizes; /**< Data sizes in bytes from VSF (length = variable_count) */
	const uint32_t* freq_divisors;	 /**< Frequency divisors from VSF (length = variable_count) */
} r1up_file_body_info_t;

/** @} */ /* End of R1UP_Types group */

/*============================================================================*/
/* FUNCTION DECLARATIONS                                                      */
/*============================================================================*/

/**
 * @defgroup R1UP_Functions R1UP File Functions
 * @brief Public API functions for R1UP file operations
 * @{
 */

/**
 * @brief Open a Race1UP file with the specified mode.
 * @details This function opens a Race1UP file for reading, writing, or appending based on the specified mode.
 * @param filename The name of the file to open. Must be a valid filename.
 * @param mode The mode to open the file in. Can be one of the following:
 * 	  - R1UP_FILE_MODE_READ_ONLY: Open the file for reading only.
 * 	  - R1UP_FILE_MODE_WRITE_ONLY: Open the file for writing only.
 * 	  - R1UP_FILE_MODE_READ_WRITE: Open the file for reading and writing.
 * 	  - R1UP_FILE_MODE_APPEND: Open the file for appending data.
 * @return A pointer to the file structure, or NULL if an error occurred.
 * @note The returned pointer must be closed using r1up_file_close().
 */
r1up_file_handle_t r1up_file_open(const char* filename, r1up_file_mode_t mode);

/**
 * @brief Close a Race1UP file.
 * @details This function closes a Race1UP file that was previously opened with r1up_file_open().
 *          It releases all resources associated with the file handle.
 * @param handle The file handle to close, obtained from r1up_file_open().
 * @return R1UP_FILE_SUCCESS on successful closure, or an appropriate error code.
 * @note After calling this function, the handle becomes invalid and should not be used.
 * @note This function automatically flushes any pending write operations.
 */
r1up_file_result_t r1up_file_close(r1up_file_handle_t handle);

/**
 * @brief Close the header section of a Race1UP file.
 * @details This function finalizes and closes the header section of a Race1UP file.
 *          It ensures that all header data is properly written and the header is marked as closed.
 * @param handle The file handle whose header section is to be closed, obtained from r1up_file_open().
 * @return R1UP_FILE_SUCCESS on successful closure of the header, or an appropriate error code.
 * @note This function should be called before starting to write body data to the file. If not the function is called
 * internally before writing body data.
 * @note After calling this function, no further modifications to the header are allowed.
 */
r1up_file_result_t r1up_file_close_header(r1up_file_handle_t handle);

/**
 * @brief Set a block payload in a Race1UP file.
 * @details This function sets the payload for a specific block in a Race1UP file.
 * @param handle The file handle obtained from r1up_file_open().
 * @param id_string The ID of the block to set.
 * @param pl Pointer to the block payload data.
 * @param sz The size of the block payload data.
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
r1up_file_result_t r1up_file_set_block_payload(r1up_file_handle_t handle, const char* id_string, void* pl, uint32_t sz);

/**
 * @brief Add a custom block to a Race1UP file.
 * @details This function adds a custom block to a Race1UP file in the specified section.
 * The block ID string is automatically copied into managed memory - you don't need to keep
 * the original string valid after this function returns.
 *
 * @warning CRITICAL SAFETY REQUIREMENT: The section_id parameter MUST point to a buffer containing at least 16
 * readable bytes. The function will read exactly 16 bytes from section_id regardless of null termination.
 * Failure to meet this requirement will result in buffer overrun and undefined behavior.
 *
 * @note Memory Management: The function automatically allocates and manages memory for the block ID.
 * You can safely pass temporary strings or stack-allocated buffers - the function will make its own copy.
 * Memory is automatically freed when you call r1up_file_close().
 *
 * @note This function should NOT be used in production. Custom blocks should be defined in r1up_file_cfg.h using
 * R1UP_FILE_BLOCKS_TABLE. This provides better type safety and compile-time validation.
 *
 * @par Valid Examples:
 * @code{.c}
 * char id[16] = "MY_CUSTOM_BLOCK_";              // Exactly 16 chars - OK
 * char id[] = "MY_CUSTOM_BLOCK_ID_LONG_NAME";    // >16 chars, first 16 used - OK
 * char id[20] = "SHORT";                         // Buffer >=16, content shorter - OK
 *
 * // All of these are now safe - memory is automatically managed:
 * {
 *     char temp_id[16] = "TEMP_BLOCK_____";
 *     r1up_file_add_custom_block(h, R1UP_FILE_HEADER_BID, temp_id, data, size);
 * } // temp_id goes out of scope - but that's OK! Function made its own copy.
 * @endcode
 *
 * @par INVALID Examples (will cause buffer overrun):
 * @code{.c}
 * char id[8] = "SHORT";                          // Buffer too small - DANGEROUS!
 * r1up_file_add_custom_block(h, "SHORT", ...);  // String literal size unknown - DANGEROUS!
 * @endcode
 *
 * @param handle The file handle obtained from r1up_file_open().
 * @param section_id The section ID where the block belongs (header or footer only - body blocks cannot be set).
 *                   MUST be a buffer with at least 16 readable bytes.
 * @param id_string The ID of the block to set (will be copied - you don't need to keep it valid).
 * @param pl Pointer to the block payload data.
 * @param sz The size of the block payload data.
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code (including R1UP_FILE_ERROR_MEMORY if allocation
 * fails).
 */
r1up_file_result_t r1up_file_add_custom_block(r1up_file_handle_t handle, const char* section_id, const char* id_string,
											  void* pl, uint32_t sz);
/**
 * @brief Set a block in a Race1UP file.
 * @details This function set payload for an existing block in a Race1UP file
 * @param handle The file handle obtained from r1up_file_open().
 * @param id_string The ID of the block to set.
 * @param pl Pointer to the block payload data.
 * @param sz The size of the block payload data.
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
r1up_file_result_t r1up_file_set_block(r1up_file_handle_t handle, const char* id_string, void* pl, uint32_t sz);

/**
 * @brief Set a block in a Race1UP file.
 * @details This function sets a block in a Race1UP file, replacing any existing block with the same ID or adding
 * a new block if it does not exist.
 * @param handle The file handle obtained from r1up_file_open().
 * @param id The ID of the block to set.
 * @param pl Pointer to the block payload data.
 * @param sz The size of the block payload data.
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
r1up_file_result_t r1up_file_set_block_by_id_string(r1up_file_handle_t handle, char* id_string, void* pl, uint32_t sz);

/**
 * @brief Retrieve the payload of a block by its ID string from a Race1UP file.
 * @details This function searches for a block with the specified ID string in the Race1UP file and returns a pointer to
 * its payload if found.
 * @param handle The file handle obtained from r1up_file_open().
 * @param id_string The ID string of the block to retrieve.
 * @return A pointer to the block payload if found, or NULL if not found.
 */
void* r1up_file_get_payload_by_id_string(r1up_file_handle_t handle, char* id_string);

/**
 * @brief Retrieve the payload of a block by its BID (block id).
 * @details This function searches for a block with the specified ID string in the Race1UP file and returns a pointer to
 * its payload if found.
 * @param handle The file handle obtained from r1up_file_open().
 * @param block_id The ID of the block (BID) to retrieve.
 * @return A pointer to the block payload if found, or NULL if not found.
 */
void* r1up_file_get_payload(r1up_file_handle_t handle, const char* id_string);

/**
 * @brief Update the variables in the Race1UP file.
 * @details This function updates the variable data in the Race1UP file based on the current sample number
 * and the defined Variables Stream Format (VSF). It ensures that the header is closed before updating
 * and writes the variable data to the file as needed.
 * @param handle The file handle obtained from r1up_file_open().
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
r1up_file_result_t r1up_file_update_vars(r1up_file_handle_t handle);

/**
 * @brief Set the value of a variable in the Race1UP file.
 * @details This function sets the value of a specified variable in the Race1UP file by copying the provided data
 * into the internal write buffer for that variable. The variable is identified by its ID from the Variables Stream
 * Format (VSF).
 * @param handle The file handle obtained from r1up_file_open().
 * @param var_id The ID of the variable to set.
 * @param data Pointer to the data to set for the variable.
 * @return void
 */
void r1up_file_set_var(r1up_file_handle_t handle, r1up_variable_id_t var_id, const void* data);

/**
 * @brief Request a lap boundary marker at the current sample position.
 *
 * Called by the application's lap timing algorithm when the finish line is crossed.
 * This function ONLY raises an internal flag (lap_mark_pending). The actual marker
 * recording (sample_index, file_offset, realloc) is performed by r1up_file_update_vars()
 * on the next call, ensuring perfect data synchronization.
 *
 * This function is ISR-safe: it performs no I/O, no allocation, no locking.
 * The flag write must be atomic on the target platform (see design doc).
 *
 * If called multiple times before the next update_vars(), only ONE marker is recorded
 * (the flag is idempotent). This is correct because multiple finish line crossings
 * within a single sample period are physically impossible at racing speeds.
 *
 * @param handle  Valid file handle opened in WRITE mode
 * @return R1UP_FILE_SUCCESS, or error if handle is NULL / wrong mode / body already closed
 */
r1up_file_result_t r1up_file_mark_lap(r1up_file_handle_t handle);

/**
 * @brief Get the number of complete laps recorded in the file.
 *
 * @details A complete lap is a segment between two consecutive finish line crossings.
 *          This excludes the outlap (start → first crossing) and inlap (last crossing → end).
 *          Returns 0 when fewer than 2 crossings were recorded.
 *
 *          Relationship:  lap_count = crossing_count - 1  (when crossing_count >= 2, else 0)
 *
 *          Use r1up_file_get_lap_sample_range() or r1up_file_get_lap_offset_range() with
 *          lap_number in [0 … lap_count + 1] to access outlap (0), complete laps (1…lap_count),
 *          and inlap (lap_count + 1).
 *
 * @param handle     Valid file handle (READ or WRITE mode)
 * @param[out] lap_count  Pointer to receive the number of complete laps
 * @return R1UP_FILE_SUCCESS, or error if handle/lap_count is NULL
 */
r1up_file_result_t r1up_file_get_lap_count(r1up_file_handle_t handle, uint32_t* lap_count);

/**
 * @brief Get the sample index range for a given lap number.
 *
 * @details Returns the first and last sample indices (inclusive) for the requested segment:
 *          - lap_number 0:          outlap  (sample 0 … first crossing - 1)
 *          - lap_number 1…lap_count: complete lap N
 *          - lap_number lap_count+1: inlap  (last crossing … total_samples - 1)
 *
 * @param handle               Valid file handle
 * @param lap_number           Segment index: 0 = outlap, 1…lap_count = complete laps, lap_count+1 = inlap
 * @param[out] start_sample_index  First sample of the segment (inclusive)
 * @param[out] end_sample_index    Last sample of the segment (inclusive)
 * @return R1UP_FILE_SUCCESS, R1UP_FILE_ERROR_NOT_FOUND if no crossings, or R1UP_FILE_ERROR_INVALID_PARAM
 */
r1up_file_result_t r1up_file_get_lap_sample_range(r1up_file_handle_t handle, uint32_t lap_number,
												  r1up_file_size_t* start_sample_index,
												  r1up_file_size_t* end_sample_index);

/**
 * @brief Get the file offset range for a given lap number.
 *
 * @details Returns the first and last byte offsets (inclusive) of body data for the requested segment.
 *          Useful for fast seek-based access to lap data without scanning the entire body.
 *
 * @param handle               Valid file handle
 * @param lap_number           Segment index: 0 = outlap, 1…lap_count = complete laps, lap_count+1 = inlap
 * @param[out] start_file_offset   First byte offset in file for this segment (inclusive)
 * @param[out] end_file_offset     Last byte offset in file for this segment (inclusive)
 * @return R1UP_FILE_SUCCESS, R1UP_FILE_ERROR_NOT_FOUND if no crossings, or R1UP_FILE_ERROR_INVALID_PARAM
 */
r1up_file_result_t r1up_file_get_lap_offset_range(r1up_file_handle_t handle, uint32_t lap_number,
												  r1up_file_size_t* start_file_offset,
												  r1up_file_size_t* end_file_offset);

/**
 * @brief Get the number of variables in the file.
 * @details This function returns the number of variables defined in the Variable Stream Format (VSF)
 * for the opened R1UP file. This value is valid only after successfully opening the file.
 * @param handle The file handle obtained from r1up_file_open().
 * @return The number of variables, or 0 if handle is invalid or file not opened.
 */
uint32_t r1up_file_get_variable_number(r1up_file_handle_t handle);

/**
 * @brief Read the body section of a Race1UP file.
 * @details This function reads and processes the body section of a Race1UP file.
 * It extracts variable stream data and other relevant information from the body.
 * @param handle The file handle obtained from r1up_file_open().
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
r1up_file_result_t r1up_file_read_body(r1up_file_handle_t handle);

/*============================================================================*/
/* GPS CONVERSION HELPER FUNCTIONS                                           */
/*============================================================================*/

/**
 * @brief Convert R1UP GPS latitude format to decimal degrees
 * @details Implements conversion from R1UP GPS format (deg + min + deci) to decimal degrees.
 *          Includes workaround for R1UP file format bug where negative coordinates
 *          are encoded with negative minutes instead of negative degrees.
 *
 *          Formula:
 *          if min < 0: decimal = -(deg + (abs(min) + deci/10000) / 60)
 *          else:       decimal = deg + (abs(min) + deci/10000) / 60
 *
 * @param deg Degrees component (LAT_DEG) [0..90]
 * @param min Minutes component (LAT_MIN) [-60..60] - can be negative (bug workaround)
 * @param deci Decimal minutes component (LAT_DECI) [0..9999]
 * @return GPS latitude in decimal degrees format (DD.DDDDDD) [-90.0 to +90.0]
 *
 * @note This function handles the R1UP file format bug where negative coordinates
 *       use negative minutes instead of negative degrees
 * @note Based on MATLAB conversion script from Race1Up project
 *
 * @par Example:
 * @code
 * // Positive latitude: 45°30'30" = 45.5083°
 * uint8_t deg = 45;
 * int8_t min = 30;
 * uint16_t deci = 5000;
 * ag_fixedpoint_float_t lat = r1up_file_convert_gps_latitude_from_raw(deg, min, deci); // 45.5083
 *
 * // Negative latitude with bug workaround: 45°-30'30" = -45.5083°
 * uint8_t deg = 45;
 * int8_t min = -30;
 * uint16_t deci = 5000;
 * ag_fixedpoint_float_t lat = r1up_file_convert_gps_latitude_from_raw(deg, min, deci); // -45.5083
 * @endcode
 */
ag_fixedpoint_float_t r1up_file_convert_gps_latitude_from_raw(uint8_t deg, int8_t min, uint16_t deci);

/**
 * @brief Convert R1UP GPS longitude format to decimal degrees
 * @details Implements conversion from R1UP GPS format (deg + min + deci) to decimal degrees.
 *          Includes workaround for R1UP file format bug where negative coordinates
 *          are encoded with negative minutes instead of negative degrees.
 *
 *          Formula:
 *          if min < 0: decimal = -(deg + (abs(min) + deci/10000) / 60)
 *          else:       decimal = deg + (abs(min) + deci/10000) / 60
 *
 * @param deg Degrees component (LONG_DEG) [0..180]
 * @param min Minutes component (LONG_MIN) [-60..60] - can be negative (bug workaround)
 * @param deci Decimal minutes component (LONG_DECI) [0..9999]
 * @return GPS longitude in decimal degrees format (DDD.DDDDDD) [-180.0 to +180.0]
 *
 * @note This function handles the R1UP file format bug where negative coordinates
 *       use negative minutes instead of negative degrees
 * @note Based on MATLAB conversion script from Race1Up project
 *
 * @par Example:
 * @code
 * // Positive longitude: 12°30'30" = 12.5083°
 * uint8_t deg = 12;
 * int8_t min = 30;
 * uint16_t deci = 5000;
 * ag_fixedpoint_float_t lon = r1up_file_convert_gps_longitude_from_raw(deg, min, deci); // 12.5083
 *
 * // Negative longitude with bug workaround: 12°-30'30" = -12.5083°
 * uint8_t deg = 12;
 * int8_t min = -30;
 * uint16_t deci = 5000;
 * ag_fixedpoint_float_t lon = r1up_file_convert_gps_longitude_from_raw(deg, min, deci); // -12.5083
 * @endcode
 */
ag_fixedpoint_float_t r1up_file_convert_gps_longitude_from_raw(uint8_t deg, int8_t min, uint16_t deci);

/**
 * @brief Convert GPS latitude from float components to decimal degrees
 * @details Same conversion as r1up_file_convert_gps_latitude_from_raw but accepts float parameters.
 *          Useful when working with already-converted float values.
 *
 * @param deg Degrees component as float [0.0..90.0]
 * @param min Minutes component as float [-60.0..60.0] - can be negative
 * @param deci Decimal minutes component as float [0.0..9999.0]
 * @return GPS latitude in decimal degrees format (DD.DDDDDD) [-90.0 to +90.0]
 *
 * @see r1up_file_convert_gps_latitude_from_raw
 */
ag_fixedpoint_float_t r1up_file_convert_gps_latitude(ag_fixedpoint_float_t deg, ag_fixedpoint_float_t min,
													 ag_fixedpoint_float_t deci);

/**
 * @brief Convert GPS longitude from float components to decimal degrees
 * @details Same conversion as r1up_file_convert_gps_longitude_from_raw but accepts float parameters.
 *          Useful when working with already-converted float values.
 *
 * @param deg Degrees component as float [0.0..180.0]
 * @param min Minutes component as float [-60.0..60.0] - can be negative
 * @param deci Decimal minutes component as float [0.0..9999.0]
 * @return GPS longitude in decimal degrees format (DDD.DDDDDD) [-180.0 to +180.0]
 *
 * @see r1up_file_convert_gps_longitude_from_raw
 */
ag_fixedpoint_float_t r1up_file_convert_gps_longitude(ag_fixedpoint_float_t deg, ag_fixedpoint_float_t min,
													  ag_fixedpoint_float_t deci);

/**
 * @brief Get GPS latitude in decimal degrees for all samples
 * @details Retrieves and converts GPS latitude from the three component variables
 *          (LAT_DEG, LAT_MIN, LAT_DECI) to decimal degrees format for all samples.
 *          Internally uses r1up_file_convert_gps_latitude() for conversion.
 *
 *          This function requires that r1up_file_read_body() has been called and
 *          that the file contains all three latitude component variables.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param out_lat_dd User-allocated array to receive latitude values in decimal degrees
 * @param buffer_size Size of the output array (must be >= total_sample_number)
 * @return R1UP_FILE_SUCCESS - All latitude values converted successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle, null output, or buffer too small
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - Body not yet read (call r1up_file_read_body first)
 *         R1UP_FILE_ERROR_NOT_FOUND - One or more GPS latitude components not present in file
 *
 * @note Requires prior call to r1up_file_read_body()
 * @note File must contain LAT_DEG, LAT_MIN, and LAT_DECI variables
 *
 * @par Example:
 * @code{.c}
 * r1up_file_size_t count = r1up_file_get_body_sample_count(handle);
 * ag_fixedpoint_float_t* lat = malloc(count * sizeof(ag_fixedpoint_float_t));
 * if (r1up_file_get_gps_latitude_dd(handle, lat, count) == R1UP_FILE_SUCCESS) {
 *     for (size_t i = 0; i < count; i++) {
 *         printf("Sample %zu: %.6f\n", i, lat[i]);
 *     }
 * }
 * free(lat);
 * @endcode
 *
 * @see r1up_file_get_gps_longitude_dd
 * @see r1up_file_convert_gps_latitude
 */
r1up_file_result_t r1up_file_get_gps_latitude_dd(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_lat_dd,
												 r1up_file_size_t buffer_size);

/**
 * @brief Get GPS longitude in decimal degrees for all samples
 * @details Retrieves and converts GPS longitude from the three component variables
 *          (LONG_DEG, LONG_MIN, LONG_DECI) to decimal degrees format for all samples.
 *          Internally uses r1up_file_convert_gps_longitude() for conversion.
 *
 *          This function requires that r1up_file_read_body() has been called and
 *          that the file contains all three longitude component variables.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param out_lon_dd User-allocated array to receive longitude values in decimal degrees
 * @param buffer_size Size of the output array (must be >= total_sample_number)
 * @return R1UP_FILE_SUCCESS - All longitude values converted successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle, null output, or buffer too small
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - Body not yet read (call r1up_file_read_body first)
 *         R1UP_FILE_ERROR_NOT_FOUND - One or more GPS longitude components not present in file
 *
 * @note Requires prior call to r1up_file_read_body()
 * @note File must contain LONG_DEG, LONG_MIN, and LONG_DECI variables
 *
 * @par Example:
 * @code{.c}
 * r1up_file_size_t count = r1up_file_get_body_sample_count(handle);
 * ag_fixedpoint_float_t* lon = malloc(count * sizeof(ag_fixedpoint_float_t));
 * if (r1up_file_get_gps_longitude_dd(handle, lon, count) == R1UP_FILE_SUCCESS) {
 *     for (size_t i = 0; i < count; i++) {
 *         printf("Sample %zu: %.6f\n", i, lon[i]);
 *     }
 * }
 * free(lon);
 * @endcode
 *
 * @see r1up_file_get_gps_latitude_dd
 * @see r1up_file_convert_gps_longitude
 */
r1up_file_result_t r1up_file_get_gps_longitude_dd(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_lon_dd,
												  r1up_file_size_t buffer_size);

/**
 * @brief Get GPS latitude in decimal degrees for a specific lap
 * @details Retrieves and converts GPS latitude from the three component variables
 *          (LAT_DEG, LAT_MIN, LAT_DECI) to decimal degrees format for samples
 *          belonging to the specified lap segment.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param lap_number Lap segment number (0 = samples before first crossing, 1 = first lap, etc.)
 * @param out_lat_dd User-allocated array to receive latitude values in decimal degrees
 * @param buffer_size Size of the output array
 * @param out_sample_count Optional pointer to receive actual sample count for this lap (can be NULL)
 * @return R1UP_FILE_SUCCESS - All latitude values converted successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid params or buffer too small
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - Body not yet read
 *         R1UP_FILE_ERROR_NOT_FOUND - GPS components or lap not found
 *
 * @see r1up_file_get_gps_latitude_dd
 * @see r1up_file_get_var_float_by_lap
 */
r1up_file_result_t r1up_file_get_gps_latitude_dd_by_lap(r1up_file_handle_t handle, uint32_t lap_number,
														ag_fixedpoint_float_t* out_lat_dd, r1up_file_size_t buffer_size,
														r1up_file_size_t* out_sample_count);

/**
 * @brief Get GPS longitude in decimal degrees for a specific lap
 * @details Retrieves and converts GPS longitude from the three component variables
 *          (LONG_DEG, LONG_MIN, LONG_DECI) to decimal degrees format for samples
 *          belonging to the specified lap segment.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param lap_number Lap segment number (0 = samples before first crossing, 1 = first lap, etc.)
 * @param out_lon_dd User-allocated array to receive longitude values in decimal degrees
 * @param buffer_size Size of the output array
 * @param out_sample_count Optional pointer to receive actual sample count for this lap (can be NULL)
 * @return R1UP_FILE_SUCCESS - All longitude values converted successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid params or buffer too small
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - Body not yet read
 *         R1UP_FILE_ERROR_NOT_FOUND - GPS components or lap not found
 *
 * @see r1up_file_get_gps_longitude_dd
 * @see r1up_file_get_var_float_by_lap
 */
r1up_file_result_t r1up_file_get_gps_longitude_dd_by_lap(r1up_file_handle_t handle, uint32_t lap_number,
														 ag_fixedpoint_float_t* out_lon_dd,
														 r1up_file_size_t buffer_size,
														 r1up_file_size_t* out_sample_count);

/**
 * @brief Get total number of samples in file body
 * @details Returns the total number of samples (rows) available in the R1UP file body.
 *          This count is based on the maximum sample rate variable (typically RPM).
 * @param handle File handle obtained from r1up_file_open()
 * @return Total sample count, or 0 if error or file not read
 * @note File must be opened and metadata initialized (after open or read_body)
 */
r1up_file_size_t r1up_file_get_body_sample_count(r1up_file_handle_t handle);

/**
 * @brief Get body metadata with unpacked VSF (Variable Stream Format) information
 * @details Provides read-only access to file body metadata and unpacked VSF arrays.
 *          Uses lazy initialization: first call allocates internal cache, subsequent
 *          calls return cached data in O(1) time.
 *
 *          All memory is managed internally and remains valid until r1up_file_close()
 *          is called. No user cleanup required.
 *
 * @param handle File handle from r1up_file_open()
 * @param info User-allocated structure to receive metadata pointers (stack allocation)
 * @return R1UP_FILE_SUCCESS - Metadata retrieved successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle or null info pointer
 *         R1UP_FILE_ERROR_MEMORY - Failed to allocate cache arrays (first call only)
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - File not opened or metadata not loaded
 *
 * @note First call allocates var_count * 4 * sizeof(uint32_t) bytes internally
 * @note Subsequent calls are O(1) with no allocations
 * @note File must be opened and header parsed before calling
 * @note All pointers in info remain valid until r1up_file_close()
 * @note Do not free any pointers from info structure - memory is library-owned
 *
 * @par Performance:
 * - First call: O(n) where n = variable_count (unpacking VSF)
 * - Subsequent calls: O(1) (returns cached pointers)
 * - Memory overhead: 16 bytes per variable (4 uint32_t arrays)
 *
 * @par Usage Example:
 * @code{.c}
 * r1up_file_body_info_t info;
 * if (r1up_file_get_body_info(handle, &info) == R1UP_FILE_SUCCESS) {
 *     printf("Total samples: %llu\n", info.total_samples);
 *     printf("Variables: %u\n", info.variable_count);
 *
 *     for (uint32_t i = 0; i < info.variable_count; i++) {
 *         printf("Var[%u]: ID=%u Type=%u Size=%u FD=%u\n",
 *                i, info.variable_ids[i], info.variable_types[i],
 *                info.data_type_sizes[i], info.freq_divisors[i]);
 *     }
 * }
 * // No cleanup - library manages memory
 * @endcode
 */
r1up_file_result_t r1up_file_get_body_info(r1up_file_handle_t handle, r1up_file_body_info_t* info);

/**
 * @brief Get raw variable data without float conversion
 * @details Returns the raw fixed-point value for the specified variable along with its
 *          runtime type information. Provides type-safe access to the internal buffer
 *          without automatic conversion to float.
 *
 *          The function reads from the internal vsd_read_buffers and packages the data
 *          in a tagged union structure for type-safe access.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param var_id Variable ID from r1up_file_variable_type_database.h (e.g., R1UP_FILE_VAR_ID_RPM)
 * @param out_raw User-allocated structure to receive raw data + type tag (8 bytes)
 * @return R1UP_FILE_SUCCESS - Data retrieved successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle or null out_raw pointer
 *         R1UP_FILE_ERROR_NOT_FOUND - Variable ID not present in file's VSF
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - File body not read yet
 *         R1UP_FILE_ERROR_CORRUPTED - Unknown type ID (should never happen)
 *
 * @note File must have been read with r1up_file_read_body() before calling this function
 * @note The type_id in out_raw indicates which union member is valid
 * @note Zero allocations - 8 bytes on stack
 * @note Returns CURRENT VALUE from last read/stream operation (not single-sample read)
 *
 * @warning Always check type_id before accessing union members to avoid undefined behavior
 *
 * @par Usage Example (RPM can be stored as different types):
 * @code{.c}
 * r1up_var_raw_t raw;
 * r1up_file_result_t res = r1up_file_get_var_raw(handle, R1UP_FILE_VAR_ID_RPM, &raw);
 *
 * if (res == R1UP_FILE_SUCCESS) {
 *     switch(raw.type_id) {
 *         case R1UP_FILE_TYPE_ID_UQ16_0:
 *             printf("RPM (UQ16.0): %u raw = %.2f converted\n",
 *                    raw.value.as_uq16_0,
 *                    ag_fixedpoint_uq16_0_to_float(raw.value.as_uq16_0));
 *             break;
 *
 *         case R1UP_FILE_TYPE_ID_UQ32_0:
 *             printf("RPM (UQ32.0): %u raw = %.2f converted\n",
 *                    raw.value.as_uq32_0,
 *                    ag_fixedpoint_uq32_0_to_float(raw.value.as_uq32_0));
 *             break;
 *
 *         default:
 *             printf("Unexpected type ID: %d\n", raw.type_id);
 *             return R1UP_FILE_ERROR_CORRUPTED;
 *     }
 * }
 * @endcode
 *
 * @par Use Cases:
 * - RaceCapture file conversion (raw data export)
 * - Binary logging without conversion overhead
 * - Type inspection and validation
 * - Custom fixed-point processing
 */
r1up_file_result_t r1up_file_get_var_raw(r1up_file_handle_t handle, r1up_variable_id_t var_id, r1up_var_raw_t* out_raw);

/**
 * @brief Get the type ID of a variable from the file's VSF
 * @details Queries the Variable Stream Format (VSF) block to determine the runtime type
 *          of a specific variable in this file. Essential for generic read operations
 *          where variable-to-type mapping is not known at compile time.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param var_id Variable ID to query (from R1UP_FILE_VARIABLE_ID_TABLE)
 * @param out_type_id Pointer to store the type ID result
 * @return R1UP_FILE_SUCCESS - Type ID retrieved successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle or null pointer
 *         R1UP_FILE_ERROR_NOT_FOUND - Variable not present in this file's VSF
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - File VSF not loaded yet
 *
 * @note File must be opened before calling this function (VSF loaded during open)
 * @note For write mode files, returns the configured type from r1up_file_cfg.h
 * @note For read mode files, returns the actual type stored in the file's VSF block
 *
 * @par Usage Example:
 * @code{.c}
 * r1up_type_id_t type_id;
 * r1up_file_result_t res = r1up_file_get_var_type_id(handle, R1UP_FILE_VAR_ID_RPM, &type_id);
 * if (res == R1UP_FILE_SUCCESS) {
 *     // Now we know RPM's actual type in this file
 *     switch(type_id) {
 *         case R1UP_FILE_TYPE_ID_UQ16_0: // Handle as uint16_t
 *             break;
 *         case R1UP_FILE_TYPE_ID_UQ32_0: // Handle as uint32_t
 *             break;
 *         // ...
 *     }
 * }
 * @endcode
 */
r1up_file_result_t r1up_file_get_var_type_id(r1up_file_handle_t handle, r1up_variable_id_t var_id,
											 r1up_type_id_t* out_type_id);

/** * @brief Get variable data as floating-point array with buffer size validation
 * @details Retrieves ALL samples of a variable from file's read buffers and converts them
 *          to floating-point format. This is the recommended interface for extracting
 *          variable data in a type-safe, convenient format with buffer overflow protection.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param var_id Variable ID from r1up_file_variable_type_database.h (e.g., R1UP_FILE_VAR_ID_RPM)
 * @param out_float User-allocated buffer to receive converted float data
 * @param buffer_size Number of ag_fixedpoint_float_t elements that out_float can hold
 * @return R1UP_FILE_SUCCESS - Data converted and copied successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle, null pointer, or buffer too small
 *         R1UP_FILE_ERROR_NOT_FOUND - Variable ID not present in file's VSF
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - File body not read yet
 *         R1UP_FILE_ERROR_CORRUPTED - Unknown type ID (should never happen)
 *
 * @note File must have been read with r1up_file_read_body() before calling this function
 * @note buffer_size must be >= r1up_file_get_body_sample_count(handle)
 * @note Converts ALL samples from fixed-point to floating-point format
 * @note Uses ag_fixedpoint library for type-correct conversions
 *
 * @par Usage Example:
 * @code{.c}
 * // 1. Get total number of samples
 * uint64_t sample_count = r1up_file_get_body_sample_count(handle);
 *
 * // 2. Allocate buffer with correct size
 * ag_fixedpoint_float_t* rpm_data = malloc(sample_count * sizeof(ag_fixedpoint_float_t));
 * if (rpm_data == NULL) {
 *     // Handle allocation failure
 * }
 *
 * // 3. Get float-converted data with size validation
 * r1up_file_result_t res = r1up_file_get_var_float(handle,
 *                                                   R1UP_FILE_VAR_ID_RPM,
 *                                                   rpm_data,
 *                                                   sample_count);
 *
 * if (res == R1UP_FILE_SUCCESS) {
 *     // Process float data
 *     for (uint64_t i = 0; i < sample_count; i++) {
 *         printf("Sample %llu: RPM = %.2f\n", i, rpm_data[i]);
 *     }
 * } else if (res == R1UP_FILE_ERROR_INVALID_PARAM) {
 *     printf("Buffer too small!\n");
 * }
 *
 * free(rpm_data);
 * @endcode
 *
 * @par Buffer Size Safety:
 * The buffer_size parameter prevents buffer overflow by verifying the caller
 * has allocated sufficient space. This follows standard C library patterns
 * (e.g., snprintf, strncpy) for safer memory operations.
 */
r1up_file_result_t r1up_file_get_var_float(r1up_file_handle_t handle, r1up_variable_id_t var_id,
										   ag_fixedpoint_float_t* out_float, r1up_file_size_t buffer_size);

/**
 * @brief Get the float-converted variable data for a single lap (or outlap/inlap).
 *
 * @details Extracts the samples belonging to one lap segment and converts them from the internal
 *          fixed-point representation to floating-point. This is a convenience wrapper that combines
 *          r1up_file_get_lap_sample_range() with the type-conversion logic of r1up_file_get_var_float().
 *
 *          The body must have been read first via r1up_file_read_body().
 *
 *          Segment numbering (same as r1up_file_get_lap_sample_range):
 *          - lap_number 0            : outlap  (sample 0 … first crossing - 1)
 *          - lap_number 1 … lap_count: complete lap N
 *          - lap_number lap_count + 1: inlap   (last crossing … total_samples - 1)
 *
 * @param handle              Valid file handle opened in READ mode with body already read
 * @param var_id              Variable identifier (semantic ID, not position)
 * @param lap_number          Segment index: 0 = outlap, 1…lap_count = complete laps, lap_count+1 = inlap
 * @param[out] out_float      Caller-allocated buffer to receive float-converted samples
 * @param buffer_size         Number of elements allocated in out_float (must be >= samples in lap)
 * @param[out] out_sample_count  Pointer to receive the actual number of samples written to out_float.
 *                               May be NULL if the caller does not need this information.
 *
 * @return R1UP_FILE_SUCCESS on success
 * @retval R1UP_FILE_ERROR_INVALID_PARAM  NULL handle/out_float, invalid var_id, buffer too small, or bad lap_number
 * @retval R1UP_FILE_ERROR_NOT_INITIALIZED Body not read (vsd_read_buffers == NULL)
 * @retval R1UP_FILE_ERROR_NOT_FOUND      Variable not present in file, or no lap crossings recorded
 * @retval R1UP_FILE_ERROR_CORRUPTED      Unknown variable type in VSF (should never happen)
 *
 * @pre r1up_file_read_body() must have been called on this handle.
 * @pre At least one lap crossing must have been recorded (see r1up_file_get_lap_count).
 *
 * @par Example:
 * @code
 * // Get lap 2 data for RPM
 * uint32_t sample_count = 0;
 * uint64_t start, end;
 * r1up_file_get_lap_sample_range(handle, 2, &start, &end);
 * uint32_t max_samples = (uint32_t)(end - start + 1U);
 * ag_fixedpoint_float_t* rpm_lap2 = malloc(max_samples * sizeof(ag_fixedpoint_float_t));
 *
 * r1up_file_result_t res = r1up_file_get_var_float_by_lap(
 *     handle, R1UP_FILE_VAR_ID_RPM, 2, rpm_lap2, max_samples, &sample_count);
 *
 * if (res == R1UP_FILE_SUCCESS) {
 *     for (uint32_t i = 0; i < sample_count; i++) {
 *         printf("Lap2 sample %u: RPM = %.2f\n", i, rpm_lap2[i]);
 *     }
 * }
 * free(rpm_lap2);
 * @endcode
 */
r1up_file_result_t r1up_file_get_var_float_by_lap(r1up_file_handle_t handle, r1up_variable_id_t var_id,
												  uint32_t lap_number, ag_fixedpoint_float_t* out_float,
												  r1up_file_size_t buffer_size, r1up_file_size_t* out_sample_count);

/**
 * @brief Get the array position for a given variable ID
 * @details Converts a variable ID to its position index in internal arrays (VSF, vsd_read_buffers).
 *          Works with sparse 32-bit IDs using an internal linear scan over VSF entries.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param var_id Variable identifier to lookup
 *
 * @return Position index (0 to var_count-1) if variable exists in file, -1 if not found or error
 *
 * @note This function is essential for converting semantic variable IDs to physical array positions
 * @note Variable IDs can be non-sequential (e.g., [0, 5, 17]), but positions are always [0, 1, 2]
 * @note The returned position can be used to index VSF arrays and data buffers
 *
 * @par Example:
 * @code
 * // File contains variables with IDs [0, 5, 17]
 * int32_t pos = r1up_file_get_var_position(handle, 5);  // Returns 1 (second position)
 * if (pos >= 0) {
 *     // Use pos to access internal arrays
 * }
 * @endcode
 */
int32_t r1up_file_get_var_position(r1up_file_handle_t handle, r1up_variable_id_t var_id);

/**
 * @brief Get the basename (filename without path) of the currently opened file
 * @details Returns a pointer to the filename without directory path.
 *          Uses standard POSIX basename semantics.
 *          For example, if file was opened with "path/to/data.r1up", returns "data.r1up".
 *          The returned pointer remains valid until r1up_file_close() is called.
 * @param handle The file handle obtained from r1up_file_open()
 * @return Pointer to null-terminated basename string, or NULL if handle is invalid
 * @note Do not modify or free the returned string - it is owned by the library
 * @note The returned pointer becomes invalid after calling r1up_file_close()
 * @see r1up_file_get_pathname() to get the complete path
 *
 * @par Usage Example:
 * @code{.c}
 * r1up_file_handle_t handle = r1up_file_open("path/to/data.r1up", R1UP_FILE_MODE_READ_ONLY);
 * if (handle) {
 *     const char* name = r1up_file_get_basename(handle);
 *     printf("Basename: %s\n", name);  // Prints: "data.r1up"
 *     r1up_file_close(handle);
 * }
 * @endcode
 */
const char* r1up_file_get_basename(r1up_file_handle_t handle);

/**
 * @brief Get the full pathname (including directory) of the currently opened file
 * @details Returns a pointer to the complete file path string stored internally.
 *          For example, if file was opened with "path/to/data.r1up", returns "path/to/data.r1up".
 *          The returned pointer remains valid until r1up_file_close() is called.
 * @param handle The file handle obtained from r1up_file_open()
 * @return Pointer to null-terminated pathname string, or NULL if handle is invalid
 * @note Do not modify or free the returned string - it is owned by the library
 * @note The returned pointer becomes invalid after calling r1up_file_close()
 * @see r1up_file_get_basename() to get only the filename without path
 *
 * @par Usage Example:
 * @code{.c}
 * r1up_file_handle_t handle = r1up_file_open("path/to/data.r1up", R1UP_FILE_MODE_READ_ONLY);
 * if (handle) {
 *     const char* pathname = r1up_file_get_pathname(handle);
 *     printf("Pathname: %s\n", pathname);  // Prints: "path/to/data.r1up"
 *     r1up_file_close(handle);
 * }
 * @endcode
 */
const char* r1up_file_get_pathname(r1up_file_handle_t handle);

/** * @brief Initialize streaming mode for body reading (low memory mode)
 * @details Prepares the file for row-by-row sequential reading of body data.
 *          Allocates internal sample-and-hold buffer and positions file pointer
 *          at the start of the body section. This mode is designed for embedded
 *          systems with limited RAM where full body allocation is not feasible.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @return R1UP_FILE_SUCCESS - Streaming initialized successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle
 *         R1UP_FILE_ERROR_MEMORY - Failed to allocate sample-and-hold buffer
 *         R1UP_FILE_ERROR_INVALID_STATE - File not properly opened/initialized
 *
 * @note Must be called AFTER r1up_file_open() and BEFORE r1up_file_stream_body_next_row()
 * @note NOT compatible with r1up_file_read_body() - use ONE mode only
 * @note Automatically called cleanup on r1up_file_close()
 *
 * @warning Do not mix streaming mode with batch mode (read_body + get_xxx functions)
 */
r1up_file_result_t r1up_file_stream_body_begin(r1up_file_handle_t handle);

/**
 * @brief Read next complete sample row with automatic sample-and-hold
 * @details Reads the next row (sample) from the R1UP file body, applying sample-and-hold
 *          logic for variables with frequency divisors. Each call returns a complete row
 *          with all variables converted to double precision floating point.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param out_values Output array for converted values (must be sized >= var_count)
 * @param array_size Size of provided output array (safety check)
 * @return R1UP_FILE_SUCCESS - Row read successfully
 *         R1UP_FILE_ERROR_EOF - End of body reached (no more samples)
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle or array too small
 *         R1UP_FILE_ERROR_NOT_INITIALIZED - stream_body_begin() not called
 *         R1UP_FILE_ERROR_IO - File read error
 *
 * @note Automatically manages frequency divisors (fd):
 *       - If variable is sampled this row: reads new value
 *       - If variable is NOT sampled: returns previous value (sample-and-hold)
 * @note First sample (index 0) always contains valid values for ALL variables
 * @note GPS latitude/longitude components are returned as raw values (deg, min, deci)
 *       Use r1up_file_convert_gps_latitude/longitude() for decimal degrees conversion
 *
 * @par Example:
 * @code
 * uint32_t var_count = r1up_file_get_variable_number(handle);
 * double* row = malloc(var_count * sizeof(double));
 *
 * r1up_file_stream_body_begin(handle);
 *
 * while (r1up_file_stream_body_next_row(handle, row, var_count) == R1UP_FILE_SUCCESS) {
 *     // Process row[0..var_count-1]
 *     double rpm = row[R1UP_FILE_VAR_ID_RPM];
 *     double latitude = r1up_file_convert_gps_latitude(
 *         (uint8_t)row[R1UP_FILE_VAR_ID_LAT_DEG],
 *         (int8_t)row[R1UP_FILE_VAR_ID_LAT_MIN],
 *         (uint16_t)row[R1UP_FILE_VAR_ID_LAT_DECI]
 *     );
 * }
 * @endcode
 */
r1up_file_result_t r1up_file_stream_body_next_row(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_values,
												  r1up_file_size_t array_size);

/**
 * @brief Cleanup streaming mode resources
 * @details Frees internal sample-and-hold buffer allocated by stream_body_begin().
 *          This function is optional as r1up_file_close() calls it automatically.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @return R1UP_FILE_SUCCESS - Resources freed successfully
 *         R1UP_FILE_ERROR_INVALID_PARAM - Invalid handle
 *
 * @note Automatically called by r1up_file_close()
 * @note Safe to call multiple times
 */
r1up_file_result_t r1up_file_stream_body_end(r1up_file_handle_t handle);

/** @} */ /* End of R1UP_Functions group */

#ifdef __cplusplus
}
#endif

#endif /* R1UP_FILE_H */

/**
 * @defgroup R1UP_Library R1UP File Library
 * @brief Complete R1UP file format reader library
 *
 * The R1UP File Library provides a comprehensive interface for reading
 * and parsing proprietary .r1up file format. It includes functions for:
 *
 * - File opening and closing
 * - Data reading and parsing
 * - Format validation
 * - Error handling
 * - Memory management
 *
 * @par Threading:
 * The library is designed to be thread-safe for most operations when
 * proper synchronization is used by the application.
 *
 * @par Error Handling:
 * All functions return R1upResult_t values. Always check return codes
 * and handle errors appropriately.
 */