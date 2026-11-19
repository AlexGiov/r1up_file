/**
 * @file r1up_file.c
 * @brief R1UP File Format Reader Library - Implementation
 *
 * This file contains the implementation of the R1UP file format reader library.
 * It provides all the functionality declared in r1up_file.h for reading,
 * parsing, and validating proprietary .r1up files.
 *
 * @author Alex Giov
 * @date September 27, 2025
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2025, Race1Up Project.
 * All rights reserved.
 *
 * @par Implementation Notes:
 * - All memory allocations are handled internally
 * - File I/O operations use standard C library functions
 * - Error checking is performed on all critical operations
 * - Thread safety is achieved through careful state management
 *
 * @par File Format Specification:
 * The .r1up file format consists of:
 * - Header section with magic number and metadata
 * - Data section with actual content
 * - Optional checksum for integrity verification
 */

/*==================================================================================================================*/
/* INCLUDES																											*/
/*==================================================================================================================*/

// r1up_file includes, this is the current library's header
#include "r1up_file/r1up_file.h"
#if (R1UP_FILE_ENABLE_RERACE_LEGACY_SUPPORT == 1)
#include "r1up_file/r1up_file_rr_file_variable_type_block_database.h"
#endif

// File system abstraction layer
#include "ag_fsal/ag_fsal.h"

// Memory management abstraction layer
#include "ag_mal/ag_mal.h"

// Standard library includes
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (R1UP_FILE_BUILD_ON_MATLAB == 1)
#include "mex.h"
#endif

/*==================================================================================================================*/
/* PRIVATE CONSTANTS AND MACROS																						*/
/*==================================================================================================================*/

/**
 * @defgroup R1UP_Private_Constants Private Constants
 * @brief Internal constants used by the implementation
 * @{
 */

/**
 * @defgroup R1UP_Unit_Testing_Macros Unit Testing Macros
 * @brief Macros to control static keyword and debug prints for unit testing
 * @{
 */

#ifdef UNIT_TESTING
/**
 * @brief Removes static keyword for unit testing (allows external linkage)
 */
#define STATIC
/**
 * @brief Enables debug helper print functions when unit testing
 * @details Set to 1 to enable, 0 to disable.
 */
#define R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS 1
/**
 * @brief Enables printf tracing inside implementation during unit testing
 * @details Set to 1 to enable, 0 to disable.
 */
#define R1UP_FILE_ENABLE_PRINTFS_INSIDE_R1UP_FILE_IMPLEMENTATION 0
#else  // UNIT_TESTING
/**
 * @brief Adds static keyword for normal compilation (internal linkage)
 */
#define STATIC static
/**
 * @brief Disable debug helper prints in production
 */
#define R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS 1
/**
 * @brief Disable printf tracing in production
 */
#define R1UP_FILE_ENABLE_PRINTFS_INSIDE_R1UP_FILE_IMPLEMENTATION 0
#endif	// UNIT_TESTING

/**
 * @brief Debug print macro for implementation-level tracing
 * @details Expands to printf() when enabled, otherwise it's a no-op.
 */
#if R1UP_FILE_ENABLE_PRINTFS_INSIDE_R1UP_FILE_IMPLEMENTATION == 1
#define R1UP_FILE_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#include <stdarg.h>
/**
 * @brief Dummy function to consume debug print arguments and avoid unused variable warnings when prints are disabled
 * @details This function does nothing but is used to suppress compiler warnings about unused variables in debug prints.
 */
void r1up_file_debug_helper_remove_warnig(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	(void)fmt;
	va_end(args);
}
#define R1UP_FILE_DEBUG_PRINT(...) r1up_file_debug_helper_remove_warnig(__VA_ARGS__)
#endif
/** @} */ /* End of R1UP_Unit_Testing_Macros group */

/**
 * @brief Macro to count the number of header blocks defined in R1UP_FILE_HEADER_BLOCKS_TABLE
 * @details This is need to correctly deallocate memory for user runtime defined blocks.
 */
#define R1UP_FILE_BLOCKS_TABLE_EXPAND_AS_COUNT(id_symbol, ...) 1U +
#define R1UP_FILE_BLOCKS_CNT (R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_BLOCKS_TABLE_EXPAND_AS_COUNT) 0U)

/**
 * @brief Macro expansion to generate string constants from block IDs
 * @details This macro converts each block ID symbol to a 16-character string constant.
 * Short strings are padded with underscores, long strings are **intentionally truncated** to exactly 16 characters.
 *
 * @par String Truncation Behavior:
 * When expanded, symbol names longer than 16 characters (e.g., "R1UP_FILE_HEADER_BID" = 20 chars) are concatenated
 * with "________________" and the compiler automatically truncates to fit the 16-character array.
 * This generates expected warnings which are suppressed via pragma since the truncation is the intended behavior.
 *
 * Usage example:
 * @code{.c}
 * // In your .c file:
 * #define R1UP_FILE_BLOCKS_AS_STRINGS(id_symbol, payload_len_type, payload_len, payload, editable) \
 *     static const char R1UP_##id_symbol##_STR[16] = #id_symbol "________________";
 *
 * // Generate all string constants:
 * R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_BLOCKS_AS_STRINGS)
 * @endcode
 *
 * This will create:
 * - R1UP_R1UP_FILE_HEADER_BID_STR[16] = "R1UP_FILE_HEADER"
 * - R1UP_R1UP_FILE_VER_BID_STR[16] = "R1UP_FILE_VER___"
 * - etc.
 *
 * @par Alternative Approaches Attempted:
 * The commented-out code below shows an attempt to avoid warnings by element-wise initialization,
 * but this doesn't work in C89 because the temporary array cannot be used as a compile-time constant
 * for array initialization:
 * @code{.c}
 * #define R1UP_FILE_BLOCKS_AS_STRINGS(id_symbol, ...)                                                  \
 *     static const char id_symbol##_temp[] = #id_symbol "________________";                       \
 *     const char id_symbol[16] = {                                                                \
 *         id_symbol##_temp[0], id_symbol##_temp[1], ... id_symbol##_temp[15]};
 * @endcode
 *
 * @note The current implementation is the most straightforward approach in C89 for achieving
 * exactly 16-character block IDs with automatic padding/truncation.
 *
 * @warning Compiler warnings about "initializer-string for array of chars is too long" are expected
 * and suppressed via #pragma GCC diagnostic in the code generation section below.
 */
#define R1UP_FILE_BLOCKS_AS_STRINGS(id_symbol, ...)  \
	const char id_symbol[R1UP_FILE_BLCK_ID_LENGTH] = \
		#id_symbol "________________________________________________________________";
/* C11 _Static_assert polyfill for pre-C11 compilers (XC32 v1.20 = C99/C89) */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ < 201112L)
#undef _Static_assert
#define _Static_assert(expr, msg) /* not available in pre-C11 */
#endif
_Static_assert(R1UP_FILE_BLCK_ID_LENGTH <= 64U, "R1UP_FILE_BLCK_ID_LENGTH must be less than or equal to 64");

/* Compile-time check: ensure all block ID symbol names fit within R1UP_FILE_BLCK_ID_LENGTH
 * to prevent hidden duplicates after string truncation */
#define R1UP_FILE_CHECK_SYMBOL_LENGTH(id_symbol, ...)                  \
	_Static_assert(sizeof(#id_symbol) - 1 <= R1UP_FILE_BLCK_ID_LENGTH, \
				   #id_symbol " symbol name exceeds R1UP_FILE_BLCK_ID_LENGTH - risk of truncation collision");
R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_CHECK_SYMBOL_LENGTH)
#undef R1UP_FILE_CHECK_SYMBOL_LENGTH

#define R1UP_FILE_INNER_DATA_TYPE_ID_TABLE_EXP_AS_ENUM(type_symbol, type_value) type_symbol = type_value,
/**
 * @brief Macro defining the number of variables in the variable stream format table
 * @details This macro counts the number of entries in the variable stream format table. This will be the numeber of
 * variables in the stream.
 */
#define R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_CNT(variable_id, ...) 1U +
#define R1UP_FILE_VARS_STREAM_NUM (R1UP_FILE_VARS_STREAM_FORMAT_TABLE(R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_CNT) 0U)

/*==================================================================================================================*/
/**
 * @defgroup R1UP_Global_Constants Global Constants
 * @brief Constants used throughout the R1UP library
 * @{
 */

/**
 * @brief Generate all string constants from the blocks table
 * @details This uses macro expansion to create properly formatted 16-character string constants for all block IDs.
 *
 * @par Expected Compiler Warnings:
 * The macro expansion intentionally truncates long symbol names to exactly 16 characters, which generates
 * "initializer-string for array of chars is too long" warnings. These warnings are expected and safe because:
 * - The truncation is the desired behavior (all block IDs must be exactly 16 characters)
 * - The compiler correctly truncates to array size (no buffer overflow)
 * - All block IDs are compile-time constants (safe and deterministic)
 * - There is no pragma in GCC to suppress this specific warning without affecting other useful warnings
 */
R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_BLOCKS_AS_STRINGS)

#if (R1UP_FILE_ENABLE_RERACE_LEGACY_SUPPORT == 1)
/**
 * @brief other id strings for ReRace files not defined in the blocks table
 */
static const char R1UP_FILE_RR_HEADER_BID[R1UP_FILE_RR_FILE_BLCK_ID_LENGTH] = R1UP_FILE_RR_HEADER_ID_STRING;
static const char R1UP_FILE_RR_BODY_BID[R1UP_FILE_RR_FILE_BLCK_ID_LENGTH] = R1UP_FILE_RR_BODY_ID_STRING;
static const char R1UP_FILE_RR_FOOTER_BID[R1UP_FILE_RR_FILE_BLCK_ID_LENGTH] = R1UP_FILE_RR_FOOTER_ID_STRING;
#endif

/** @} */ /* End of R1UP_Global_Constants group */

/*==================================================================================================================*/
/* PRIVATE TYPE DEFINITIONS */
/*==================================================================================================================*/

/**
 * @defgroup R1UP_Private_Types Private Types
 * @brief Internal type definitions
 * @{
 */

/* r1up_file_size_t is now defined in r1up_file.h (public API) */

/**
 * @brief Block ID type with safe memory management
 * @details This structure provides type-safe handling of block identifiers with explicit
 * distinction between static (compile-time) and dynamic (runtime) strings.
 *
 * The union ensures that the same memory location is used for both static and dynamic pointers,
 * while the is_static flag determines which member is valid and how memory should be managed.
 *
 * @note This design prevents common memory management bugs:
 * - Cannot accidentally free static strings
 * - Cannot forget to allocate memory for dynamic strings
 * - Clear ownership semantics
 */
typedef struct {
	union {
		const char* static_id;	///!< Pointer to compile-time constant string (from macro table)
		char* dynamic_id;		///!< Pointer to runtime-allocated string (from file or custom blocks)
	};
	bool is_static;	 ///!< true = static_id is valid, false = dynamic_id is valid
} r1up_file_block_id_t;

/**
 * @brief Block structure definition for header, body and footer blocks
 */
typedef struct {
	r1up_file_block_id_t id;  ///!< Block identifier with safe memory management
	uint32_t sz;			  ///!< payload size in bytes
	void* pl;				  ///!< payload
	bool is_writable;		  ///!< is block writable by the user?
} r1up_file_block_t;

/**
 * @brief List element for blocks for header, body and footer
 */
typedef struct r1up_file_block_node {
	r1up_file_block_t blck;
	struct r1up_file_block_node* next;
} r1up_file_block_node_t;

/**
 * @brief Variable Stream Format structure definition
 * @details This structure holds information about a single variable in the variable stream format.
 * It includes the variable ID, data type, size, and frequency divisor.
 * This is used to parse and interpret the variables data stream stored in the R1UP file body.
 */
typedef struct {
	uint32_t id;	//!< variable id
	uint32_t type;	//!< variable type
	uint32_t sz;	//!< size in byte of the type
	uint32_t fd;	//!< frequency divisor
} r1up_file_vsf_t;

/**
 * @brief Inner data type IDs used in the variable stream format
 */
typedef enum {
	R1UP_FILE_RR_FILE_INNER_DATA_TYPE_ID_TABLE(R1UP_FILE_INNER_DATA_TYPE_ID_TABLE_EXP_AS_ENUM)
		R1UP_FILE_RR_FILE_INNER_DATA_TYPE_ID_NUM
} r1up_file_rr_file_inner_data_type_id_t;

/**
 * @brief File type enumeration
 */
typedef enum { R1UP_FILE_FILETYPE_UNKNOWN, R1UP_FILE_FILETYPE_RACE1UP, R1UP_FILE_FILETYPE_RERACE } r1up_file_filetype_t;

/**
 * @brief Instance structure for the r1up_file library.
 * @details This structure holds all the internal state and data needed for managing R1UP files.
 * It includes the FSAL file handle, linked list of header/body/footer blocks, variable stream format
 * structures, file size and offsets, and status flags.
 * This helps to write reentrant code.
 * This ensures that multiple files can be handled simultaneously without state conflicts.
 */

/**
 * @brief Single lap boundary marker, recorded at each finish line crossing.
 * @details Stores the sample number and byte offset at the exact lap boundary.
 * The marker indicates: "the finish line was crossed between sample (sample_index - 1)
 * and sample (sample_index)". sample_index is the first sample of the NEW lap.
 */
typedef struct {
	r1up_file_size_t sample_index;	///!< Body sample number at the lap boundary (first sample of new lap)
	r1up_file_size_t file_offset;	///!< Byte offset in the file at this boundary (for fast seek on read)
} r1up_lap_marker_t;

/**
 * @brief Dynamic array of lap markers, grows one element at a time.
 * @details Laps last tens of seconds at racing speeds, so allocation frequency is negligible.
 * Grows via realloc on each new marker.
 */
typedef struct {
	r1up_lap_marker_t* markers;	 ///!< Dynamically allocated array of markers (realloc per lap)
	uint32_t count;				 ///!< Number of markers recorded (= finish line crossings)
	uint32_t capacity;			 ///!< Current allocated slots
} r1up_lap_table_t;

typedef struct {
	fsal_handle_t* fsal_handle_ptr;			   ///!< FSAL file handler
	r1up_file_filetype_t file_type;			   ///!< Type of file (R1UP or legacy ReRace)
	char filename[R1UP_FILE_MAX_PATH_LENGTH];  ///!< Name of the opened file
	r1up_file_block_node_t* blocks_head;	   ///!< Head of the linked list of header body and footer blocks

	r1up_file_vsf_t* vsf;  ///!< Variable Stream Format structure array (indexed by POSITION, not ID!)
	// TODO: maybe the write buffer and read buffer can the same, because they are not used at the same time
	void** vsd_write_buffer;  ///!< Buffer for writing variable stream data to the file. This is an etherogenic
							  /// array of different data types. It is built as an array of void pointers and each
							  /// pointer points to an element of the array. The size (types) of each element is
							  /// defined in the vsf structure array.
							  /// vsd stands for "variables stream data"
	void** vsd_read_buffers;  ///!< Variable Stream Data buffer (indexed by POSITION, not ID!)

	r1up_file_size_t current_sample_number;	 ///!< current sample number being processed
	r1up_file_size_t
		total_sample_number;			 ///!< total sample number in the file (of the variables with higher frequency)
	r1up_file_size_t vsd_bytes_written;	 ///!< total number of bytes written in the variable stream data
										 /// section
	uint32_t varible_number;			 ///!< number of variables in the variable stream format

	uint32_t hdr_ln;  /// !< header length, includes the id of the block.
					  ///	uint32_t because in the file the dimension is uint32_t
	uint32_t bdy_ln;  /// !< body length, includes the id of the block
	uint32_t ftr_ln;  /// !< footer length, includes the id of the block

	fsal_result_t fsal_res;		  /// !< result of the last FSAL operation
	r1up_file_result_t r1up_res;  /// !< result of the last R1UP operation
	r1up_file_mode_t mode;		  /// !< mode the file was opened with

	/// \brief The file size.
	r1up_file_size_t file_size;

	/// \brief The body section offsets.
	r1up_file_size_t body_offset_end;	 /// !< first not valid byte pointer
	r1up_file_size_t body_offset_start;	 /// !< first valid byte of the body, this is the first byte after the header
										 /// blocks, so the first byte after the last header block.
										 /// It is equal to the length of the header blocks.
	r1up_file_size_t body_offset_var_data_start;  /// !< first valid byte of the variable data section in the body
	r1up_file_size_t var_stream_data_size;		  /// !< size of the variable stream data section in bytes

	bool hdr_closed;  /// !< is header closed?
	bool bdy_closed;  /// !< is body closed?
	bool ftr_closed;  /// !< is footer closed?

	// Streaming mode state (for low-memory body reading)
	bool stream_initialized;					/// !< is streaming mode initialized?
	r1up_file_size_t stream_current_sample;		/// !< current sample index in streaming mode
	ag_fixedpoint_float_t* stream_last_values;	/// !< sample-and-hold buffer for streaming (var_count doubles)

	// Body info cache (lazy initialization for r1up_file_get_body_info)
	uint32_t* body_info_ids;	 /// !< Unpacked VSF IDs [variable_count]
	uint32_t* body_info_types;	 /// !< Unpacked VSF types [variable_count]
	uint32_t* body_info_sizes;	 /// !< Unpacked VSF data type sizes [variable_count]
	uint32_t* body_info_fds;	 /// !< Unpacked VSF frequency divisors [variable_count]
	bool body_info_initialized;	 /// !< Flag for lazy allocation (private, invisible to user)

	/**
	 * @warning Maybe this is a violation of single responsibility principle, as lap marker state is managed within the
	 * main file structure. The file contains data over time so is more convenient to think lap markers as time markers.
	 * So this coverts also the intermediate time line. The user of the file libraries records a time marker each time
	 * it want. Nowing the structure of data
	 * ... maybe this is not have sense: the file contains a track session not data over the time...
	 * .. or maybe not, the data is over the time and the session is configurable through the configuratinoe file.
	 */
	// Lap marker state (for lap-by-lap data access feature)
	bool lap_mark_pending;	/// !< Flag raised by r1up_file_mark_lap(), consumed by r1up_file_update_vars()
							///   TODO: make atomic for ISR safety (platform-dependent: volatile, atomic_flag, critical
							///   section)
	r1up_lap_table_t
		lap_table;	/// !< Dynamic array of lap boundary markers (WRITE mode runtime, READ mode deserialized)
} r1up_file_t;

/** @} */ /* End of R1UP_Private_Types group */

/*==================================================================================================================*/
/* PRIVATE VARIABLES																								*/
/*==================================================================================================================*/

/**
 * @defgroup R1UP_Private_Variables Private Variables
 * @brief Internal module variables
 * @{
 */

/** @} */ /* End of R1UP_Private_Variables group */

/*==================================================================================================================*/
/* BLOCK ID FACTORY FUNCTIONS - Memory-Safe Block ID Creation											            */
/*==================================================================================================================*/

/**
 * @brief Create a block ID from a static compile-time string constant
 * @details This function creates a block ID that references a static string from the macro table.
 * No memory allocation is performed - the ID simply holds a pointer to the existing constant.
 * This is the preferred method for standard blocks defined in R1UP_FILE_BLOCKS_TABLE.
 *
 * @param static_str Pointer to a compile-time constant string (must remain valid for program lifetime)
 * @return Initialized block ID structure with is_static=true
 *
 * @note The returned structure does NOT own the memory - never attempt to free static_id
 * @note Use this for: blocks from macro table, compile-time constants
 *
 * @par Example:
 * @code
 * r1up_file_block_id_t header_id = r1up_file_block_id_create_static(R1UP_FILE_HEADER_BID);
 * @endcode
 */
static inline r1up_file_block_id_t r1up_file_block_id_create_static(const char* static_str) {
	r1up_file_block_id_t id;
	id.static_id = static_str;
	id.is_static = true;
	return id;
}

/**
 * @brief Create a block ID by copying a runtime string into dynamically allocated memory
 * @details This function allocates new memory and copies exactly R1UP_FILE_BLCK_ID_LENGTH bytes from the input string.
 * The caller must eventually call r1up_file_block_id_free() to release this memory.
 * This is used for blocks read from files or created at runtime.
 *
 * @param runtime_str Pointer to source string buffer (must contain at least R1UP_FILE_BLCK_ID_LENGTH readable bytes)
 * @return Initialized block ID structure with is_static=false, or a zeroed structure on allocation failure
 *
 * @note The returned structure OWNS the allocated memory - must be freed with r1up_file_block_id_free()
 * @note Use this for: blocks read from files, runtime custom blocks
 * @note Returns zeroed structure (dynamic_id=NULL, is_static=false) on allocation failure
 *
 * @par Example:
 * @code
 * char block_from_file[R1UP_FILE_BLCK_ID_LENGTH];
 * fsal_read(block_from_file, ...);
 * r1up_file_block_id_t id = r1up_file_block_id_create_dynamic(block_from_file);
 * if (id.dynamic_id == NULL) {
 *     // Handle allocation failure
 * }
 * // ... use id ...
 * r1up_file_block_id_free(&id);
 * @endcode
 */
static inline r1up_file_block_id_t r1up_file_block_id_create_dynamic(const char* runtime_str) {
	// Precondition: runtime_str must be exactly R1UP_FILE_BLCK_ID_LENGTH bytes, no null terminators allowed
	// This assert catches improperly prepared strings (e.g., passing "HELLO" instead of "HELLO___...")
	// Note: if you remove this assert use the following precondition check: force read of last expected byte
	// AddressSanitizer/Valgrind will catch if buffer is shorter than required
	// @code {.c}
	// (void)runtime_str[R1UP_FILE_BLCK_ID_LENGTH - 1];
	// @endcode
	assert(runtime_str[R1UP_FILE_BLCK_ID_LENGTH - 1] != '\0');

	char* new_str = AG_MALLOC(R1UP_FILE_BLCK_ID_LENGTH);
	if (new_str == NULL) {
		// Return invalid ID on allocation failure
		r1up_file_block_id_t invalid;
		invalid.dynamic_id = NULL;
		invalid.is_static = false;
		return invalid;
	}

	memcpy(new_str, runtime_str, R1UP_FILE_BLCK_ID_LENGTH);

	r1up_file_block_id_t id;
	id.dynamic_id = new_str;
	id.is_static = false;
	return id;
}

/**
 * @brief Create a block ID by matching against known static IDs, or allocating if unknown
 * @details This function first attempts to match the input string against all known block IDs
 * from the macro table. If a match is found, returns a static block ID (no allocation).
 * If no match is found, allocates memory and creates a dynamic block ID.
 *
 * This is the recommended function for READ operations as it optimizes memory usage
 * by reusing static strings when possible.
 *
 * @param string_from_file Pointer to string buffer read from file (must contain at least R1UP_FILE_BLCK_ID_LENGTH
 * bytes)
 * @return Block ID structure - either static (if matched) or dynamic (if unknown)
 *
 * @note Caller must always call r1up_file_block_id_free() - it's safe for both static and dynamic IDs
 * @note Returns invalid ID (dynamic_id=NULL) on allocation failure for unknown blocks
 *
 * @par Example:
 * @code
 * char id_from_file[R1UP_FILE_BLCK_ID_LENGTH];
 * fsal_read(id_from_file, ...);
 * r1up_file_block_id_t id = r1up_file_block_id_create_from_file(id_from_file);
 * // ... use id ...
 * r1up_file_block_id_free(&id);  // Safe for both static and dynamic
 * @endcode
 */
static inline r1up_file_block_id_t r1up_file_block_id_create_from_file(const char* string_from_file) {
#if R1UP_FILE_REMOVE_BLOCK_STRING_ID_CHECK_IN_READ == 0
// Try to match against known static block IDs
#define R1UP_FILE_BLOCKS_TABLE_MATCH_ID(id_symbol, ...)                       \
	if (memcmp(string_from_file, id_symbol, R1UP_FILE_BLCK_ID_LENGTH) == 0) { \
		return r1up_file_block_id_create_static(id_symbol);                   \
	}

	R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_BLOCKS_TABLE_MATCH_ID)

#undef R1UP_FILE_BLOCKS_TABLE_MATCH_ID

	// Unknown block ID - this shouldn't happen in well-formed files
	// Return invalid ID to signal error
	r1up_file_block_id_t invalid;
	invalid.dynamic_id = NULL;
	invalid.is_static = false;
	return invalid;
#else
	// Skip validation - always create dynamic copy
	return r1up_file_block_id_create_dynamic(string_from_file);
#endif
}

/**
 * @brief Safely free a block ID, handling both static and dynamic cases
 * @details This function releases memory for dynamic block IDs and safely handles static IDs.
 * After freeing, the structure is zeroed to prevent use-after-free bugs.
 *
 * @param id Pointer to block ID to free (must not be NULL)
 *
 * @note Safe to call on static IDs (no-op)
 * @note Safe to call multiple times (idempotent after first call)
 * @note The block ID structure is zeroed after freeing
 *
 * @par Example:
 * @code
 * r1up_file_block_id_t id = r1up_file_block_id_create_dynamic("MY_BLOCK_______");
 * // ... use id ...
 * r1up_file_block_id_free(&id);  // Memory freed, id zeroed
 * r1up_file_block_id_free(&id);  // Safe to call again (no-op)
 * @endcode
 */
static inline r1up_file_result_t r1up_file_block_id_free(r1up_file_block_id_t* id) {
	if (id == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;
	bool error = false;

	if (!id->is_static && id->dynamic_id != NULL) {
		if (AG_FREE(id->dynamic_id) != AG_MAL_OK) {
			error = true;
		}
	}

	// Zero out the structure to prevent use-after-free
	id->dynamic_id = NULL;
	id->is_static = false;

	return error ? R1UP_FILE_ERROR_MEMORY : R1UP_FILE_SUCCESS;
}

/**
 * @brief Get the raw string pointer from a block ID (for I/O operations)
 * @details Returns a const pointer to the actual string data, regardless of whether
 * the ID is static or dynamic. Useful for writing to files or printing.
 *
 * @param id Pointer to block ID
 * @return Const pointer to the 16-byte string data, or NULL if ID is invalid
 *
 * @note The returned pointer is valid as long as the block ID structure is valid
 * @note Do NOT attempt to free the returned pointer
 *
 * @par Example:
 * @code
 * r1up_file_block_id_t id = ...;
 * const char* str = r1up_file_block_id_get_string(&id);
 * fsal_write(str, 1, R1UP_FILE_BLCK_ID_LENGTH, file_handle);
 * @endcode
 */
static inline const char* r1up_file_block_id_get_string(const r1up_file_block_id_t* id) {
	if (id == NULL) return NULL;
	return id->is_static ? id->static_id : id->dynamic_id;
}

/**
 * @brief Compare two block IDs for equality
 * @details Performs efficient comparison optimized for the common case of comparing
 * against static block IDs. Uses pointer comparison for static IDs and memcmp for dynamic IDs.
 *
 * @param a Pointer to first block ID
 * @param b Pointer to second block ID
 * @return true if IDs are equal, false otherwise
 *
 * @note Returns false if either pointer is NULL or if either ID is invalid (NULL string)
 * @note Optimized for static-vs-static comparisons (single pointer comparison)
 *
 * @par Example:
 * @code
 * r1up_file_block_id_t header_id = r1up_file_block_id_create_static(R1UP_FILE_HEADER_BID);
 * if (r1up_file_block_id_equals(&node->blck.id, &header_id)) {
 *     // This is a header block
 * }
 * @endcode
 */
static inline bool r1up_file_block_id_equals(const r1up_file_block_id_t* a, const r1up_file_block_id_t* b) {
	if (a == NULL || b == NULL) return false;

	const char* str_a = r1up_file_block_id_get_string(a);
	const char* str_b = r1up_file_block_id_get_string(b);

	if (str_a == NULL || str_b == NULL) return false;

	// Fast path: if both are static and point to the same address, they're equal
	if (a->is_static && b->is_static && str_a == str_b) {
		return true;
	}

	if (!a->is_static || !b->is_static) {
		// This condition prevent error if the static string content are the same but the pointer are different. This
		// can happen when a static string create by R1UP_FILE_BLOCKS_TABLE is truncated.
		return memcmp(str_a, str_b, R1UP_FILE_BLCK_ID_LENGTH) == 0;
	}

	// this should not happen, but just in case
	return false;
}
/*==================================================================================================================*/
/* PRIVATE FUNCTION DECLARATIONS																					*/
/*==================================================================================================================*/

/**
 * @defgroup R1UP_Private_Functions Private Functions
 * @brief Internal helper functions
 * @{
 */

/**
 * @brief Perform compile-time checks to ensure consistency between related enumerations and definitions
 */
static void r1up_file_compile_time_checks(void);

/**
 * @brief Convert R1UP file mode to FSAL file mode
 * @param mode R1UP file mode
 * @return Corresponding FSAL file mode, if invalid mode returns FSAL_MODE_READ
 */
static fsal_mode_t r1up_file_mode_to_fsal(r1up_file_mode_t mode);

/**
 * @brief Generate next available automatic filename in the form r1up_00000.r1up
 * @param out_filename Output buffer for generated filename
 * @param out_filename_size Output buffer size
 * @return true if a free filename was found, false otherwise
 */
static bool r1up_file_generate_auto_filename(char* out_filename, size_t out_filename_size);

/**
 * @brief Convert FSAL result code to R1UP result code
 * @param fsal_res FSAL result code
 * @return Corresponding R1UP result code
 */
static r1up_file_result_t r1up_file_convert_fsal_result_to_r1up(fsal_result_t fsal_res);

/**
 * @brief Initialize the r1up_file_t structure with default values
 * @details Zeros the entire structure using memset, then initializes only non-zero fields.
 * This is more efficient than manually setting each field.
 * @param file Pointer to the file instance to initialize
 * @param fsal_handle Pointer to the FSAL handle
 * @param mode File opening mode
 */
static void r1up_file_init_structure(r1up_file_t* file, fsal_handle_t* fsal_handle, r1up_file_mode_t mode);

/**
 * @brief Initialize the linked list of header blocks, setting up default blocks
 * @note A dynamic list is used to allow easy addition/removal of blocks if needed
 * @param file Pointer to the file instance to initialize
 */
static r1up_file_result_t r1up_file_init_blocks_list(r1up_file_t* file);

/**
 * @brief Initialize the linked list of blocks for reading the legacy ReRace
 * file format.
 * @details The legacy file is read and blocks are adapted to the R1UP format.
 * @param file Pointer to the file instance to initialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_rr_file_init_blocks_list(r1up_file_t* file);

/**
 * @brief Wrapper function to initialize the linked list of blocks
 * @param file Pointer to the file instance to initialize
 */
static r1up_file_result_t r1up_file_init_blocks_list_wrapper(r1up_file_t* file);

/**
 * @brief Split sample frequency from the variable stream format from the legacy ReRace file format.
 * @details The legacy file format encodes sample frequency differently, so this function
 * translates it into the R1UP variable stream format structure.
 * @param file Pointer to the file instance to initialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_rr_file_split_data_format(r1up_file_t* file);

/**
 * @brief Initialize the variable stream format structure array
 * @param file Pointer to the file instance to initialize
 */
static r1up_file_result_t r1up_file_init_variables_stream_format(r1up_file_t* file);

/**
 * @brief Linear-scan lookup: variable ID → position in VSF array.
 * @param file   File handle (vsf must be non-NULL).
 * @param var_id Semantic variable ID (e.g. 0x01000000 for RPM).
 * @return Position (0…var_count-1) or -1 if not found.  O(N), N ≤ 28.
 */
static int32_t r1up_file_find_var_position(const r1up_file_t* file, r1up_variable_id_t var_id);

/**
 * @brief Initialize the variable stream write data buffer
 * @param file Pointer to the file instance to initialize
 */
static r1up_file_result_t r1up_file_init_vsd_write_buffer(r1up_file_t* file);

/**
 * @brief Deinitialize the variable stream write data buffer
 * @param file Pointer to the file instance to deinitialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_deinit_vsd_write_buffer(r1up_file_t* file);

/**
 * @brief Deinitialize the variable stream read data buffer
 * @param file Pointer to the file instance to deinitialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_deinit_vsd_read_buffer(r1up_file_t* file);

/**
 * @brief Deinitialize all components of the r1up_file instance
 * @param file Pointer to the file instance to deinitialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_deinit_all(r1up_file_t* file);

/**
 * @brief Deinitialize the body info cache arrays
 * @param file Pointer to the file instance to deinitialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_deinit_body_info(r1up_file_t* file);

/**
 * @brief Free all blocks in a linked list
 * @param file Pointer to the file instance containing blocks to free
 */
static r1up_file_result_t r1up_file_free_blocks_list(r1up_file_t* file);

/**
 * @brief Deinitialize the variable stream format structure array
 * @param file Pointer to the file instance to deinitialize
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_deinit_variables_stream_format(r1up_file_t* file);

/**
 * @brief Retrieve the node of a block by its ID string from the file's block lists
 * @warning This function always search by string and not by enum, because the user can define custom blocks in a spare
 * order. Always use this function to get a block by its ID.
 * @param file Pointer to the file structure
 * @param id_string ID string of the block to retrieve
 * @return Pointer to the block node if found, NULL otherwise
 */
static r1up_file_block_node_t* r1up_file_get_block_node(r1up_file_t* file, const char* id_string);

/**
 * @brief Compute the total size of the header, or body, or footer of the block linked list
 * @param file Pointer to the file instance
 * @param id_string ID string of header, body, or footer block list to compute size for
 * @return R1UP_FILE_SUCCESS on success, or an error code on failure
 */
static r1up_file_result_t r1up_file_cmp_blocks_list_size(r1up_file_t* file, const char* id_string,
														 r1up_file_size_t* total_size);

/**
 * @brief Compute the maximum length of the variable stream data block based on the variable stream format
 * @param file Pointer to the file instance
 * @param var_number Number of variables in the variable stream format
 * @return Maximum length in bytes of the variable stream data block
 */
static r1up_file_size_t r1up_file_cmpt_max_vsd_block_length(r1up_file_t* file, uint16_t var_number);

/**
 * @brief Retrieve a block by its ID string from the file's block lists
 * @warning This function always search by string and not by enum, because the user can define custom blocks in a spare
 * order. Always use this function to get a block by its ID.
 * @param f Pointer to the file structure
 * @param id_string ID string of the block to retrieve
 * @return Pointer to the block if found, NULL otherwise
 */
static r1up_file_block_t* r1up_file_get_block(r1up_file_t* f, const char* id_string);

/**
 * @brief Retrieve a pointer to the block_id of an existing block in the file's block list
 * @details This function returns a pointer to the actual block_id structure stored in the list,
 * not a newly created one. This is important for pointer-based comparisons.
 * @param file Pointer to the file structure
 * @param id_string ID string of the block to retrieve
 * @return Pointer to the block_id if found, NULL otherwise
 */
static r1up_file_block_id_t* r1up_file_get_block_id(r1up_file_t* file, const char* id_string);

/**
 * @brief Write all blocks of the header, body, or footer section to the file
 * @warning This function assumes the file is opened in write mode and the file pointer is at the correct position.
 * @param file Pointer to the file instance
 * @param id_string ID string of the block to start writing at (exclusive) It can be only R1UP_FILE_HEADER_BID,
 * R1UP_FILE_BODY_BID or R1UP_FILE_FOOTER_BID
 * @param bytes_written_out Pointer to store the number of bytes written to the file
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code
 */
static r1up_file_result_t r1up_file_write_blocks_to_file(r1up_file_t* file, const char* id_string,
														 r1up_file_size_t* bytes_written_out);

/**
 * @brief Check if a block ID corresponds to a header block
 * @param file The file structure to check
 * @param id_string Block ID string to check
 * @return true if the block ID is a header block, false otherwise
 */
static bool r1up_file_is_header_block(r1up_file_t* file, const char* id_string);

/**
 * @brief Check if a block ID corresponds to a body block
 * @param file The file structure to check
 * @param id_string Block ID string to check
 * @return true if the block ID is a body block, false otherwise
 */
static bool r1up_file_is_body_block(r1up_file_t* file, const char* id_string);

/**
 * @brief Check if a block ID corresponds to a footer block
 * @param file The file structure to check
 * @param id_string Block ID string to check
 * @return true if the block ID is a footer block, false otherwise
 */
static bool r1up_file_is_footer_block(r1up_file_t* file, const char* id_string);

/**
 * @brief Close the body section of a Race1UP file.
 * @details This function finalizes and closes the body section of a Race1UP file.
 *          It ensures that all body data is properly written and the body is marked as closed.
 * @param file The file structure whose body section is to be closed.
 * @return R1UP_FILE_SUCCESS on successful closure of the body, or an appropriate error code.
 * @note This function should be called after finishing writing body data to the file. If not the function is called
 * internally before closing the file.
 * @note After calling this function, no further modifications to the body are allowed.
 */
static r1up_file_result_t r1up_file_close_body(r1up_file_t* file);

/**
 * @brief Close the footer section of a Race1UP file.
 * @details This function finalizes and closes the footer section of a Race1UP file.
 *          It ensures that all footer data is properly written and the footer is marked as closed.
 * @param file The file structure whose footer section is to be closed.
 * @return R1UP_FILE_SUCCESS on successful closure of the footer, or an appropriate error code.
 * @note This function should be called after finishing writing footer data to the file. If not the function is called
 * internally before closing the file.
 * @note After calling this function, no further modifications to the footer are allowed.
 */
static r1up_file_result_t r1up_file_close_footer(r1up_file_t* file);

/**
 * @brief Serialize the in-RAM lap_table into the R1UP_FILE_LAP_TBL_BID footer block payload.
 * @details Called internally by r1up_file_close_footer() before computing the total footer size.
 *          If no lap markers were recorded (count == 0), the block payload stays empty (sz = 0, pl = NULL).
 *          Layout:  [ uint32_t count ][ r1up_lap_marker_t[0] … r1up_lap_marker_t[count-1] ]
 * @param file Pointer to the file instance containing the lap_table to serialize.
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
static r1up_file_result_t r1up_file_serialize_lap_table(r1up_file_t* file);

/**
 * @brief Deserialize the R1UP_FILE_LAP_TBL_BID footer block payload into the in-RAM lap_table.
 * @details Called internally by r1up_file_open() on the READ path, after all blocks have been loaded.
 *          If the block has no payload (sz == 0), lap_table stays empty (count = 0, markers = NULL).
 *          Expected layout:  [ uint32_t count ][ r1up_lap_marker_t[0] … r1up_lap_marker_t[count-1] ]
 * @param file Pointer to the file instance whose lap_table will be populated.
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code.
 */
static r1up_file_result_t r1up_file_deserialize_lap_table(r1up_file_t* file);

/**
 * @brief Check for consistency between the recorded file size and the actual size based on header, body, and footer
 * lengths
 * @warning If the function returns an error, the file pointer may be in an undefined value.
 * @param file Pointer to the file instance to check
 * @return R1UP_FILE_SUCCESS if sizes are consistent, or an appropriate error code if inconsistent
 */
static r1up_file_result_t r1up_file_check_file_size_consistency(r1up_file_t* file);

/**
 * @brief Initialize file metadata for legacy ReRace file format
 * @param file Pointer to the file instance to initialize
 */
static r1up_file_result_t r1up_file_init_file_metadata(r1up_file_t* file);

/**
 * @brief Detect the file type (R1UP or legacy ReRace) based on file content
 * @param file Pointer to the file instance
 * @return Detected file type enumeration value
 */
static r1up_file_filetype_t r1up_file_detect_file_type(r1up_file_t* file);

/**
 * @brief Check if a string exists in the list of blocks
 * @param file Pointer to the file instance
 * @param id_string The string ID to check for
 * @return true if the string exists in the blocks list, false otherwise
 */
static bool r1up_file_string_exists_in_blocks_list(r1up_file_t* file, const char* id_string);

/**
 * @brief Compute the sample number based on the variable stream data block size
 * @param file Pointer to the file instance
 * @param vsd_blck_sz Size of the variable stream data block in bytes
 * @param out_sample_number Pointer to store the computed sample number
 * @return R1UP_FILE_SUCCESS on success, or an appropriate error code
 */
static r1up_file_result_t r1up_file_compute_sample_number(r1up_file_t* file, r1up_file_size_t vsd_blck_sz,
														  r1up_file_size_t* out_sample_number);
/**
 * \brief Compute the number of samples of a given variable with its frequency divisor that occurs in total_samples.
 *
 * \details This function calculates how many times a variable is actually sampled within a total number of samples,
 * considering the frequency divisor. A variable with frequency divisor N is sampled every N samples starting from 0.
 * This implements the "sample and hold" logic used throughout the library.
 *
 * The frequency divisor works as follows:
 * - fd = 1: variable sampled at every sample (samples: 0, 1, 2, 3, 4, ...)
 * - fd = 2: variable sampled every 2 samples (samples: 0, 2, 4, 6, 8, ...)
 * - fd = 3: variable sampled every 3 samples (samples: 0, 3, 6, 9, 12, ...)
 *
 * Example with total_samples = 10 and fd = 3:
 * - Sample 0: 0 % 3 == 0 ✓ (sampled)
 * - Sample 1: 1 % 3 == 1 ✗
 * - Sample 2: 2 % 3 == 2 ✗
 * - Sample 3: 3 % 3 == 0 ✓ (sampled)
 * - Sample 6: 6 % 3 == 0 ✓ (sampled)
 * - Sample 9: 9 % 3 == 0 ✓ (sampled)
 * Result: 4 samples (at positions 0, 3, 6, 9)
 *
 * \param file Pointer to the file instance
 * \param var_id ID of the variable to compute samples for
 * \param samples Pointer to store the computed number of samples
 * \return R1UP_FILE_SUCCESS on success, or an appropriate error code
 *
 * \note The simple formula total_samples/frequency_divisor would be incorrect because it doesn't account for
 * the exact sampling positions. For example, with total_samples=10 and fd=3, the simple formula would give
 * 10/3=3 samples, but the correct result is 4 samples.
 *
 * \warning The original implementation used a loop to count samples. This optimized version uses the mathematical
 * formula (total_samples + fd - 1) / fd which is equivalent but much faster, especially for large datasets.
 * Original loop code that was replaced:
 * \code
 * long int samples = 0;
 * for (long int i = 0; i < total_samples; i++) {
 *     if (i % ((vsf_pnt + var_id)->fd) == 0)
 *         samples++;
 * }
 * return samples;
 * \endcode
 */
static r1up_file_result_t r1up_file_compute_sample_number_for_single_variable(r1up_file_t* file, uint8_t var_id,
																			  r1up_file_size_t* samples);

/**
 * @brief Get the size of the file
 * @param file Pointer to the file instance
 * @return Size of the file in bytes
 */
static r1up_file_size_t r1up_file_get_file_size(r1up_file_t* file);

/**
 * @brief Get the number of bytes from the current file pointer position to the end of the file
 * @param file Pointer to the file instance
 * @return Number of bytes till end of file
 */
static r1up_file_size_t r1up_file_byte_till_eof(r1up_file_t* file);

/**
 * @brief Check if the file pointer is at the end of the file
 * @param file Pointer to the file instance
 * @return true if at end of file, false otherwise
 */
static bool r1up_file_check_eof(r1up_file_t* file);

/**
 * @brief Positions the file pointer at the beginning of the footer or at the end of the file if the footer is not
 * present.
 * @note Used for reading legacy ReRace files.
 * @param file Pointer to the file instance
 * @return true if the string is found (pointer at the beginning of the string),
 *         false otherwise (pointer at the end of the file)
 */
static bool r1up_file_rer_file_seek_to_footer_or_eof(r1up_file_t* file);

/**
 * @brief Convert a single raw sample value to floating-point using VSF type information
 * @details This helper function converts one variable's raw value (fixedpoint format) to float.
 * Used internally by streaming mode to convert samples on-the-fly without buffering.
 *
 * @param file Pointer to the file instance (for VSF access)
 * @param var_id Variable ID to convert (resolved by linear scan in VSF)
 * @param raw_value Pointer to the raw value in fixedpoint format (type determined by VSF)
 * @param out_value Pointer to store the converted float value
 * @return R1UP_FILE_SUCCESS on success, R1UP_FILE_ERROR_NOT_FOUND if variable not in file,
 *         R1UP_FILE_ERROR_INVALID_PARAM if parameters are NULL
 *
 * @note The function performs a linear scan to map var_id → position, then reads the type from VSF[position]
 * @note This is more efficient than r1up_file_get_var_float() for single samples (no array allocation)
 */
static r1up_file_result_t r1up_file_convert_raw_sample_to_float(r1up_file_t* file, r1up_variable_id_t var_id,
																const void* raw_value,
																ag_fixedpoint_float_t* out_value);

#ifdef UNIT_TESTING
/**
 * @brief Debug helper function to print a block ID
 * @param file Pointer to the file instance
 * @param block_id Pointer to the block ID to print
 */
void r1up_file_debug_helper_print_blockid(r1up_file_t* file, r1up_file_block_id_t* block_id);

/**
 * @brief Debug helper function to print the variable stream format
 * @param file Pointer to the file instance
 */
static void _r1up_file_debug_helper_print_vsf(r1up_file_t* file);

/**
 * @brief Debug helper function to print the list of blocks in a Race1UP file.
 * @details This function iterates through the list of blocks in the Race1UP file and prints their details
 * including ID, size, payload (in hex and text), and writability status.
 * @param handle The file handle obtained from r1up_file_open().
 * @return void
 */
void r1up_file_debug_helper_print_block_list(r1up_file_handle_t handle);

/**
 * @brief Debug helper function to print the IDs of blocks in a Race1UP file.
 * @details This function iterates through the list of blocks in the Race1UP file and prints their IDs.
 * @param handle The file handle obtained from r1up_file_open().
 * @return void
 */
void r1up_file_debug_helper_print_block_ids(r1up_file_handle_t handle);

/**
 * @brief Debug helper function to print the Variables Stream Format of a Race1UP file.
 * @details This function prints the Variables Stream Format (VSF) structure array of the
 * Race1UP file, including variable IDs, types, sizes, and divisors.
 * @param handle The file handle obtained from r1up_file_open().
 */
void r1up_file_debug_helper_print_variables_stream_format(r1up_file_handle_t handle);

/**
 * @brief Debug helper function to print metadata of a Race1UP file.
 * @details This function prints key metadata information of the Race1UP file,
 * including version, sample rate, number of samples, and variable data offsets.
 * @param handle The file handle obtained from r1up_file_open().
 * @return void
 */
void r1up_file_debug_helper_print_metadata(r1up_file_handle_t handle);

/**
 * @brief Debug helper function to print the variable stream format
 * @param handle The file handle obtained from r1up_file_open().
 */
void r1up_file_debug_helper_print_vsf(r1up_file_handle_t handle);
#endif

/** @} */ /* End of R1UP_Private_Functions group */

/*==================================================================================================================*/
/* PUBLIC FUNCTION IMPLEMENTATIONS */
/*==================================================================================================================*/

/*==================================================================================================================*/
static void r1up_file_init_structure(r1up_file_t* file, fsal_handle_t* fsal_handle, r1up_file_mode_t mode) {
	// Zero the entire structure - all pointers become NULL, all integers become 0, all bools become false
	memset(file, 0, sizeof(r1up_file_t));

	// Initialize only non-zero fields
	file->fsal_handle_ptr = fsal_handle;
	file->mode = mode;
	file->fsal_res = FSAL_SUCCESS;

	if (file->mode == R1UP_FILE_MODE_WRITE_ONLY) {
		// only Race1UP file writing is supported this set the number of viarable directly from the configuration table
		// you also set up file type here but there is a dedciated function latter.
		file->varible_number = R1UP_FILE_VARS_STREAM_NUM;
	}

	// Note: The following fields are already zero/NULL/false after memset and don't need explicit initialization:
	// - filename = "" (empty string after memset, will be set by caller in r1up_file_open)
	// - blocks_head = NULL
	// - vsf = NULL
	// - vsd_write_buffer = NULL
	// - vsd_read_buffers = NULL
	// - current_sample_number = 0
	// - total_sample_number = 0
	// - vsd_bytes_written = 0
	// - varible_number = 0 if in read mode
	// - hdr_ln = 0, bdy_ln = 0, ftr_ln = 0
	// - file_size = 0
	// - body_offset_end = 0, body_offset_start = 0
	// - body_offset_var_data_start = 0, var_stream_data_size = 0
	// - hdr_closed = false, bdy_closed = false, ftr_closed = false
	// - file_type = R1UP_FILE_FILETYPE_UNKNOWN (0)
	// - body_info_ids = NULL, body_info_types = NULL, body_info_sizes = NULL, body_info_fds = NULL
	// - body_info_initialized = false
	// - lap_mark_pending = false
	// - lap_table = { .markers = NULL, .count = 0, .capacity = 0 }
}

/*==================================================================================================================*/
static r1up_file_filetype_t r1up_file_detect_file_type(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return R1UP_FILE_FILETYPE_UNKNOWN;
	}

	fsal_result_t res;
	long found_pos;

	switch (file->mode) {
		case R1UP_FILE_MODE_READ_ONLY:
			res = fsal_search(file->fsal_handle_ptr, R1UP_FILE_RR_HEADER_BID, R1UP_FILE_RR_FILE_BLCK_ID_LENGTH, false,
							  &found_pos, true);
			if (res == FSAL_SUCCESS && found_pos == 0U) {
				// ReRace file detected
				return R1UP_FILE_FILETYPE_RERACE;
			}

			res = fsal_search(file->fsal_handle_ptr, R1UP_FILE_HEADER_BID, R1UP_FILE_BLCK_ID_LENGTH, false, &found_pos,
							  true);
			if (res == FSAL_SUCCESS && found_pos == 0U) {
				// ReRace file detected
				return R1UP_FILE_FILETYPE_RACE1UP;
			}
			break;
		case R1UP_FILE_MODE_WRITE_ONLY:
			// For write-only mode, only Race1UP files are supported
			return R1UP_FILE_FILETYPE_RACE1UP;
			break;
		default:
			// Unsupported mode for detection
			return R1UP_FILE_FILETYPE_UNKNOWN;
	}

	// Unknown file type
	return R1UP_FILE_FILETYPE_UNKNOWN;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_init_file_metadata(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// this function works only for read mode, skip for other modes
	if (file->mode != R1UP_FILE_MODE_READ_ONLY) {
		return R1UP_FILE_SUCCESS;
	}

	// Select header, body, footer ID string and lengthbased on detected file type
	const char* header_id =
		(file->file_type == R1UP_FILE_FILETYPE_RERACE ? R1UP_FILE_RR_HEADER_BID : R1UP_FILE_HEADER_BID);
	const char* body_id = (file->file_type == R1UP_FILE_FILETYPE_RERACE ? R1UP_FILE_RR_BODY_BID : R1UP_FILE_BODY_BID);
	const char* footer_id =
		(file->file_type == R1UP_FILE_FILETYPE_RERACE ? R1UP_FILE_RR_FOOTER_BID : R1UP_FILE_FOOTER_BID);
	size_t id_length =
		(file->file_type == R1UP_FILE_FILETYPE_RERACE ? R1UP_FILE_RR_FILE_BLCK_ID_LENGTH : R1UP_FILE_BLCK_ID_LENGTH);

	file->file_size = r1up_file_get_file_size(file);
	if (file->file_size == 0U) {
		// empty file
		return R1UP_FILE_ERROR_INVALID_FORMAT;
	}

	fsal_result_t res;
	long found_pos;
	res = fsal_search(file->fsal_handle_ptr, header_id, id_length, false, &found_pos, true);
	if (res != FSAL_SUCCESS || found_pos != 0U) {
		// not a ReRace file
		return R1UP_FILE_ERROR_INVALID_FORMAT;
	}

	res = fsal_search(file->fsal_handle_ptr, body_id, id_length, false, &found_pos, true);
	if (res != FSAL_SUCCESS) {
		// some error occurred, consider not a valide ReRace file, just return invalid format error.
		return R1UP_FILE_ERROR_INVALID_FORMAT;
	}
	// set header length in file structure
	file->hdr_ln = (r1up_file_size_t)found_pos;
	// set body dimension and offsets in file structure
	file->body_offset_start = file->hdr_ln;
	file->body_offset_var_data_start = (file->file_type == R1UP_FILE_FILETYPE_RERACE)
										   ? file->body_offset_start + id_length + sizeof(uint32_t)
										   : file->body_offset_start + 0x04U + id_length + sizeof(uint32_t);

	res = fsal_search(file->fsal_handle_ptr, footer_id, id_length, false, &found_pos, true);
	if (res != FSAL_SUCCESS && res != FSAL_ERROR_NOT_FOUND /* note a file without footer is valid*/) {
		// some error occurred, consider not a valide ReRace file, just return invalid format error.
		return R1UP_FILE_ERROR_INVALID_FORMAT;
	}

	// set footer length in file structure
	if (res == FSAL_ERROR_NOT_FOUND) {
		file->ftr_ln = 0U;
	} else {
		file->ftr_ln = file->file_size - (r1up_file_size_t)found_pos;
	}
	// set body lenght and end offset in file structure
	file->bdy_ln = file->file_size - file->hdr_ln - file->ftr_ln;
	file->body_offset_end = file->file_size - file->ftr_ln;	 // if footer is 0, body_offset_end = file_size

	assert(file->file_size == file->hdr_ln + file->bdy_ln + file->ftr_ln);
	assert(file->bdy_ln == file->body_offset_end - file->body_offset_start);

	// set variable stream data size
	file->var_stream_data_size = file->file_type == R1UP_FILE_FILETYPE_RERACE
									 ? file->bdy_ln - (id_length + sizeof(uint32_t))
									 : file->bdy_ln - (id_length + sizeof(uint32_t) + 0x04U);
	assert(file->var_stream_data_size == file->body_offset_end - file->body_offset_var_data_start);

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_handle_t r1up_file_open(const char* filename, r1up_file_mode_t mode) {
	// Parameter validation
	if (mode < R1UP_FILE_MODE_READ_ONLY || mode > R1UP_FILE_MODE_APPEND) {
		return NULL;
	}
	if (filename != NULL && strlen(filename) == 0U) {
		return NULL;
	}
	if (filename == NULL && mode != R1UP_FILE_MODE_WRITE_ONLY) {
		return NULL;
	}

	/** @brief Current version of the library allows only write and read modes
	 */
	if (mode != R1UP_FILE_MODE_READ_ONLY && mode != R1UP_FILE_MODE_WRITE_ONLY) {
		return NULL;
	}

	// Initialize MAL library: Memory Abstraction Layer.
	ag_mal_result_t result = ag_mal_init();
	if (result != AG_MAL_OK) {
		return NULL;
	}

	// Allocate memory for file structure
	r1up_file_t* file_hndl = AG_MALLOC(sizeof(r1up_file_t));
	if (file_hndl == NULL) {
		return NULL;
	}

	// Initialize FSAL library: File System Abstraction Layer.
	fsal_result_t fsal_result = fsal_init();
	if (fsal_result != FSAL_SUCCESS) {
		AG_FREE(file_hndl);
		return NULL;
	}

	// Allocate memory for FSAL handle
	fsal_handle_t* fsal_handle_ptr = AG_MALLOC(sizeof(fsal_handle_t));
	if (fsal_handle_ptr == NULL) {
		AG_FREE(file_hndl);
		return NULL;
	}

	// Resolve filename: if NULL in write mode, generate r1up_00000.r1up ... r1up_99999.r1up
	char resolved_filename[R1UP_FILE_MAX_PATH_LENGTH] = {0};
	const char* open_filename = filename;
	if (filename == NULL && mode == R1UP_FILE_MODE_WRITE_ONLY) {
		if (!r1up_file_generate_auto_filename(resolved_filename, sizeof(resolved_filename))) {
			AG_FREE(fsal_handle_ptr);
			AG_FREE(file_hndl);
			return NULL;
		}
		open_filename = resolved_filename;
	}

	fsal_mode_t fsal_mode = r1up_file_mode_to_fsal(mode);
	if (filename == NULL && mode == R1UP_FILE_MODE_WRITE_ONLY) {
		// Atomic create to avoid TOCTOU races in multi-thread/process environments.
		fsal_mode = FSAL_MODE_CREATE_NEW;
	}

	// Open file using FSAL
	fsal_result = fsal_open(open_filename, fsal_mode, fsal_handle_ptr);
	if (fsal_result != FSAL_SUCCESS) {
		// Set error status and cleanup
		file_hndl->fsal_res = fsal_result;
		R1UP_FILE_DEBUG_PRINT("[ERROR] fsal_open failed for file '%s' with result: %d\n", open_filename, fsal_result);
		AG_FREE(fsal_handle_ptr);
		AG_FREE(file_hndl);
		return NULL;
	}

	// Initialize file structure with default values (zeros entire structure, then sets non-zero fields)
	r1up_file_init_structure(file_hndl, fsal_handle_ptr, mode);

	// Save filename for later retrieval
	strncpy(file_hndl->filename, open_filename, R1UP_FILE_MAX_PATH_LENGTH - 1);
	file_hndl->filename[R1UP_FILE_MAX_PATH_LENGTH - 1] = '\0';	// Ensure null termination

	// Detect file type after FSAL handle is set
	file_hndl->file_type = r1up_file_detect_file_type(file_hndl);

	// Initialize file metadata and check format
	r1up_file_result_t r1up_res = r1up_file_init_file_metadata(file_hndl);
	if (r1up_res != R1UP_FILE_SUCCESS && mode == R1UP_FILE_MODE_READ_ONLY) {
		fsal_close(file_hndl->fsal_handle_ptr);
		AG_FREE(fsal_handle_ptr);
		AG_FREE(file_hndl);
		return NULL;
	}

	// create list for header, body and footer blocks
	if (R1UP_FILE_SUCCESS != r1up_file_init_blocks_list_wrapper(file_hndl)) {
		fsal_close(file_hndl->fsal_handle_ptr);
		AG_FREE(fsal_handle_ptr);
		AG_FREE(file_hndl);
		return NULL;
	}

	// create variable stream format structure
	if (R1UP_FILE_SUCCESS != r1up_file_init_variables_stream_format(file_hndl)) {
		r1up_file_free_blocks_list(file_hndl);
		fsal_close(file_hndl->fsal_handle_ptr);
		AG_FREE(fsal_handle_ptr);
		AG_FREE(file_hndl);
		return NULL;
	}

	// Calculate current sample number based on variable stream data size.
	// Note: this function must be called after initializing the variable stream
	// format with r1up_file_rr_file_init_variables_stream_format()
	if (file_hndl->mode == R1UP_FILE_MODE_READ_ONLY) {
		// The sample number can be computed only in read mode
		if (R1UP_FILE_SUCCESS != r1up_file_compute_sample_number(file_hndl, file_hndl->var_stream_data_size,
																 &(file_hndl->total_sample_number))) {
			r1up_file_free_blocks_list(file_hndl);
			fsal_close(file_hndl->fsal_handle_ptr);
			AG_FREE(fsal_handle_ptr);
			AG_FREE(file_hndl);
			return NULL;
		}
	}

	// ── Deserialize lap table from footer block (READ mode only) ──────────────
	if (file_hndl->mode == R1UP_FILE_MODE_READ_ONLY) {
		if (R1UP_FILE_SUCCESS != r1up_file_deserialize_lap_table(file_hndl)) {
			r1up_file_free_blocks_list(file_hndl);
			fsal_close(file_hndl->fsal_handle_ptr);
			AG_FREE(fsal_handle_ptr);
			AG_FREE(file_hndl);
			return NULL;
		}
	}

	// Return file handle
	return (r1up_file_handle_t)file_hndl;
}

/*==================================================================================================================*/
static bool r1up_file_generate_auto_filename(char* out_filename, size_t out_filename_size) {
	if (out_filename == NULL || out_filename_size == 0U) {
		return false;
	}

	fsal_dir_handle_t dir;
	const char* scan_dir = (R1UP_FILE_DEFAULT_PATH_PREFIX[0] == '\0') ? "." : R1UP_FILE_DEFAULT_PATH_PREFIX;
	if (fsal_opendir(scan_dir, &dir) != FSAL_SUCCESS) {
		return false;
	}

	int32_t max_index = -1;
	fsal_dir_entry_t entry;

	while (fsal_readdir(&dir, &entry) == FSAL_SUCCESS) {
		/* Skip directories */
		if (entry.is_directory) {
			continue;
		}

		/* Check prefix "r1up_" */
		if (strncmp(entry.name, "r1up_", 5) != 0) {
			continue;
		}

		/* Check that characters 5..9 are all digits */
		if (!isdigit((unsigned char)entry.name[5]) || !isdigit((unsigned char)entry.name[6]) ||
			!isdigit((unsigned char)entry.name[7]) || !isdigit((unsigned char)entry.name[8]) ||
			!isdigit((unsigned char)entry.name[9])) {
			continue;
		}

		/* Check extension ".r1up" (case-insensitive) */
		const char* ext = &entry.name[10];
		if (ext[0] != '.' || tolower((unsigned char)ext[1]) != 'r' || tolower((unsigned char)ext[2]) != '1' ||
			tolower((unsigned char)ext[3]) != 'u' || tolower((unsigned char)ext[4]) != 'p' || ext[5] != '\0') {
			continue;
		}

		/* Extract the 5-digit number */
		char num_buf[6];
		memcpy(num_buf, &entry.name[5], 5);
		num_buf[5] = '\0';
		int32_t file_index = (int32_t)atoi(num_buf);

		if (file_index > max_index) {
			max_index = file_index;
		}
	}

	fsal_closedir(&dir);

	int32_t next_index = max_index + 1;
	if (next_index > 99999) {
		return false;
	}

	int written =
		snprintf(out_filename, out_filename_size, R1UP_FILE_DEFAULT_PATH_PREFIX "r1up_%05ld.r1up", (long)next_index);
	return (written > 0 && (size_t)written < out_filename_size);
}

/*===================================================================================================================*/
r1up_file_result_t r1up_file_close_header(r1up_file_handle_t handle) {
	// Parameter validation
	if (handle == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	if (file->fsal_handle_ptr == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	// check if file is in write mode
	if (file->mode != R1UP_FILE_MODE_WRITE_ONLY) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}

	// Check if header is already closed
	if (file->hdr_closed) {
		return R1UP_FILE_SUCCESS;
	}

	// get the block with R1UP_FILE_HEADER_BID
	r1up_file_block_t* header_block = r1up_file_get_block(file, R1UP_FILE_HEADER_BID);
	if (header_block == NULL) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}

	r1up_file_size_t total_hdr_size = 0U;
	if (R1UP_FILE_SUCCESS != r1up_file_cmp_blocks_list_size(file, R1UP_FILE_HEADER_BID, &total_hdr_size)) {
		return R1UP_FILE_ERROR_UNSUPPORTED;
	}
	// set payload to the length of the header blocks
	*((uint32_t*)header_block->pl) = (uint32_t)total_hdr_size;

	// write header blocks to file
	// Move file pointer to the beginning
	fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	// Write all header blocks to file
	r1up_file_size_t bytes_written = 0U;
	r1up_file_result_t r1up_res = r1up_file_write_blocks_to_file(file, R1UP_FILE_HEADER_BID, &bytes_written);
	if (r1up_res != R1UP_FILE_SUCCESS) {
		return r1up_res;
	}
	if (bytes_written != total_hdr_size) {
		return R1UP_FILE_ERROR_IO;
	}
	// Update header length in file structure
	file->hdr_ln = total_hdr_size;
	// Update file size in file structure
	file->file_size = file->hdr_ln;

	r1up_res = r1up_file_check_file_size_consistency(file);
	if (r1up_res != R1UP_FILE_SUCCESS) {
		return r1up_res;
	}

	// Mark header as closed
	file->hdr_closed = true;

	// Prepare the file for body writing
	file->body_offset_start = total_hdr_size;
	file->body_offset_end = total_hdr_size;	 // start and end are the same until body is written

	r1up_file_block_t* body_block = r1up_file_get_block(file, R1UP_FILE_BODY_BID);
	if (body_block == NULL) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}
	// Set payload to the length of the body blocks that currently is R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t) +
	// body_block->sz (the size is zero)
	*((uint32_t*)body_block->pl) = (uint32_t)(R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t) + body_block->sz);

	// Move file pointer to the end of the header blocks, i.e. the start of the body blocks
	fsal_res = fsal_seek(file->fsal_handle_ptr, file->body_offset_start, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}
	// Write body blocks to file
	r1up_res = r1up_file_write_blocks_to_file(file, R1UP_FILE_BODY_BID, &bytes_written);
	if (r1up_res != R1UP_FILE_SUCCESS) {
		return r1up_res;
	}
	if (bytes_written != (R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t) + body_block->sz)) {
		return R1UP_FILE_ERROR_IO;
	}
	// Update body length in file structure
	file->bdy_ln = (uint32_t)bytes_written;
	// Update file size in file structure
	file->file_size = file->hdr_ln + file->bdy_ln;
	// check file size consistency
	r1up_res = r1up_file_check_file_size_consistency(file);
	if (r1up_res != R1UP_FILE_SUCCESS) {
		return r1up_res;
	}
	// Set body end offset to current position
	long pos = 0;
	fsal_result_t fsal_result = fsal_tell(file->fsal_handle_ptr, &pos);
	if (fsal_result != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_result);
	}
	file->body_offset_end = pos;

	// Mark body as open
	file->bdy_closed = false;
	fsal_flush(file->fsal_handle_ptr);

	// After the body is written, the file is ready to write variable stream data
	// Initialize variable stream data write buffer
	if (R1UP_FILE_SUCCESS != r1up_file_init_vsd_write_buffer(file)) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	return R1UP_FILE_SUCCESS;
}

/*===================================================================================================================*/
static r1up_file_result_t r1up_file_close_body(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	// check if file is in write mode
	if (file->mode != R1UP_FILE_MODE_WRITE_ONLY) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}
	// Check if header is closed
	if (!file->hdr_closed) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}
	// Check if body is already closed
	if (file->bdy_closed) {
		return R1UP_FILE_SUCCESS;
	}

	// check consistency of body length related parameters
	fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_END);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	long pos = 0;
	fsal_res = fsal_tell(file->fsal_handle_ptr, &pos);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}
	file->body_offset_end = (r1up_file_size_t)pos;
	file->bdy_ln = file->body_offset_end - file->body_offset_start;

	if (file->bdy_ln != pos - file->hdr_ln) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}
	// other check
	uint32_t expected_bdy_ln =
		(file->vsd_bytes_written == 0 ? 4U + R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t)
									  : file->vsd_bytes_written + 4U + R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t));

	if (file->bdy_ln != expected_bdy_ln) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}

	// Update body length block payload
	// Move file pointer to the body payload to write correct body length
	pos = (long)(file->body_offset_start + R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t));
	fsal_res = fsal_seek(file->fsal_handle_ptr, pos, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}
	// write body length to file
	size_t bytes_written = 0;
	fsal_res = fsal_write(&(file->bdy_ln), 1U, sizeof(uint32_t), file->fsal_handle_ptr, &bytes_written);
	if (fsal_res != FSAL_SUCCESS || bytes_written != sizeof(uint32_t)) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	// Mark body as closed
	file->bdy_closed = true;

	// move pointer to the end of the file, i.e. the end of the body
	fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_END);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	fsal_flush(file->fsal_handle_ptr);
	return R1UP_FILE_SUCCESS;
}

/*===================================================================================================================*/
/**
 * @brief Deserialize R1UP_FILE_LAP_TBL_BID block payload → in-RAM lap_table.
 * @details Binary format (little-endian, native byte order):
 *          ┌──────────────┬─────────────────────────────────────────────┐
 *          │ uint32_t     │ count — number of lap markers              │
 *          ├──────────────┼─────────────────────────────────────────────┤
 *          │ uint64_t     │ markers[0].sample_index                    │
 *          │ uint64_t     │ markers[0].file_offset                     │
 *          │   …          │   …                                        │
 *          │ uint64_t     │ markers[count-1].sample_index              │
 *          │ uint64_t     │ markers[count-1].file_offset               │
 *          └──────────────┴─────────────────────────────────────────────┘
 */
static r1up_file_result_t r1up_file_deserialize_lap_table(r1up_file_t* file) {
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// Locate the block
	r1up_file_block_t* lap_blck = r1up_file_get_block(file, R1UP_FILE_LAP_TBL_BID);
	if (lap_blck == NULL) {
		// Block not present in file — not an error, just no lap data
		return R1UP_FILE_SUCCESS;
	}

	// Empty payload — no lap markers recorded during acquisition
	if (lap_blck->sz == 0U || lap_blck->pl == NULL) {
		return R1UP_FILE_SUCCESS;
	}

	const uint8_t* buf = (const uint8_t*)lap_blck->pl;
	const uint32_t payload_size = lap_blck->sz;

	// Minimum payload: at least the count field
	if (payload_size < sizeof(uint32_t)) {
		return R1UP_FILE_ERROR_CORRUPTED;
	}

	// Read count
	uint32_t count = 0U;
	memcpy(&count, buf, sizeof(uint32_t));

	// Validate payload size consistency
	const uint32_t expected_size = (uint32_t)sizeof(uint32_t) + count * (uint32_t)sizeof(r1up_lap_marker_t);
	if (payload_size != expected_size) {
		return R1UP_FILE_ERROR_CORRUPTED;
	}

	if (count == 0U) {
		// Count is zero — nothing to allocate
		return R1UP_FILE_SUCCESS;
	}

	// Allocate markers array
	r1up_lap_marker_t* markers = (r1up_lap_marker_t*)AG_MALLOC(count * sizeof(r1up_lap_marker_t));
	if (markers == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	// Copy marker data
	memcpy(markers, buf + sizeof(uint32_t), count * sizeof(r1up_lap_marker_t));

	// Populate the lap_table
	file->lap_table.markers = markers;
	file->lap_table.count = count;
	file->lap_table.capacity = count;

	return R1UP_FILE_SUCCESS;
}

/*===================================================================================================================*/
/**
 * @brief Serialize lap_table → R1UP_FILE_LAP_TBL_BID block payload.
 * @details Binary format (little-endian, native byte order):
 *          ┌──────────────┬─────────────────────────────────────────────┐
 *          │ uint32_t     │ count — number of lap markers              │
 *          ├──────────────┼─────────────────────────────────────────────┤
 *          │ uint64_t     │ markers[0].sample_index                    │
 *          │ uint64_t     │ markers[0].file_offset                     │
 *          │   …          │   …                                        │
 *          │ uint64_t     │ markers[count-1].sample_index              │
 *          │ uint64_t     │ markers[count-1].file_offset               │
 *          └──────────────┴─────────────────────────────────────────────┘
 */
static r1up_file_result_t r1up_file_serialize_lap_table(r1up_file_t* file) {
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// Nothing to serialize — leave block payload empty (sz = 0, pl = NULL)
	if (file->lap_table.count == 0U) {
		return R1UP_FILE_SUCCESS;
	}

	// Locate the block
	r1up_file_block_t* lap_blck = r1up_file_get_block(file, R1UP_FILE_LAP_TBL_BID);
	if (lap_blck == NULL) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}

	// Compute payload size:  uint32_t count  +  count × sizeof(r1up_lap_marker_t)
	const uint32_t marker_array_bytes = file->lap_table.count * (uint32_t)sizeof(r1up_lap_marker_t);
	const uint32_t payload_size = (uint32_t)sizeof(uint32_t) + marker_array_bytes;

	// Free any previous payload (defensive — should be NULL on first call)
	if (lap_blck->pl != NULL) {
		AG_FREE(lap_blck->pl);
		lap_blck->pl = NULL;
		lap_blck->sz = 0U;
	}

	// Allocate and fill
	uint8_t* buf = (uint8_t*)AG_MALLOC(payload_size);
	if (buf == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	// Write count
	memcpy(buf, &file->lap_table.count, sizeof(uint32_t));

	// Write marker array
	memcpy(buf + sizeof(uint32_t), file->lap_table.markers, marker_array_bytes);

	// Assign to block
	lap_blck->pl = buf;
	lap_blck->sz = payload_size;

	return R1UP_FILE_SUCCESS;
}

/*===================================================================================================================*/
static r1up_file_result_t r1up_file_close_footer(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	// check if file is in write mode
	if (file->mode != R1UP_FILE_MODE_WRITE_ONLY) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}

	// Check if footer is already closed
	if (file->ftr_closed) {
		return R1UP_FILE_SUCCESS;
	}

	// ── Serialize lap table into its footer block before computing total size ──
	r1up_file_result_t lap_res = r1up_file_serialize_lap_table(file);
	if (lap_res != R1UP_FILE_SUCCESS) {
		return lap_res;
	}

	// get the block with R1UP_FILE_FOOTER_BID
	r1up_file_block_t* footer_block = r1up_file_get_block(file, R1UP_FILE_FOOTER_BID);
	if (footer_block == NULL) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}

	r1up_file_size_t total_ftr_size = 0U;
	if (R1UP_FILE_SUCCESS != r1up_file_cmp_blocks_list_size(file, R1UP_FILE_FOOTER_BID, &total_ftr_size)) {
		return R1UP_FILE_ERROR_UNSUPPORTED;
	}
	// set payload to the length of the footer blocks
	*((uint32_t*)footer_block->pl) = (uint32_t)total_ftr_size;

	// set file pointer to the end of the body, i.e. the end of the file
	fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, file->body_offset_end, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	// write footer blocks to file
	r1up_file_size_t bytes_written = 0U;
	r1up_file_result_t res = r1up_file_write_blocks_to_file(file, R1UP_FILE_FOOTER_BID, &bytes_written);
	if (res != R1UP_FILE_SUCCESS) {
		return res;
	}
	if (bytes_written != total_ftr_size) {
		return R1UP_FILE_ERROR_IO;
	}
	file->ftr_ln = total_ftr_size;

	// Mark footer as closed
	file->ftr_closed = true;

	// Update file size
	file->file_size = file->hdr_ln + file->bdy_ln + file->ftr_ln;

	fsal_flush(file->fsal_handle_ptr);
	return R1UP_FILE_SUCCESS;
}

/*===================================================================================================================*/
static r1up_file_result_t r1up_file_check_file_size_consistency(r1up_file_t* file) {
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	if (file->fsal_handle_ptr == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// Save current file position
	long current_pos = 0;
	fsal_result_t fsal_res = fsal_tell(file->fsal_handle_ptr, &current_pos);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	// Move to end of file to get actual size
	long pos = 0;
	fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_END);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}
	fsal_res = fsal_tell(file->fsal_handle_ptr, &pos);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}
	r1up_file_size_t actual_file_size = (r1up_file_size_t)pos;

	// Check consistency
	if (file->hdr_ln + file->bdy_ln + file->ftr_ln != actual_file_size) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}

	// Restore original file position
	fsal_res = fsal_seek(file->fsal_handle_ptr, current_pos, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	return R1UP_FILE_SUCCESS;
}

/*===================================================================================================================*/
int32_t r1up_file_get_var_position(r1up_file_handle_t handle, r1up_variable_id_t var_id) {
	if (handle == NULL) {
		return -1;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	if (file->vsf == NULL) {
		return -1;
	}

	// Return position by linear scan over VSF (-1 if variable not present in file)
	return r1up_file_find_var_position(file, var_id);
}

/*===================================================================================================================*/
r1up_file_result_t r1up_file_close(r1up_file_handle_t handle) {
	// Parameter validation
	if (handle == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	if (file->fsal_handle_ptr == NULL) {
		(void)r1up_file_deinit_all(file);
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	/* Cleanup streaming resources if initialized */
	if (file->stream_initialized) {
		(void)r1up_file_stream_body_end(handle);
	}

	switch (file->mode) {
		case R1UP_FILE_MODE_READ_ONLY:
			/* code */
			break;
		case R1UP_FILE_MODE_WRITE_ONLY:
			// If header is not closed, close it
			if (!file->hdr_closed) {
				r1up_file_result_t res = r1up_file_close_header(handle);
				if (res != R1UP_FILE_SUCCESS) {
					(void)r1up_file_deinit_all(file);
					return res;
				}
			}

			// If body is not closed, close it
			if (!file->bdy_closed) {
				r1up_file_result_t res = r1up_file_close_body(file);
				if (res != R1UP_FILE_SUCCESS) {
					(void)r1up_file_deinit_all(file);
					return res;
				}
			}

			// if footer is not closed, close it
			if (!file->ftr_closed) {
				r1up_file_result_t res = r1up_file_close_footer(file);
				if (res != R1UP_FILE_SUCCESS) {
					(void)r1up_file_deinit_all(file);
					return res;
				}
			}
			break;
		default:
			(void)r1up_file_deinit_all(file);
			return R1UP_FILE_ERROR_INVALID_STATE;
	}

	return r1up_file_deinit_all(file);
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_add_custom_block(r1up_file_handle_t handle, const char* section_id, const char* id_string,
											  void* pl, uint32_t sz) {
	// Parameter validation
	if (handle == NULL || section_id == NULL || id_string == NULL || pl == NULL || sz == 0)
		return R1UP_FILE_ERROR_INVALID_PARAM;

	// IMPORTANT: section_id must point to a buffer with at least R1UP_FILE_BLCK_ID_LENGTH readable bytes
	// The function will read exactly R1UP_FILE_BLCK_ID_LENGTH bytes from section_id regardless of null termination
	// Caller is responsible for ensuring the buffer is large enough to avoid buffer overrun
	// Examples of valid usage:
	//   - char id[R1UP_FILE_BLCK_ID_LENGTH] = "MY_CUSTOM_BLOCK_"; (exactly R1UP_FILE_BLCK_ID_LENGTH chars)
	//   - char id[] = "MY_CUSTOM_BLOCK_ID_WITH_MORE_CHARS"; (>R1UP_FILE_BLCK_ID_LENGTH chars, first
	//   R1UP_FILE_BLCK_ID_LENGTH will be used)
	//   - char id[20] = "SHORT"; (buffer size >=R1UP_FILE_BLCK_ID_LENGTH, content can be shorter)
	// Examples of INVALID usage:
	//   - char id[8] = "SHORT"; (buffer too small - will cause buffer overrun!)
	//   - r1up_file_set_custom_block(handle, "SHORT", ...); (string literal may not guarantee R1UP_FILE_BLCK_ID_LENGTH
	//   bytes)

	r1up_file_t* file = (r1up_file_t*)handle;

	if (r1up_file_string_exists_in_blocks_list(file, id_string)) {
		// block with same id_string already exists
		return R1UP_FILE_ERROR_ALREADY_EXISTS;
	}

	if (section_id == R1UP_FILE_HEADER_BID) {
		if (file->hdr_closed) {
			return R1UP_FILE_ERROR_INVALID_STATE;
		}
		// Add new block to header section (before BODY block)
		r1up_file_block_node_t* new_node = AG_MALLOC(sizeof(r1up_file_block_node_t));
		if (new_node == NULL) {
			return R1UP_FILE_ERROR_MEMORY;
		}

		// Initialize block ID using factory function for runtime-allocated strings
		// This ensures memory is properly allocated and managed
		new_node->blck.id = r1up_file_block_id_create_dynamic(id_string);
		if (r1up_file_block_id_get_string(&new_node->blck.id) == NULL) {
			AG_FREE(new_node);
			return R1UP_FILE_ERROR_MEMORY;
		}

		new_node->blck.sz = sz;
		new_node->blck.pl = AG_MALLOC(sz);
		if (new_node->blck.pl == NULL) {
			r1up_file_block_id_free(&new_node->blck.id);
			AG_FREE(new_node);
			return R1UP_FILE_ERROR_MEMORY;
		}
		memcpy(new_node->blck.pl, pl, sz);
		new_node->blck.is_writable = true;	// User-added blocks are writable

		// Find the position to insert (before BODY block)
		r1up_file_block_node_t* current = file->blocks_head;
		r1up_file_block_node_t* prev = NULL;

		// Find the BODY block
		r1up_file_block_id_t body_id = r1up_file_block_id_create_static(R1UP_FILE_BODY_BID);
		while (current != NULL && !r1up_file_block_id_equals(&current->blck.id, &body_id)) {
			prev = current;
			current = current->next;
		}

		// Insert new node before BODY block
		if (prev != NULL) {
			prev->next = new_node;
			new_node->next = current;
		} else {
			// This shouldn't happen if the list is properly initialized
			new_node->next = file->blocks_head;
			file->blocks_head = new_node;
		}
	} else if (section_id == R1UP_FILE_BODY_BID) {
		// you can never set the body block, it is managed internally
		return R1UP_FILE_ERROR_INVALID_PARAM;
	} else if (section_id == R1UP_FILE_FOOTER_BID) {
		if (file->ftr_closed) {
			return R1UP_FILE_ERROR_INVALID_STATE;
		}
		// Add new block to footer section (at the end of the list)
		r1up_file_block_node_t* new_node = AG_MALLOC(sizeof(r1up_file_block_node_t));
		if (new_node == NULL) {
			return R1UP_FILE_ERROR_MEMORY;
		}

		// Initialize block ID using factory function for runtime-allocated strings
		new_node->blck.id = r1up_file_block_id_create_dynamic(id_string);
		if (r1up_file_block_id_get_string(&new_node->blck.id) == NULL) {
			AG_FREE(new_node);
			return R1UP_FILE_ERROR_MEMORY;
		}

		new_node->blck.sz = sz;
		new_node->blck.pl = AG_MALLOC(sz);
		if (new_node->blck.pl == NULL) {
			r1up_file_block_id_free(&new_node->blck.id);
			AG_FREE(new_node);
			return R1UP_FILE_ERROR_MEMORY;
		}
		memcpy(new_node->blck.pl, pl, sz);
		new_node->blck.is_writable = true;	// User-added blocks are writable
		new_node->next = NULL;

		// Find the end of the list and append
		r1up_file_block_node_t* current = file->blocks_head;
		if (current == NULL) {
			file->blocks_head = new_node;
		} else {
			while (current->next != NULL) {
				current = current->next;
			}
			current->next = new_node;
		}
	} else {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}  // else not valid id_string, this works only for header, body and footer blocks

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
void* r1up_file_get_payload(r1up_file_handle_t handle, const char* id_string) {
	if (handle == NULL || id_string == NULL) {
		return NULL;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	r1up_file_block_id_t search_id = r1up_file_block_id_create_static(id_string);
	r1up_file_block_node_t* current = file->blocks_head;
	while (current != NULL) {
		if (r1up_file_block_id_equals(&current->blck.id, &search_id)) {
			return current->blck.pl;
		}
		current = current->next;
	}
	return NULL;
}

/*==================================================================================================================*/
/**
 * @brief Deinitialize the body info cache arrays
 * @details Frees all memory allocated for unpacked VSF arrays and resets the initialization flag
 * @param file Pointer to the file instance
 * @return R1UP_FILE_SUCCESS on success, R1UP_FILE_ERROR_MEMORY if deallocation fails
 */
static r1up_file_result_t r1up_file_deinit_body_info(r1up_file_t* file) {
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	bool error = false;

	if (file->body_info_initialized) {
		if (file->body_info_ids && AG_FREE(file->body_info_ids) != AG_MAL_OK) error = true;
		if (file->body_info_types && AG_FREE(file->body_info_types) != AG_MAL_OK) error = true;
		if (file->body_info_sizes && AG_FREE(file->body_info_sizes) != AG_MAL_OK) error = true;
		if (file->body_info_fds && AG_FREE(file->body_info_fds) != AG_MAL_OK) error = true;

		file->body_info_ids = NULL;
		file->body_info_types = NULL;
		file->body_info_sizes = NULL;
		file->body_info_fds = NULL;
		file->body_info_initialized = false;
	}

	return error ? R1UP_FILE_ERROR_MEMORY : R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/**
 * @brief Get body metadata and unpacked VSF information
 * @details Provides read-only access to file body metadata. Memory is managed internally
 *          and remains valid until file is closed. Uses lazy initialization - first call
 *          allocates cache, subsequent calls return cached data.
 *
 * @param handle File handle from r1up_file_open()
 * @param info Pointer to structure to populate (allocated by caller on stack)
 * @return R1UP_FILE_SUCCESS on success, error code otherwise
 *
 * @note First call allocates internal cache, subsequent calls reuse it (O(1))
 * @note All pointers in info remain valid until r1up_file_close()
 * @note Thread-safe for same handle (idempotent initialization)
 */
r1up_file_result_t r1up_file_get_body_info(r1up_file_handle_t handle, r1up_file_body_info_t* info) {
	// Validation
	if (handle == NULL || info == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	// Lazy initialization - allocate only on first call
	if (!file->body_info_initialized) {
		uint32_t var_count = file->varible_number;

		// Allocate arrays
		file->body_info_ids = (uint32_t*)AG_MALLOC(var_count * sizeof(uint32_t));
		file->body_info_types = (uint32_t*)AG_MALLOC(var_count * sizeof(uint32_t));
		file->body_info_sizes = (uint32_t*)AG_MALLOC(var_count * sizeof(uint32_t));
		file->body_info_fds = (uint32_t*)AG_MALLOC(var_count * sizeof(uint32_t));

		// Check allocations
		if (file->body_info_ids == NULL || file->body_info_types == NULL || file->body_info_sizes == NULL ||
			file->body_info_fds == NULL) {
			// Cleanup partial allocations
			if (file->body_info_ids) AG_FREE(file->body_info_ids);
			if (file->body_info_types) AG_FREE(file->body_info_types);
			if (file->body_info_sizes) AG_FREE(file->body_info_sizes);
			if (file->body_info_fds) AG_FREE(file->body_info_fds);

			file->body_info_ids = NULL;
			file->body_info_types = NULL;
			file->body_info_sizes = NULL;
			file->body_info_fds = NULL;

			return R1UP_FILE_ERROR_MEMORY;
		}

		// Unpack VSF into separate arrays
		uint32_t i;
		for (i = 0; i < var_count; i++) {
			file->body_info_ids[i] = file->vsf[i].id;
			file->body_info_types[i] = file->vsf[i].type;
			file->body_info_sizes[i] = file->vsf[i].sz;
			file->body_info_fds[i] = file->vsf[i].fd;
		}

		file->body_info_initialized = true;
	}

	// Populate output structure (points to internal memory)
	info->total_samples = file->total_sample_number;
	info->variable_count = file->varible_number;

	info->variable_ids = file->body_info_ids;
	info->variable_types = file->body_info_types;
	info->data_type_sizes = file->body_info_sizes;
	info->freq_divisors = file->body_info_fds;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_set_block_payload(r1up_file_handle_t handle, const char* id_string, void* pl,
											   uint32_t sz) {
	// Parameter validation
	if (handle == NULL || id_string == NULL || pl == NULL || sz == 0) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	// Check if block exists
	r1up_file_block_node_t* block_node = r1up_file_get_block_node(file, id_string);
	if (block_node == NULL) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}

	// Check if block is writable
	if (!block_node->blck.is_writable) {
		return R1UP_FILE_ERROR_ACCESS;
	}

	// Check if header/body/footer is closed based on block type

	if (r1up_file_is_header_block(file, id_string) && file->hdr_closed) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}
	if (r1up_file_is_body_block(file, id_string) && file->bdy_closed) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}
	if (r1up_file_is_footer_block(file, id_string) && file->ftr_closed) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}

	// Free existing payload if any
	if (block_node->blck.pl != NULL) {
		AG_FREE(block_node->blck.pl);
		block_node->blck.pl = NULL;
		block_node->blck.sz = 0;
	}

	// Allocate new payload and copy data
	block_node->blck.pl = AG_MALLOC(sz);
	if (block_node->blck.pl == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}
	memcpy(block_node->blck.pl, pl, sz);
	block_node->blck.sz = sz;

	return R1UP_FILE_SUCCESS;
}

#ifdef UNIT_TESTING
/*==================================================================================================================*/
void r1up_file_debug_helper_print_block_list(r1up_file_handle_t handle) {
#if R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS == 0
	(void)handle;  // Suppress unused parameter warning when debug prints are disabled
#else
	if (handle == NULL) return;

	r1up_file_t* file = (r1up_file_t*)handle;
	// Print block list
	const r1up_file_block_node_t* cp = file->blocks_head;

	while (cp != NULL) {
		const char* id_str = r1up_file_block_id_get_string(&cp->blck.id);
		R1UP_FILE_DEBUG_PRINT("Block ID: %.32s, Size: %u, Payload: ", id_str ? id_str : "(null)", cp->blck.sz);

		if (cp->blck.pl == NULL) {
			R1UP_FILE_DEBUG_PRINT("NULL");
		} else if (cp->blck.sz == 0) {
			R1UP_FILE_DEBUG_PRINT("(empty)");
		} else {
			// Stampa i primi byte in esadecimale (max 16 bytes per leggibilità)
			uint32_t bytes_to_print = cp->blck.sz > 16 ? 16 : cp->blck.sz;
			uint32_t i;
			for (i = 0; i < bytes_to_print; i++) {
				R1UP_FILE_DEBUG_PRINT("%02X ", ((unsigned char*)cp->blck.pl)[i]);
			}
			// If the payload is larger than 16 bytes, indicate that there is more data
			if (cp->blck.sz > 16) {
				R1UP_FILE_DEBUG_PRINT("... (%u bytes total)", cp->blck.sz);
			}
			// Stampa come testo, limitando a 16 caratteri massimo per leggibilità
			uint32_t text_chars_to_print = cp->blck.sz > 16 ? 16 : cp->blck.sz;
			R1UP_FILE_DEBUG_PRINT("'%.*s'", (int)text_chars_to_print, (char*)cp->blck.pl);
			if (cp->blck.sz > 16) {
				R1UP_FILE_DEBUG_PRINT("...");
			}
		}

		R1UP_FILE_DEBUG_PRINT(", Writable: %s\n", cp->blck.is_writable ? "Yes" : "No");
		cp = cp->next;
	}
#endif
}

/*==================================================================================================================*/
void r1up_file_debug_helper_print_block_ids(r1up_file_handle_t handle) {
#if R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS == 0
	(void)handle;  // Suppress unused parameter warning when debug prints are disabled
#else
	if (handle == NULL) return;

	r1up_file_t* file = (r1up_file_t*)handle;
	// Print block IDs
	const r1up_file_block_node_t* cp = file->blocks_head;

	R1UP_FILE_DEBUG_PRINT("=================================================================\n");
	R1UP_FILE_DEBUG_PRINT("Block IDs in the file:\n");
	R1UP_FILE_DEBUG_PRINT("=================================================================\n");

	int block_count = 0;
	while (cp != NULL) {
		R1UP_FILE_DEBUG_PRINT("\n[Block #%d]\n", block_count);
		R1UP_FILE_DEBUG_PRINT("  Static ID:    %.16s\n", cp->blck.id.static_id ? cp->blck.id.static_id : "(null)");
		R1UP_FILE_DEBUG_PRINT("  Static ptr:   %p\n", (void*)cp->blck.id.static_id);
		R1UP_FILE_DEBUG_PRINT("  Dynamic ID:   %.16s\n", cp->blck.id.dynamic_id ? cp->blck.id.dynamic_id : "(null)");
		R1UP_FILE_DEBUG_PRINT("  Dynamic ptr:  %p\n", (void*)cp->blck.id.dynamic_id);
		R1UP_FILE_DEBUG_PRINT("  Is static:    %s\n", cp->blck.id.is_static ? "true" : "false");
		R1UP_FILE_DEBUG_PRINT("  Size:         %u bytes\n", cp->blck.sz);
		R1UP_FILE_DEBUG_PRINT("  Writable:     %s\n", cp->blck.is_writable ? "Yes" : "No");

		cp = cp->next;
		block_count++;
	}

	R1UP_FILE_DEBUG_PRINT("\n=================================================================\n");
	R1UP_FILE_DEBUG_PRINT("Total blocks: %d\n", block_count);
	R1UP_FILE_DEBUG_PRINT("=================================================================\n");
#endif
}
/*==================================================================================================================*/
void r1up_file_debug_helper_print_variables_stream_format(r1up_file_handle_t handle) {
#if R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS == 0
	(void)handle;  // Suppress unused parameter warning when debug prints are disabled
#else
	if (handle == NULL) return;

	r1up_file_t* file = (r1up_file_t*)handle;
	if (file->vsf == NULL) {
		R1UP_FILE_DEBUG_PRINT("No variables stream format defined.\n");
		return;
	}

	R1UP_FILE_DEBUG_PRINT("Variables Stream Format:\n");
	R1UP_FILE_DEBUG_PRINT("Number of Variables: %u\n", file->varible_number);
	uint32_t i;
	for (i = 0; i < file->varible_number; i++) {
		r1up_file_vsf_t* var = &file->vsf[i];
		R1UP_FILE_DEBUG_PRINT("Index %u: ID: %u, Type: %u, Size: %u, Divisor: %u\n", i, var->id, var->type, var->sz,
							  var->fd);
	}
#endif
}

/*==================================================================================================================*/
void r1up_file_debug_helper_print_metadata(r1up_file_handle_t handle) {
#if R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS == 0
	(void)handle;  // Suppress unused parameter warning when debug prints are disabled
#else
	if (handle == NULL) return;

	r1up_file_t* file = (r1up_file_t*)handle;

	R1UP_FILE_DEBUG_PRINT("R1UP File Metadata:\n");
	R1UP_FILE_DEBUG_PRINT("Header Length: %lu bytes\n", (unsigned long)file->hdr_ln);
	R1UP_FILE_DEBUG_PRINT("Body Length:   %lu bytes\n", (unsigned long)file->bdy_ln);
	R1UP_FILE_DEBUG_PRINT("Footer Length: %lu bytes\n", (unsigned long)file->ftr_ln);
	R1UP_FILE_DEBUG_PRINT("File Size:     %lu bytes\n", (unsigned long)file->file_size);
	R1UP_FILE_DEBUG_PRINT("Body Offset Start: %lu\n", (unsigned long)file->body_offset_start);
	R1UP_FILE_DEBUG_PRINT("Body Offset End:   %lu\n", (unsigned long)file->body_offset_end);
	R1UP_FILE_DEBUG_PRINT("Body Var Data Start Offset: %lu\n", (unsigned long)file->body_offset_var_data_start);
	R1UP_FILE_DEBUG_PRINT("Body Var Data Size: %lu bytes\n", (unsigned long)file->var_stream_data_size);
	R1UP_FILE_DEBUG_PRINT("Total Sample Number: %lu\n", (unsigned long)file->total_sample_number);

	R1UP_FILE_DEBUG_PRINT("Body Var Data Size Calc 1: %lu\n",
						  (unsigned long)(file->body_offset_end - file->body_offset_start -
										  (4U + R1UP_FILE_BLCK_ID_LENGTH + sizeof(uint32_t))));
	R1UP_FILE_DEBUG_PRINT("Body Var Data Size Calc 2: %lu\n",
						  (unsigned long)(file->body_offset_end - file->body_offset_var_data_start));

	R1UP_FILE_DEBUG_PRINT("Header Closed: %s\n", file->hdr_closed ? "Yes" : "No");
	R1UP_FILE_DEBUG_PRINT("Body Closed:   %s\n", file->bdy_closed ? "Yes" : "No");
	R1UP_FILE_DEBUG_PRINT("Footer Closed: %s\n", file->ftr_closed ? "Yes" : "No");
#endif
}
/*==================================================================================================================*/
void r1up_file_debug_helper_print_blockid(r1up_file_t* file, r1up_file_block_id_t* block_id) {
#if R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS == 0
	(void)file;	 // Suppress unused parameter warning when debug prints are disabled
	(void)block_id;
#else
	if (file == NULL || block_id == NULL) return;

	R1UP_FILE_DEBUG_PRINT("\n[Block]\n");
	R1UP_FILE_DEBUG_PRINT("  Static ID:    %.16s\n", block_id->static_id ? block_id->static_id : "(null)");
	R1UP_FILE_DEBUG_PRINT("  Static ptr:   %p\n", (void*)block_id->static_id);
	R1UP_FILE_DEBUG_PRINT("  Dynamic ID:   %.16s\n", block_id->dynamic_id ? block_id->dynamic_id : "(null)");
	R1UP_FILE_DEBUG_PRINT("  Dynamic ptr:  %p\n", (void*)block_id->dynamic_id);
	R1UP_FILE_DEBUG_PRINT("  Is static:    %s\n", block_id->is_static ? "true" : "false");
#endif
}

/*==================================================================================================================*/
void r1up_file_debug_helper_print_vsf(r1up_file_handle_t handle) {
#if R1UP_FILE_ENABLE_DEBUG_HELPER_PRINTS == 0
	(void)handle;  // Suppress unused parameter warning when debug prints are disabled
#else
	if (handle == NULL) return;
	r1up_file_t* file = (r1up_file_t*)handle;
	_r1up_file_debug_helper_print_vsf(file);
#endif
}

static void _r1up_file_debug_helper_print_vsf(r1up_file_t* file) {
	if (file == NULL || file->vsf == NULL) return;

	R1UP_FILE_DEBUG_PRINT("\n[Variables Stream Format]\n");
	R1UP_FILE_DEBUG_PRINT("  Variable count: %u\n", file->varible_number);
	uint32_t i;
	for (i = 0; i < file->varible_number; i++) {
		R1UP_FILE_DEBUG_PRINT("  [%u] ID: %u, Type: %u, Size: %u, Freq Div: %u\n", i, file->vsf[i].id,
							  file->vsf[i].type, file->vsf[i].sz, file->vsf[i].fd);
	}
}
#endif /* UNIT_TESTING */
/*==================================================================================================================*/
void r1up_file_set_var(r1up_file_handle_t handle, r1up_variable_id_t var_id, const void* data) {
	if (handle == NULL || data == NULL) return;

	r1up_file_t* file = (r1up_file_t*)handle;

	// Check if body is closed
	if (file->bdy_closed) return;

	// before starting to update variables, ensure that the header is closed
	if (!file->hdr_closed) {
		r1up_file_result_t res = r1up_file_close_header(handle);
		if (res != R1UP_FILE_SUCCESS) {
			return;
		}
	}

	if (file->vsf == NULL) return;

	// Check if write buffer is initialized (this happens only after header is closed)
	if (file->vsd_write_buffer == NULL) {
		// Buffer not yet initialized - header must be closed first
		return;
	}

	// Find variable position in file using linear scan (O(N), N ≤ 28)
	int32_t pos = r1up_file_find_var_position(file, var_id);
	if (pos < 0) {
		return; /* Variable not present in this file */
	}

	// Copy data into write buffer using position, not var_id
	memcpy(file->vsd_write_buffer[pos], data, file->vsf[pos].sz);
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_mark_lap(r1up_file_handle_t handle) {
	if (handle == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;

	r1up_file_t* file = (r1up_file_t*)handle;

	// Only allowed in WRITE mode while body is open
	if (file->mode != R1UP_FILE_MODE_WRITE_ONLY) return R1UP_FILE_ERROR_INVALID_STATE;
	if (file->bdy_closed) return R1UP_FILE_ERROR_INVALID_STATE;

	// Raise the pending flag — actual marker recording is deferred to r1up_file_update_vars().
	// This function is ISR-safe: no I/O, no allocation, no locking.
	// TODO: ensure atomic write on target platform (volatile, atomic_flag, or critical section)
	file->lap_mark_pending = true;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_lap_count(r1up_file_handle_t handle, uint32_t* lap_count) {
	if (handle == NULL || lap_count == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;

	const r1up_file_t* file = (const r1up_file_t*)handle;

	// Complete laps = crossing count - 1 (excludes outlap and inlap).
	// Need at least 2 crossings to define one complete lap.
	*lap_count = (file->lap_table.count >= 2U) ? (file->lap_table.count - 1U) : 0U;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_lap_sample_range(r1up_file_handle_t handle, uint32_t lap_number,
												  r1up_file_size_t* start_sample_index,
												  r1up_file_size_t* end_sample_index) {
	if (handle == NULL || start_sample_index == NULL || end_sample_index == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	const r1up_file_t* file = (const r1up_file_t*)handle;
	const uint32_t crossing_count = file->lap_table.count;

	// Need at least 1 crossing to define any segment
	if (crossing_count == 0U) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}

	// lap_count (complete laps) = crossing_count - 1 (when >= 2, else 0)
	// Total segments = crossing_count + 1  (outlap + lap_count + inlap)
	// Valid lap_number: 0 … crossing_count  (inclusive)
	if (lap_number > crossing_count) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	const r1up_lap_marker_t* markers = file->lap_table.markers;

	if (lap_number == 0U) {
		// Outlap: from start of acquisition to first crossing
		*start_sample_index = 0U;
		*end_sample_index = markers[0].sample_index - 1U;
	} else if (lap_number < crossing_count) {
		// Complete lap: between two consecutive crossings
		*start_sample_index = markers[lap_number - 1U].sample_index;
		*end_sample_index = markers[lap_number].sample_index - 1U;
	} else {
		// Inlap: from last crossing to end of acquisition
		*start_sample_index = markers[crossing_count - 1U].sample_index;
		*end_sample_index = file->total_sample_number - 1U;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_lap_offset_range(r1up_file_handle_t handle, uint32_t lap_number,
												  r1up_file_size_t* start_file_offset,
												  r1up_file_size_t* end_file_offset) {
	if (handle == NULL || start_file_offset == NULL || end_file_offset == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	const r1up_file_t* file = (const r1up_file_t*)handle;
	const uint32_t crossing_count = file->lap_table.count;

	// Need at least 1 crossing to define any segment
	if (crossing_count == 0U) {
		return R1UP_FILE_ERROR_NOT_FOUND;
	}

	if (lap_number > crossing_count) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	const r1up_lap_marker_t* markers = file->lap_table.markers;

	if (lap_number == 0U) {
		// Outlap: from body start to first crossing offset
		*start_file_offset = file->body_offset_start;
		*end_file_offset = markers[0].file_offset - 1U;
	} else if (lap_number < crossing_count) {
		// Complete lap: between two consecutive crossing offsets
		*start_file_offset = markers[lap_number - 1U].file_offset;
		*end_file_offset = markers[lap_number].file_offset - 1U;
	} else {
		// Inlap: from last crossing offset to end of body
		*start_file_offset = markers[crossing_count - 1U].file_offset;
		*end_file_offset = file->body_offset_end - 1U;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_update_vars(r1up_file_handle_t handle) {
	if (handle == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;

	r1up_file_t* file = (r1up_file_t*)handle;

	// update variables as needed
	uint32_t i;
	for (i = 0; i < file->varible_number; i++) {
		if (file->current_sample_number % file->vsf[i].fd != 0)
			continue;  // Skip updating this variable if not the right sample rate

		size_t bytes_written = 0;
		fsal_result_t fsal_res = fsal_write((const void*)file->vsd_write_buffer[i], 1U, file->vsf[i].sz,
											file->fsal_handle_ptr, &bytes_written);
		if (fsal_res != FSAL_SUCCESS || bytes_written != file->vsf[i].sz) {
			return r1up_file_convert_fsal_result_to_r1up(fsal_res);
		}
		file->vsd_bytes_written += bytes_written;
	}
	fsal_flush(file->fsal_handle_ptr);

	// ── Deferred lap marker processing ──────────────────────────────────────────
	// If r1up_file_mark_lap() raised the pending flag, record the marker now.
	// This is done AFTER writing all variables of sample N and BEFORE incrementing
	// current_sample_number, so that:
	//   marker.sample_index = current_sample_number = N (first sample of new lap)
	//   marker.file_offset  = fsal_tell() = first byte AFTER sample N
	// TODO: ensure atomic read of lap_mark_pending on target platform
	if (file->lap_mark_pending) {
		file->lap_mark_pending = false;	 // consume the flag

		// Grow the markers array by one slot
		uint32_t new_count = file->lap_table.count + 1U;
		r1up_lap_marker_t* new_markers =
			(r1up_lap_marker_t*)AG_REALLOC(file->lap_table.markers, new_count * sizeof(r1up_lap_marker_t));
		if (new_markers != NULL) {
			file->lap_table.markers = new_markers;
			file->lap_table.markers[file->lap_table.count].sample_index = file->current_sample_number;
			long tell_pos = 0;
			fsal_tell(file->fsal_handle_ptr, &tell_pos);
			file->lap_table.markers[file->lap_table.count].file_offset = (r1up_file_size_t)tell_pos;
			file->lap_table.count = new_count;
			file->lap_table.capacity = new_count;
		}
		// Note: if realloc fails, the marker is silently lost. This is acceptable
		// because lap markers are non-critical metadata — the body data is intact.
	}

	file->current_sample_number++;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_compute_sample_number(r1up_file_t* file, r1up_file_size_t vsd_blck_sz,
														  uint32_t* out_sample_number) {
	if (file == NULL || out_sample_number == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;
	if (file->vsf == NULL) return R1UP_FILE_ERROR_INVALID_STATE;

	r1up_file_size_t byte_number = 0;
	*out_sample_number = 0;

	// Simulate the exact writing process to calculate how many samples fit in sz bytes
	while (byte_number < vsd_blck_sz) {
		uint8_t i;
		for (i = 0; i < file->varible_number; i++) {
			// Only count bytes when the variable should be sampled according to its frequency divisor
			if ((*out_sample_number) % ((file->vsf + i)->fd) == 0) {
				byte_number += (long int)(file->vsf + i)->sz;

				// If we exceed the block size, we can't fit another complete sample. This is the case of a
				// truncated file and at the end there is note full block of varialbes
				if (byte_number > vsd_blck_sz) {
					return R1UP_FILE_SUCCESS;  // Return the last complete sample that fits
				}
			}
		}
		(*out_sample_number)++;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
__attribute__((unused)) static r1up_file_result_t r1up_file_compute_sample_number_for_single_variable(
	r1up_file_t* file, uint8_t var_id, r1up_file_size_t* samples) {
	if (file->vsf == NULL || samples == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;	 // Invalid variable stream format

	r1up_file_size_t total_samples = file->current_sample_number;
	uint8_t fd = (file->vsf + var_id)->fd;

	if (fd == 0) return R1UP_FILE_ERROR_INVALID_PARAM;	// Invalid frequency divisor

	// Optimized calculation: number of multiples of fd in range [0, total_samples)
	// This is equivalent to the loop but much faster. For the loop see function documentation, below the loop for
	// fast reference.
	/// \code
	/// long int samples = 0;
	/// for (long int i = 0; i < total_samples; i++) {
	///     if (i % ((vsf_pnt + var_id)->fd) == 0)
	///         samples++;
	/// }
	/// return samples;
	/// \endcode
	*samples = (total_samples + (r1up_file_size_t)(fd - 1)) / (r1up_file_size_t)fd;
	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
uint32_t r1up_file_get_variable_number(r1up_file_handle_t handle) {
	r1up_file_t* file = (r1up_file_t*)handle;
	if (file == NULL) {
		return 0U;
	}
	return file->varible_number;
}

/*==================================================================================================================*/
const char* r1up_file_get_basename(r1up_file_handle_t handle) {
	if (handle == NULL) {
		return NULL;
	}

	r1up_file_t* file = (r1up_file_t*)handle;
	const char* full_path = file->filename;
	const char* basename = full_path;

	/* Find last path separator (works for both Unix '/' and Windows '\\') */
	const char* ptr = full_path;
	while (*ptr != '\0') {
		if (*ptr == '/' || *ptr == '\\') {
			basename = ptr + 1; /* Point to character after separator */
		}
		ptr++;
	}

	return basename;
}

/*==================================================================================================================*/
const char* r1up_file_get_pathname(r1up_file_handle_t handle) {
	if (handle == NULL) {
		return NULL;
	}

	r1up_file_t* file = (r1up_file_t*)handle;
	return file->filename;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_read_body(r1up_file_handle_t handle) {
	if (handle == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	// Check if file is in read mode
	if (file->mode != R1UP_FILE_MODE_READ_ONLY) {
		return R1UP_FILE_ERROR_INVALID_STATE;
	}

	// Move file pointer to the start of the body variable data
	fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, file->body_offset_var_data_start, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	// Allocate an array of void pointers to reference each variable's data buffer
	// This approach allows generic code without hardcoding variable names
	// Dynamic allocation is used since the number of variables is determined at runtime
	file->vsd_read_buffers = AG_MALLOC(file->varible_number * sizeof(void*));
	if (file->vsd_read_buffers == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	uint8_t i;
	for (i = 0; i < file->varible_number; i++) {
		r1up_file_size_t total_samples = 0;
#if R1UP_FILE_SIGNALS_INTERPOLATION == 0
		r1up_file_result_t res = r1up_file_compute_sample_number_for_single_variable(file, i, &total_samples);
		if (res != R1UP_FILE_SUCCESS) {
			(void)r1up_file_deinit_vsd_read_buffer(file);
			return res;
		}
#elif R1UP_FILE_SIGNALS_INTERPOLATION == 1
		total_samples = file->total_sample_number;
#endif
		// Allocate buffer for each variable based on its size defined in the variables stream format
		file->vsd_read_buffers[i] = AG_MALLOC((size_t)(total_samples * (file->vsf + i)->sz));
		if (file->vsd_read_buffers[i] == NULL) {
			(void)r1up_file_deinit_vsd_read_buffer(file);
			return R1UP_FILE_ERROR_MEMORY;
		}
	}

	// Enable read buffer in file system abstraction layer
	fsal_result_t res = fsal_enable_buffering(file->fsal_handle_ptr, FSAL_BUFFER_FULL, 512U);
	if (res != FSAL_SUCCESS) {
		(void)r1up_file_deinit_vsd_read_buffer(file);
		return r1up_file_convert_fsal_result_to_r1up(res);
	}

	// Read variable data sequentially
	// Sample index: ranges from 0 to total_sample_number-1
	r1up_file_size_t sample_index = 0U;
	r1up_file_size_t total_bytes_read = 0U;

	while (sample_index < file->total_sample_number) {
		for (i = 0; i < file->varible_number; i++) {
			// Only read data when the variable should be sampled according to its frequency divisor
			if (sample_index % file->vsf[i].fd != 0) {
#if R1UP_FILE_SIGNALS_INTERPOLATION == 1
				// For interpolation mode, fill skipped samples with the last read value
				if (sample_index > 0) {
					size_t var_size = file->vsf[i].sz;
					void* dest_ptr = (uint8_t*)file->vsd_read_buffers[i] + (sample_index * var_size);
					void* src_ptr = (uint8_t*)file->vsd_read_buffers[i] + ((sample_index - 1) * var_size);
					memcpy(dest_ptr, src_ptr, var_size);
				}
#endif
				continue;
			}

			size_t bytes_read = 0;
			size_t var_size = file->vsf[i].sz;
			void* dest_ptr = ((uint8_t*)file->vsd_read_buffers[i]) + (sample_index * var_size);

			fsal_res = fsal_read(dest_ptr, 1U, var_size, file->fsal_handle_ptr, &bytes_read);
			total_bytes_read += bytes_read;

			if (fsal_res != FSAL_SUCCESS) {
				(void)r1up_file_deinit_vsd_read_buffer(file);
				return r1up_file_convert_fsal_result_to_r1up(fsal_res);
			}
		}
		sample_index++;
	}

	// Verify that the total bytes read matches the expected variable stream data size, to be sure we read
	// everything in the body of the file.
	assert(total_bytes_read == file->var_stream_data_size);

	// Disable buffering after reading
	res = fsal_enable_buffering(file->fsal_handle_ptr, FSAL_BUFFER_NONE, 512U);
	if (res != FSAL_SUCCESS) {
		(void)r1up_file_deinit_vsd_read_buffer(file);
		return r1up_file_convert_fsal_result_to_r1up(res);
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/* PRIVATE FUNCTION IMPLEMENTATIONS */
/*==================================================================================================================*/

/*==================================================================================================================*/
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
__attribute__((unused)) static void r1up_file_compile_time_checks(void) {
	// Enum type definition for string blocks IDs
	// This enum is used to ensure that the the string IDs are unique and its number is equal to
	// R1UP_FILE_BLOCKS_CNT If there is a mismatch, a compile-time error will be generated Note: This enum is
	// defined inside a function to limit its scope and avoid polluting the global namespace
#define R1UP_FILE_BLOCKS_TABLE_EXPAND_AS_ENUM_ID_UNIQUE_CHECK(id_symbol, ...) id_symbol,
	typedef enum {
		R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_BLOCKS_TABLE_EXPAND_AS_ENUM_ID_UNIQUE_CHECK) R1UP_FILE_BLOCK_ID_COUNT
	} R1UP_FILE_BLOCK_ID_t;
#undef R1UP_FILE_BLOCKS_TABLE_EXPAND_AS_ENUM_ID_UNIQUE_CHECK
	_Static_assert(R1UP_FILE_BLOCKS_CNT == R1UP_FILE_BLOCK_ID_COUNT, "R1UP_FILE_HEADER_BLOCKS_DFLT_CNT error");
}
#pragma GCC diagnostic warning "-Wunused-local-typedefs"

/*==================================================================================================================*/
static fsal_mode_t r1up_file_mode_to_fsal(r1up_file_mode_t mode) {
	switch (mode) {
		case R1UP_FILE_MODE_READ_ONLY:
			return FSAL_MODE_READ;
		case R1UP_FILE_MODE_WRITE_ONLY:
			return FSAL_MODE_WRITE;
		case R1UP_FILE_MODE_READ_WRITE:
			return FSAL_MODE_READ_WRITE;
		case R1UP_FILE_MODE_APPEND:
			return FSAL_MODE_APPEND;
		default:
			return FSAL_MODE_READ;	// Default to read mode on invalid input
	}
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_convert_fsal_result_to_r1up(fsal_result_t fsal_res) {
	switch (fsal_res) {
		case FSAL_SUCCESS:
			return R1UP_FILE_SUCCESS;
		case FSAL_ERROR_INVALID_PARAM:
			return R1UP_FILE_ERROR_INVALID_PARAM;
		case FSAL_ERROR_NOT_FOUND:
			return R1UP_FILE_ERROR_NOT_FOUND;
		case FSAL_ERROR_ACCESS_DENIED:
			return R1UP_FILE_ERROR_ACCESS;
		case FSAL_ERROR_IO:
			return R1UP_FILE_ERROR_IO;
		case FSAL_ERROR_MEMORY:
			return R1UP_FILE_ERROR_MEMORY;
		case FSAL_ERROR_CORRUPTED:
			return R1UP_FILE_ERROR_CORRUPTED;
		case FSAL_ERROR_UNSUPPORTED:
			return R1UP_FILE_ERROR_UNSUPPORTED;
		default:
			return R1UP_FILE_ERROR_IO;	// Map unknown errors to a generic I/O error
	}
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_init_blocks_list_wrapper(r1up_file_t* file) {
	r1up_file_result_t res;

	switch (file->file_type) {
		case R1UP_FILE_FILETYPE_RACE1UP:
			res = r1up_file_init_blocks_list(file);
			break;
		case R1UP_FILE_FILETYPE_RERACE:
			res = r1up_file_rr_file_init_blocks_list(file);
			break;
		default:
			res = R1UP_FILE_ERROR_UNSUPPORTED;
			break;
	}

	return res;
}
/*==================================================================================================================*/
#pragma GCC diagnostic ignored "-Wnonnull"
static r1up_file_result_t r1up_file_init_blocks_list(r1up_file_t* file) {
	// Define macro to get frequency pointer, for use in the blocks table and initialize the payload
	const uint8_t freq = R1UP_FILE_DFLT_SMPL_FRQ;
#define R1UP_FILE_BLOCKS_TBL_GET_FRQNCY() (&freq)
	// Define macro to get header length pointer, for use in the blocks table and initialize the payload
	uint32_t dummy = 0U;
#define R1UP_FILE_BLOCKS_TBL_GET_HDR_LEN() (&dummy)
#define R1UP_FILE_BLOCKS_TBL_GET_BDY_LEN() (&dummy)
#define R1UP_FILE_BLOCKS_TBL_GET_FTR_LEN() (&dummy)
	// Macro to expand and create each block element, valid for header, body and footer blocks
#define R1UP_FILE_INIT_BLOCK_ELEMENT(id_symbol, payload_len_type, payload_len, payload, editable) \
	{                                                                                             \
		r1up_file_block_node_t* new_node = AG_MALLOC(sizeof(r1up_file_block_node_t));             \
		if (new_node == NULL) {                                                                   \
			r1up_file_free_blocks_list(file);                                                     \
			return R1UP_FILE_ERROR_MEMORY;                                                        \
		}                                                                                         \
		new_node->blck.id = r1up_file_block_id_create_static(id_symbol);                          \
		new_node->blck.sz = (uint32_t)payload_len;                                                \
		if (payload_len > 0 && payload != NULL) {                                                 \
			new_node->blck.pl = AG_MALLOC(payload_len);                                           \
			if (new_node->blck.pl == NULL) {                                                      \
				AG_FREE(new_node);                                                                \
				r1up_file_free_blocks_list(file);                                                 \
				return R1UP_FILE_ERROR_MEMORY;                                                    \
			}                                                                                     \
			memcpy(new_node->blck.pl, payload, payload_len);                                      \
		} else {                                                                                  \
			new_node->blck.pl = NULL;                                                             \
		}                                                                                         \
		new_node->blck.is_writable = editable;                                                    \
		new_node->next = NULL;                                                                    \
		if (file->blocks_head == NULL) {                                                          \
			file->blocks_head = new_node;                                                         \
		} else {                                                                                  \
			r1up_file_block_node_t* current = file->blocks_head;                                  \
			while (current->next != NULL) {                                                       \
				current = current->next;                                                          \
			}                                                                                     \
			current->next = new_node;                                                             \
		}                                                                                         \
	}
	// Function implementation starts here

	// Parameter validation
	if (file == NULL || file->blocks_head != NULL) return R1UP_FILE_ERROR_INVALID_PARAM;

	switch (file->mode) {
		case R1UP_FILE_MODE_READ_ONLY:
			// Read all blocks from file (header, body, and footer)
			{
				// Move to the beginning of the file
				fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_SET);
				if (fsal_res != FSAL_SUCCESS) {
					return r1up_file_convert_fsal_result_to_r1up(fsal_res);
				}

				// Read blocks until end of file
				while (true) {
					r1up_file_block_node_t* new_node = NULL;
					char string_id_from_file[R1UP_FILE_BLCK_ID_LENGTH];
					uint32_t block_size = 0;
					void* payload = NULL;
					size_t bytes_read = 0;

					// Allocate new node for the block
					new_node = AG_MALLOC(sizeof(r1up_file_block_node_t));
					if (new_node == NULL) {
						r1up_file_free_blocks_list(file);
						return R1UP_FILE_ERROR_MEMORY;
					}

					// Read block ID (16 bytes)
					fsal_res = fsal_read(string_id_from_file, 1U, R1UP_FILE_BLCK_ID_LENGTH, file->fsal_handle_ptr,
										 &bytes_read);
					if (fsal_res != FSAL_SUCCESS || bytes_read != R1UP_FILE_BLCK_ID_LENGTH) {
						AG_FREE(new_node);
						// EOF reached normally - exit loop successfully
						if (fsal_res == FSAL_ERROR_NOT_FOUND || bytes_read == 0) {
							break;
						}
						// Real error occurred - cleanup and return error
						r1up_file_free_blocks_list(file);
						return r1up_file_convert_fsal_result_to_r1up(fsal_res);
					}

					// Read block size (4 bytes)
					fsal_res = fsal_read(&block_size, 1U, sizeof(uint32_t), file->fsal_handle_ptr, &bytes_read);
					if (fsal_res != FSAL_SUCCESS || bytes_read != sizeof(uint32_t)) {
						AG_FREE(new_node);
						r1up_file_free_blocks_list(file);
						return r1up_file_convert_fsal_result_to_r1up(fsal_res);
					}

					// Allocate and read payload if size > 0
					if (block_size > 0) {
						payload = AG_MALLOC(block_size);
						if (payload == NULL) {
							AG_FREE(new_node);
							r1up_file_free_blocks_list(file);
							return R1UP_FILE_ERROR_MEMORY;
						}

						fsal_res = fsal_read(payload, 1U, block_size, file->fsal_handle_ptr, &bytes_read);
						if (fsal_res != FSAL_SUCCESS || bytes_read != block_size) {
							AG_FREE(payload);
							AG_FREE(new_node);
							r1up_file_free_blocks_list(file);
							return r1up_file_convert_fsal_result_to_r1up(fsal_res);
						}
					}

					// Initialize block ID using factory function (auto-detects static vs dynamic)
					new_node->blck.id = r1up_file_block_id_create_from_file(string_id_from_file);
					if (r1up_file_block_id_get_string(&new_node->blck.id) == NULL) {
						if (payload != NULL) AG_FREE(payload);
						AG_FREE(new_node);
						r1up_file_free_blocks_list(file);
#if R1UP_FILE_REMOVE_BLOCK_STRING_ID_CHECK_IN_READ == 0
						return R1UP_FILE_ERROR_CORRUPTED;  // Unknown block ID in strict mode
#else
						return R1UP_FILE_ERROR_MEMORY;	// Allocation failed in permissive mode
#endif
					}

					// Initialize node fields
					new_node->blck.sz = block_size;
					new_node->blck.pl = payload;
					new_node->blck.is_writable = false;	 // Read-only blocks are not writable
					new_node->next = NULL;

					// Append node to the linked list
					if (file->blocks_head == NULL) {
						file->blocks_head = new_node;
					} else {
						r1up_file_block_node_t* current = file->blocks_head;
						while (current->next != NULL) {
							current = current->next;
						}
						current->next = new_node;
					}

					// If BODY block encountered, skip variable stream data and jump to footer
					if (memcmp(string_id_from_file, R1UP_FILE_BODY_BID, R1UP_FILE_BLCK_ID_LENGTH) == 0) {
						// Calculate position of footer: header_length + body_length
						// Header length is in first block payload, body length is in current block payload
						uint32_t header_len = *((uint32_t*)file->blocks_head->blck.pl);
						uint32_t body_len = *((uint32_t*)new_node->blck.pl);
						fsal_res = fsal_seek(file->fsal_handle_ptr, header_len + body_len, SEEK_SET);
						if (fsal_res != FSAL_SUCCESS) {
							r1up_file_free_blocks_list(file);
							return r1up_file_convert_fsal_result_to_r1up(fsal_res);
						}
					}
				}
			}
			break;
		case R1UP_FILE_MODE_WRITE_ONLY: {
			// Macro expansion to create each block element, valid for header, body and footer blocks
			R1UP_FILE_BLOCKS_TABLE(R1UP_FILE_INIT_BLOCK_ELEMENT)
		} break;
		default:
			return R1UP_FILE_ERROR_INVALID_PARAM;
	}

#undef R1UP_FILE_INIT_BLOCK_ELEMENT
#undef R1UP_FILE_BLOCKS_TBL_GET_FREQUENCY
#undef R1UP_FILE_BLOCKS_TBL_GET_HDR_LEN
#undef R1UP_FILE_BLOCKS_TBL_GET_BODY_LEN
#undef R1UP_FILE_BLOCKS_TBL_GET_FTR_LEN
	// Successful initialization
	return R1UP_FILE_SUCCESS;
}
#pragma GCC diagnostic warning "-Wnonnull"

/*==================================================================================================================*/
static bool r1up_file_rer_file_seek_to_footer_or_eof(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return false;
	}

	fsal_result_t res;
	long found_pos;
	res = fsal_search(file->fsal_handle_ptr, R1UP_FILE_RR_FOOTER_BID, R1UP_FILE_RR_FILE_BLCK_ID_LENGTH, true,
					  &found_pos, true);

	if (res == FSAL_ERROR_NOT_FOUND) {
		// Not found
		fsal_seek(file->fsal_handle_ptr, 0, SEEK_END);
		return false;
	} else if (res != FSAL_SUCCESS) {
		// Some error occurred
		return false;
	} else {
		// Found
		return true;
	}
}

/*==================================================================================================================*/
r1up_file_size_t r1up_file_get_file_size(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return 0;
	}

	// Get current position
	long current_pos = 0;
	fsal_result_t fsal_res = fsal_tell(file->fsal_handle_ptr, &current_pos);
	if (fsal_res != FSAL_SUCCESS) {
		return 0;
	}

	// Get file size
	long file_size = 0;
	fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_END);
	if (fsal_res != FSAL_SUCCESS) {
		return 0;
	}

	fsal_res = fsal_tell(file->fsal_handle_ptr, &file_size);
	if (fsal_res != FSAL_SUCCESS) {
		return 0;
	}

	// Restore original position
	fsal_seek(file->fsal_handle_ptr, current_pos, SEEK_SET);

	return (size_t)(file_size);
}

/*==================================================================================================================*/
static r1up_file_size_t r1up_file_byte_till_eof(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return 0;
	}

	// Get current position
	long current_pos = 0;
	fsal_result_t fsal_res = fsal_tell(file->fsal_handle_ptr, &current_pos);
	if (fsal_res != FSAL_SUCCESS) {
		return 0;
	}

	// Get file size
	long file_size = (long)r1up_file_get_file_size(file);

	if (file_size < current_pos) {
		return 0;
	}

	return (size_t)(file_size - current_pos);
}

/*==================================================================================================================*/
static bool r1up_file_check_eof(r1up_file_t* file) {
	if (file == NULL || file->fsal_handle_ptr == NULL) {
		return false;
	}

	// Get current position
	long current_pos = 0;
	fsal_result_t fsal_res = fsal_tell(file->fsal_handle_ptr, &current_pos);
	if (fsal_res != FSAL_SUCCESS) {
		return false;
	}

	// Get file size
	long file_size = (long)r1up_file_get_file_size(file);

	R1UP_FILE_DEBUG_PRINT("Currente pos: %ld, file size: %ld\n", current_pos, file_size);
	return (current_pos >= file_size);
}

#define RR_FILE_INIT_BLOCK_MIX_ELEMENT(id_var, id_file, id_type, id_length, payload_length_type, payload_length,       \
									   is_length_in_file, ...)                                                         \
	{                                                                                                                  \
		r1up_file_block_node_t* new_node = NULL;                                                                       \
		id_type id_buffer[id_length];                                                                                  \
		id_type id_compare_buffer[id_length] = {id_file};                                                              \
		payload_length_type rr_block_payload_length = 0;                                                               \
		void* payload = NULL;                                                                                          \
		size_t bytes_read = 0U;                                                                                        \
		/* Allocate new node for the block*/                                                                           \
		new_node = AG_MALLOC(sizeof(r1up_file_block_node_t));                                                          \
		if (new_node == NULL) {                                                                                        \
			r1up_file_free_blocks_list(file);                                                                          \
			return R1UP_FILE_ERROR_MEMORY;                                                                             \
		}                                                                                                              \
		/* Read block ID*/                                                                                             \
		long start_position = 0;                                                                                       \
		fsal_tell(file->fsal_handle_ptr, &start_position);                                                             \
		fsal_res = fsal_read(id_buffer, 1U, id_length, file->fsal_handle_ptr, &bytes_read);                            \
		if (fsal_res != FSAL_SUCCESS || bytes_read != id_length) {                                                     \
			AG_FREE(new_node);                                                                                         \
			r1up_file_free_blocks_list(file);                                                                          \
			return r1up_file_convert_fsal_result_to_r1up(fsal_res);                                                    \
		}                                                                                                              \
		/* Check read value */                                                                                         \
		if (memcmp(id_buffer, id_compare_buffer, id_length) != 0) {                                                    \
			AG_FREE(new_node);                                                                                         \
			r1up_file_free_blocks_list(file);                                                                          \
			return R1UP_FILE_ERROR_CORRUPTED;                                                                          \
		}                                                                                                              \
		/* Read payload length*/                                                                                       \
		long length_position = 0;                                                                                      \
		fsal_tell(file->fsal_handle_ptr, &length_position);                                                            \
		if (is_length_in_file) {                                                                                       \
			fsal_res = fsal_read(&rr_block_payload_length, 1U, sizeof(rr_block_payload_length), file->fsal_handle_ptr, \
								 &bytes_read);                                                                         \
			if (fsal_res != FSAL_SUCCESS || bytes_read != sizeof(rr_block_payload_length)) {                           \
				AG_FREE(new_node);                                                                                     \
				r1up_file_free_blocks_list(file);                                                                      \
				return r1up_file_convert_fsal_result_to_r1up(fsal_res);                                                \
			}                                                                                                          \
			rr_block_payload_length -= (payload_length_type)(id_length + sizeof(payload_length_type));                 \
		} else {                                                                                                       \
			rr_block_payload_length = (payload_length_type)(payload_length);                                           \
		}                                                                                                              \
		if (memcmp(id_var, R1UP_FILE_TRACK_FTR_BID, R1UP_FILE_BLCK_ID_LENGTH) == 0) {                                  \
			rr_block_payload_length += (payload_length_type)(id_length + sizeof(payload_length_type));                 \
			R1UP_FILE_DEBUG_PRINT("[DEBUG] Track id rerace: %.32s\n", id_var);                                         \
		} else if (memcmp(id_var, R1UP_FILE_SESSION_FTR_BID, R1UP_FILE_BLCK_ID_LENGTH) == 0) {                         \
			rr_block_payload_length = (payload_length_type)r1up_file_byte_till_eof(file);                              \
			R1UP_FILE_DEBUG_PRINT("[DEBUG] Laptime id rerace: %.32s\n", id_var);                                       \
		}                                                                                                              \
		/* Allocate and read payload if size > 0*/                                                                     \
		long payload_position = 0;                                                                                     \
		if (rr_block_payload_length > 0) {                                                                             \
			payload = AG_MALLOC(rr_block_payload_length);                                                              \
			if (payload == NULL) {                                                                                     \
				AG_FREE(new_node);                                                                                     \
				r1up_file_free_blocks_list(file);                                                                      \
				return R1UP_FILE_ERROR_MEMORY;                                                                         \
			}                                                                                                          \
			fsal_tell(file->fsal_handle_ptr, &payload_position);                                                       \
			fsal_res = fsal_read(payload, 1U, rr_block_payload_length, file->fsal_handle_ptr, &bytes_read);            \
			if (fsal_res != FSAL_SUCCESS || bytes_read != rr_block_payload_length) {                                   \
				AG_FREE(payload);                                                                                      \
				AG_FREE(new_node);                                                                                     \
				r1up_file_free_blocks_list(file);                                                                      \
				return r1up_file_convert_fsal_result_to_r1up(fsal_res);                                                \
			}                                                                                                          \
		}                                                                                                              \
		long end_position = 0;                                                                                         \
		fsal_tell(file->fsal_handle_ptr, &end_position);                                                               \
		if (memcmp(id_var, R1UP_FILE_TRACK_FTR_BID, R1UP_FILE_BLCK_ID_LENGTH) == 0) {                                  \
			R1UP_FILE_DEBUG_PRINT("[ID TRACK %d]\n", id_buffer[0]);                                                    \
			R1UP_FILE_DEBUG_PRINT("length: %d\n", rr_block_payload_length);                                            \
			R1UP_FILE_DEBUG_PRINT("File pointer start                      : %ld\n", start_position);                  \
			R1UP_FILE_DEBUG_PRINT("File pointer before length              : %ld\n", length_position);                 \
			R1UP_FILE_DEBUG_PRINT("File pointer after before payload       : %ld\n", payload_position);                \
			R1UP_FILE_DEBUG_PRINT("File pointer after reading block payload: %ld\n", end_position);                    \
		} else if (memcmp(id_var, R1UP_FILE_SESSION_FTR_BID, R1UP_FILE_BLCK_ID_LENGTH) == 0) {                         \
			R1UP_FILE_DEBUG_PRINT("[ID LAP %d]\n", id_buffer[0]);                                                      \
			R1UP_FILE_DEBUG_PRINT("length: %d\n", rr_block_payload_length);                                            \
			R1UP_FILE_DEBUG_PRINT("File pointer start                      : %ld\n", start_position);                  \
			R1UP_FILE_DEBUG_PRINT("File pointer before length              : %ld\n", length_position);                 \
			R1UP_FILE_DEBUG_PRINT("File pointer after before payload       : %ld\n", payload_position);                \
			R1UP_FILE_DEBUG_PRINT("File pointer after reading block payload: %ld\n", end_position);                    \
		}                                                                                                              \
		/* Initialize block ID using factory function (auto-detects static vs dynamic)*/                               \
		new_node->blck.id = r1up_file_block_id_create_from_file(id_var);                                               \
		if (r1up_file_block_id_get_string(&new_node->blck.id) == NULL) {                                               \
			if (payload != NULL) AG_FREE(payload);                                                                     \
			AG_FREE(new_node);                                                                                         \
			r1up_file_free_blocks_list(file);                                                                          \
			return R1UP_FILE_ERROR_MEMORY;                                                                             \
		} /* Initialize node fields */                                                                                 \
		new_node->blck.sz = rr_block_payload_length;                                                                   \
		new_node->blck.pl = payload;                                                                                   \
		new_node->blck.is_writable = false;                                                                            \
		new_node->next = NULL;                                                                                         \
		/* Append node to the linked list */                                                                           \
		if (file->blocks_head == NULL) {                                                                               \
			file->blocks_head = new_node;                                                                              \
		} else {                                                                                                       \
			r1up_file_block_node_t* current = file->blocks_head;                                                       \
			while (current->next != NULL) {                                                                            \
				current = current->next;                                                                               \
			}                                                                                                          \
			current->next = new_node;                                                                                  \
		}                                                                                                              \
		if (memcmp(id_var, R1UP_FILE_BODY_BID, R1UP_FILE_BLCK_ID_LENGTH) == 0) {                                       \
			/* seek file pointer to the file footer */                                                                 \
			if (!r1up_file_rer_file_seek_to_footer_or_eof(file)) {                                                     \
				/* This is the case of a corrupt file without footer */                                                \
				break;                                                                                                 \
			}                                                                                                          \
		}                                                                                                              \
		/* Check if we reached end of file */                                                                          \
		if (r1up_file_check_eof(file)) {                                                                               \
			break;                                                                                                     \
		}                                                                                                              \
	}

static r1up_file_size_t r1up_file_get_type_size(r1up_type_id_t type) {
#define TYPE_ID_AS_SIZE(name, value, token_type, ...) \
	case name:                                        \
		return sizeof(R1UP_FILE_TYPE(token_type));
	switch (type) {
		R1UP_FILE_TYPE_ID_TABLE(TYPE_ID_AS_SIZE)
		default:
			return 0;
	}
}
static r1up_file_result_t r1up_file_convert_rr_vsf_payload_to_r1up(r1up_file_t* file, void* rr_vsf_payload,
																   r1up_file_size_t rr_vsf_payload_size,
																   void** out_converted_payload,
																   r1up_file_size_t* out_converted_payload_size) {
	if (file == NULL || rr_vsf_payload == NULL || out_converted_payload == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// Pointer to legacy vsf structure in RER file
	struct {
		uint8_t id;
		uint8_t type;
		uint8_t fd;
	}* rer_file_vsf = NULL;

	rer_file_vsf = (typeof(rer_file_vsf))rr_vsf_payload;
	uint32_t varible_number = rr_vsf_payload_size / sizeof(*rer_file_vsf);

	// Allocate memory for converted vsf array
	r1up_file_vsf_t* vsf = AG_MALLOC(varible_number * sizeof(r1up_file_vsf_t));
	if (vsf == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

#define R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE_EXPAND_AS_RR_TO_R1UP_CONVERSION(            \
	rr_var_id, rr_var_id_value, rr_type_id, rr_freq_divisior, r1up_var_id, r1up_type, ...) \
	case rr_var_id:                                                                        \
		vsf[i].id = r1up_var_id;                                                           \
		vsf[i].type = r1up_type;                                                           \
		vsf[i].fd = rr_freq_divisior;                                                      \
		vsf[i].sz = r1up_file_get_type_size(r1up_type);                                    \
		break;
	// Covert legacy rerace vsf to r1up vsf
	uint32_t i;
	for (i = 0; i < varible_number; i++) {
		switch (rer_file_vsf[i].id) {
			R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE(
				R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE_EXPAND_AS_RR_TO_R1UP_CONVERSION)
			default:
				AG_FREE(vsf);
				return R1UP_FILE_ERROR_CORRUPTED;
		}
	}
	*out_converted_payload = vsf;
	*out_converted_payload_size = varible_number * sizeof(r1up_file_vsf_t);
	return R1UP_FILE_SUCCESS;
}
/*==================================================================================================================*/
static r1up_file_result_t r1up_file_rr_file_split_data_format(r1up_file_t* file) {
	// Parameter validation
	if (file == NULL || file->blocks_head == NULL || file->mode != R1UP_FILE_MODE_READ_ONLY)
		return R1UP_FILE_ERROR_INVALID_PARAM;

	// get data legacy data format blocks from rerace file
	r1up_file_block_node_t* legacy_data_format_block = r1up_file_get_block_node(file, R1UP_FILE_VARS_STRM_FRMT_BID);
	if (legacy_data_format_block == NULL || legacy_data_format_block->blck.pl == NULL) {
		return R1UP_FILE_ERROR_CORRUPTED;
	}

	// Save the original payload pointer and size before modification
	void* original_payload = legacy_data_format_block->blck.pl;
	r1up_file_size_t original_size = legacy_data_format_block->blck.sz;

	uint8_t sample_rate = *((uint8_t*)original_payload);

	// Allocate new node for the sample rate block
	r1up_file_block_node_t* new_node = AG_MALLOC(sizeof(r1up_file_block_node_t));
	if (new_node == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	// Initialize block ID using factory function (auto-detects static vs dynamic)
	new_node->blck.id = r1up_file_block_id_create_from_file(R1UP_FILE_DT_SAMPLE_RATE_BID);
	if (r1up_file_block_id_get_string(&new_node->blck.id) == NULL) {
		AG_FREE(new_node);
		return R1UP_FILE_ERROR_MEMORY;
	}

	// Allocate separate memory for the sample rate and copy the byte
	new_node->blck.sz = sizeof(uint8_t);
	new_node->blck.pl = AG_MALLOC(new_node->blck.sz);
	if (new_node->blck.pl == NULL) {
		r1up_file_block_id_free(&new_node->blck.id);
		AG_FREE(new_node);
		return R1UP_FILE_ERROR_MEMORY;
	}
	// Copy the sample rate byte from legacy block
	*((uint8_t*)new_node->blck.pl) = sample_rate;
	new_node->blck.is_writable = false;
	new_node->next = legacy_data_format_block;

	// Now allocate new payload for the legacy block (without the first byte)
	r1up_file_size_t new_legacy_size = 0U;
	void* new_legacy_payload = NULL;
	r1up_file_result_t res = r1up_file_convert_rr_vsf_payload_to_r1up(
		file, ((uint8_t*)original_payload) + 1, original_size - sizeof(uint8_t), &new_legacy_payload, &new_legacy_size);

	if (res != R1UP_FILE_SUCCESS) {
		// Cleanup allocated sample rate block
		r1up_file_block_id_free(&new_node->blck.id);
		AG_FREE(new_node->blck.pl);
		AG_FREE(new_node);
		return res;
	}
	// Free the original payload and update legacy block
	AG_FREE(original_payload);
	legacy_data_format_block->blck.pl = new_legacy_payload;
	legacy_data_format_block->blck.sz = new_legacy_size;

	// Insert new_node before legacy_data_format_block
	if (file->blocks_head == legacy_data_format_block) {
		// legacy_data_format_block is the first node
		file->blocks_head = new_node;
	} else {
		// Find the node that points to legacy_data_format_block
		r1up_file_block_node_t* prev = file->blocks_head;
		while (prev != NULL && prev->next != legacy_data_format_block) {
			prev = prev->next;
		}
		if (prev != NULL) {
			prev->next = new_node;
		}
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_rr_file_init_blocks_list(r1up_file_t* file) {
	// Parameter validation
	if (file == NULL || file->blocks_head != NULL || file->mode != R1UP_FILE_MODE_READ_ONLY)
		return R1UP_FILE_ERROR_INVALID_PARAM;

	// Move to the beginning of the file
	fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, 0, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	// Read all blocks from file (header, body, and footer)
	while (true) {
		R1UP_FILE_RR_FILE_BLOCKS_TABLE(RR_FILE_INIT_BLOCK_MIX_ELEMENT)
	}

	if (r1up_file_rr_file_split_data_format(file) != R1UP_FILE_SUCCESS) {
		r1up_file_free_blocks_list(file);
		return R1UP_FILE_ERROR_CORRUPTED;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/**
 * @brief Linear-scan lookup: variable ID → position in VSF array.
 * @param file  Opened file handle (must have vsf != NULL).
 * @param var_id  Semantic variable ID (e.g. 0x01000000 for RPM).
 * @return Position index (0 … var_count-1), or -1 if not found.
 * @note  O(N) with N ≤ R1UP_FILE_VARS_STREAM_NUM (currently 28).
 */
static int32_t r1up_file_find_var_position(const r1up_file_t* file, r1up_variable_id_t var_id) {
	uint32_t i;
	for (i = 0; i < file->varible_number; i++) {
		if (file->vsf[i].id == var_id) {
			return (int32_t)i;
		}
	}
	return -1;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_init_variables_stream_format(r1up_file_t* file) {
	// Parameter validation
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	switch (file->mode) {
		case R1UP_FILE_MODE_READ_ONLY: {
			// Curly braces are required here because in C89/C90, a label (case or regular label)
			// must be followed by a statement, not a declaration. Variable declarations are not
			// considered statements. The braces create a new compound statement that allows
			// variable declarations immediately after the case label.
			r1up_file_block_t* vsf_blck = r1up_file_get_block(file, R1UP_FILE_VARS_STRM_FRMT_BID);
			if (vsf_blck == NULL) {
				return R1UP_FILE_ERROR_CORRUPTED;
			}
			file->vsf = (r1up_file_vsf_t*)vsf_blck->pl;
			assert(vsf_blck->pl != NULL);								// never happen payload just be checked
			assert(((vsf_blck->sz) % (sizeof(r1up_file_vsf_t))) == 0);	// check size is multiple of vsf structure size
			file->varible_number = vsf_blck->sz / sizeof(r1up_file_vsf_t);
			// Note: Removed assert for exact variable count to allow flexible variable sets
		} break;
		case R1UP_FILE_MODE_WRITE_ONLY: {
			// Initialize variable stream format structure
			file->vsf = AG_MALLOC(file->varible_number * sizeof(r1up_file_vsf_t));
			if (file->vsf == NULL) {
				return R1UP_FILE_ERROR_MEMORY;
			}

			// Initialize each VSF entry using sequential indexing
			uint32_t vsf_index = 0;
#define R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_VARS_STREAM_FORMAT_INIT(var_id, inner_type, data_type, freq_div, \
																		  ...)                                     \
	file->vsf[vsf_index].id = var_id;                                                                              \
	file->vsf[vsf_index].type = inner_type;                                                                        \
	file->vsf[vsf_index].sz = sizeof(ag_fixedpoint_##data_type##_t);                                               \
	file->vsf[vsf_index].fd = freq_div;                                                                            \
	vsf_index++;

			R1UP_FILE_VARS_STREAM_FORMAT_TABLE(R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_VARS_STREAM_FORMAT_INIT)
#undef R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_VARS_STREAM_FORMAT_INIT

			r1up_file_block_t* vsf_blck = r1up_file_get_block(file, R1UP_FILE_VARS_STRM_FRMT_BID);
			if (vsf_blck == NULL) {
				return R1UP_FILE_ERROR_CORRUPTED;
			}
			vsf_blck->pl = (void*)file->vsf;
			vsf_blck->sz = (uint32_t)(sizeof(r1up_file_vsf_t) * file->varible_number);
		} break;
		default:
			assert(false);	// This should never happen, because mode is validated earlier
			break;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_deinit_variables_stream_format(r1up_file_t* file) {
	// Parameter validation
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// Handle VSF memory differently based on file format:
	// - R1UP format: vsf points to block payload (freed by r1up_file_free_blocks_list) → just NULL it
	// - ReRace legacy: vsf allocated separately in read mode → must be freed here
	if (file->file_type == R1UP_FILE_FILETYPE_RERACE && file->mode == R1UP_FILE_MODE_READ_ONLY && file->vsf != NULL) {
		AG_FREE(file->vsf);
	}

	file->vsf = NULL;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_init_vsd_write_buffer(r1up_file_t* file) {
	// Parameter validation
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	// Build the array of pointers for variable data buffers
	file->vsd_write_buffer = AG_MALLOC(file->varible_number * sizeof(void*));
	if (file->vsd_write_buffer == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	// Initialize all buffer entries to zero
	memset(file->vsd_write_buffer, 0, file->varible_number * sizeof(void*));

	// allocate the write buffer
	*(file->vsd_write_buffer) = AG_MALLOC(r1up_file_cmpt_max_vsd_block_length(file, file->varible_number));
	if (*(file->vsd_write_buffer) == NULL) {
		AG_FREE(file->vsd_write_buffer);
		return R1UP_FILE_ERROR_MEMORY;
	}

	// make the other pointers point to the correct offsets in the allocated buffer
	r1up_file_size_t offset = 0;
	uint32_t i;
	for (i = 0; i < file->varible_number; i++) {
		file->vsd_write_buffer[i] = (void*)((uint8_t*)(*(file->vsd_write_buffer)) + offset);
		offset += file->vsf[i].sz;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_deinit_vsd_write_buffer(r1up_file_t* file) {
	// Parameter validation
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	bool error = false;

	// Free the allocated write buffer
	if (file->vsd_write_buffer != NULL) {
		if (*(file->vsd_write_buffer) != NULL) {
			if (AG_FREE(*(file->vsd_write_buffer)) != AG_MAL_OK) {
				error = true;
			}
		}
		if (AG_FREE(file->vsd_write_buffer) != AG_MAL_OK) {
			error = true;
		}
		file->vsd_write_buffer = NULL;
	}

	return error ? R1UP_FILE_ERROR_MEMORY : R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_deinit_vsd_read_buffer(r1up_file_t* file) {
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	bool error = false;

	// Free the allocated read buffers
	if (file->vsd_read_buffers != NULL) {
		uint8_t i;
		for (i = 0; i < file->varible_number; i++) {
			if (file->vsd_read_buffers[i] != NULL) {
				if (AG_FREE(file->vsd_read_buffers[i]) != AG_MAL_OK) {
					error = true;
				}
			}
		}
		if (AG_FREE(file->vsd_read_buffers) != AG_MAL_OK) {
			error = true;
		}
		file->vsd_read_buffers = NULL;
	}

	return error ? R1UP_FILE_ERROR_MEMORY : R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_free_blocks_list(r1up_file_t* file) {
	if (file == NULL) return R1UP_FILE_ERROR_INVALID_PARAM;
	bool error = false;

	r1up_file_block_node_t* current = file->blocks_head;
	while (current != NULL) {
		r1up_file_block_node_t* next = current->next;

		// Free block ID using safe factory function
		if (r1up_file_block_id_free(&current->blck.id) != R1UP_FILE_SUCCESS) {
			error = true;
		}

		// Free payload and node itself
		if (current->blck.pl != NULL && AG_FREE(current->blck.pl) != AG_MAL_OK) {
			error = true;
		}
		if (AG_FREE(current) != AG_MAL_OK) {
			error = true;
		}
		current = next;
	}
	file->blocks_head = NULL;

	return error ? R1UP_FILE_ERROR_MEMORY : R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/**
 * @brief Compare block ID in node against a static string constant
 * @details Helper function for searching blocks by ID. Optimized for comparing node IDs
 * against static string constants from the macro table.
 *
 * @param node Pointer to the block node containing the ID to compare
 * @param static_id_string Pointer to a static ID string (typically from macro table)
 * @return true if the IDs match, false otherwise
 */
static inline bool r1up_file_block_id_match(const r1up_file_block_node_t* node, const char* static_id_string) {
	if (node == NULL || static_id_string == NULL) return false;

	// Create a static block ID for comparison
	r1up_file_block_id_t search_id = r1up_file_block_id_create_static(static_id_string);

	return r1up_file_block_id_equals(&node->blck.id, &search_id);
}

/*==================================================================================================================*/
static r1up_file_block_node_t* r1up_file_get_block_node(r1up_file_t* file, const char* id_string) {
	if (file == NULL || id_string == NULL) return NULL;

	r1up_file_block_node_t* current = file->blocks_head;
	while (current != NULL) {
		if (r1up_file_block_id_match(current, id_string)) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}
/*==================================================================================================================*/
static r1up_file_result_t r1up_file_cmp_blocks_list_size(r1up_file_t* file, const char* id_string,
														 r1up_file_size_t* total_size) {
	if (file == NULL || id_string == NULL || total_size == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	if (file->blocks_head == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	*total_size = 0;
	r1up_file_block_id_t body_id = r1up_file_block_id_create_static(R1UP_FILE_BODY_BID);
	r1up_file_block_id_t footer_id = r1up_file_block_id_create_static(R1UP_FILE_FOOTER_BID);

	if (id_string == R1UP_FILE_HEADER_BID) {
		// calculate size of header blocks
		r1up_file_block_node_t* current = file->blocks_head;
		while (current != NULL && !r1up_file_block_id_equals(&current->blck.id, &body_id)) {
			*total_size += R1UP_FILE_BLCK_ID_LENGTH;  // Add block ID size
			*total_size += sizeof(uint32_t);		  // Add block size field
			*total_size += current->blck.sz;		  // Add payload size

			current = current->next;
		}
	} else if (id_string == R1UP_FILE_BODY_BID) {
		// calculate size of body blocks
		r1up_file_block_node_t* current = r1up_file_get_block_node(file, R1UP_FILE_BODY_BID);
		if (current == NULL) {
			return R1UP_FILE_ERROR_NOT_FOUND;
		}

		while (current != NULL && !r1up_file_block_id_equals(&current->blck.id, &footer_id)) {
			*total_size += R1UP_FILE_BLCK_ID_LENGTH;  // Add block ID size
			*total_size += sizeof(uint32_t);		  // Add block size field
			*total_size += current->blck.sz;		  // Add payload size

			current = current->next;
		}
	} else if (id_string == R1UP_FILE_FOOTER_BID) {
		// calculate size of footer blocks
		r1up_file_block_node_t* current = r1up_file_get_block_node(file, R1UP_FILE_FOOTER_BID);
		if (current == NULL) {
			return R1UP_FILE_ERROR_NOT_FOUND;
		}
		while (current != NULL) {
			*total_size += R1UP_FILE_BLCK_ID_LENGTH;  // Add block ID size
			*total_size += sizeof(uint32_t);		  // Add block size field
			*total_size += current->blck.sz;		  // Add payload size

			current = current->next;
		}
	} else {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static r1up_file_size_t r1up_file_cmpt_max_vsd_block_length(r1up_file_t* file, uint16_t var_number) {
	r1up_file_size_t l = 0;

	uint32_t i;
	for (i = 0; i < var_number; i++) l += file->vsf[i].sz;

	return l;
}

/*==================================================================================================================*/
static r1up_file_block_t* r1up_file_get_block(r1up_file_t* file, const char* id_string) {
	r1up_file_block_node_t* node = r1up_file_get_block_node(file, id_string);
	return node ? &node->blck : NULL;
}

/*==================================================================================================================*/
static r1up_file_block_id_t* r1up_file_get_block_id(r1up_file_t* file, const char* id_string) {
	r1up_file_block_node_t* node = r1up_file_get_block_node(file, id_string);
	return node ? &node->blck.id : NULL;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_write_blocks_to_file(r1up_file_t* file, const char* id_string,
														 r1up_file_size_t* bytes_written_out) {
	if (file == NULL || file->fsal_handle_ptr == NULL || id_string == NULL || bytes_written_out == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_block_node_t* start_node;
	const char* stop_id_string = NULL;

	if (id_string == R1UP_FILE_HEADER_BID) {
		start_node = file->blocks_head;
		stop_id_string = R1UP_FILE_BODY_BID;
	} else if (id_string == R1UP_FILE_BODY_BID) {
		start_node = r1up_file_get_block_node(file, R1UP_FILE_BODY_BID);
		if (start_node == NULL) {
			return R1UP_FILE_ERROR_NOT_FOUND;
		}
		stop_id_string = R1UP_FILE_FOOTER_BID;
	} else if (id_string == R1UP_FILE_FOOTER_BID) {
		start_node = r1up_file_get_block_node(file, R1UP_FILE_FOOTER_BID);
		if (start_node == NULL) {
			return R1UP_FILE_ERROR_NOT_FOUND;
		}
		// id string stay null, the loop will write until the end of the list
	} else {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}  // else not valid id_string, this works only for header, body and footer blocks

	*bytes_written_out = 0U;
	r1up_file_block_id_t stop_block_id = {0};
	if (stop_id_string != NULL) {
		stop_block_id = r1up_file_block_id_create_static(stop_id_string);
	}

	r1up_file_block_node_t* current = start_node;
	while (current != NULL &&
		   (stop_id_string == NULL || !r1up_file_block_id_equals(&current->blck.id, &stop_block_id))) {
		size_t bytes_written = 0;
		// Write block ID
		const char* id_str = r1up_file_block_id_get_string(&current->blck.id);
		fsal_result_t fsal_res =
			fsal_write(id_str, 1U, R1UP_FILE_BLCK_ID_LENGTH, file->fsal_handle_ptr, &bytes_written);
		if (fsal_res != FSAL_SUCCESS || bytes_written != R1UP_FILE_BLCK_ID_LENGTH) {
			return r1up_file_convert_fsal_result_to_r1up(fsal_res);
		}
		*bytes_written_out += bytes_written;

		// Write block size
		bytes_written = 0;
		fsal_res = fsal_write(&(current->blck.sz), 1U, sizeof(uint32_t), file->fsal_handle_ptr, &bytes_written);
		if (fsal_res != FSAL_SUCCESS || bytes_written != sizeof(uint32_t)) {
			return r1up_file_convert_fsal_result_to_r1up(fsal_res);
		}
		*bytes_written_out += bytes_written;

		// Write block payload if it exists
		bytes_written = 0;
		if (current->blck.sz > 0 && current->blck.pl != NULL) {
			fsal_res = fsal_write(current->blck.pl, 1U, current->blck.sz, file->fsal_handle_ptr, &bytes_written);
			if (fsal_res != FSAL_SUCCESS || bytes_written != current->blck.sz) {
				return r1up_file_convert_fsal_result_to_r1up(fsal_res);
			}
			*bytes_written_out += bytes_written;
		}

		current = current->next;
	}
	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
static bool r1up_file_is_header_block(r1up_file_t* file, const char* id_string) {
	if (file == NULL || id_string == NULL) {
		return false;
	}

	// Get the actual block_id from the BODY block in the list
	r1up_file_block_id_t* body_id = r1up_file_get_block_id(file, R1UP_FILE_BODY_BID);
	if (body_id == NULL) {
		return false;  // BODY block not found - should not happen in a properly initialized file
	}

	r1up_file_block_id_t search_id = r1up_file_block_id_create_static(id_string);

	r1up_file_block_node_t* current = file->blocks_head;
	while (current != NULL && !r1up_file_block_id_equals(&current->blck.id, body_id)) {
		if (r1up_file_block_id_equals(&current->blck.id, &search_id)) {
			return true;
		}
		current = current->next;
	}

	return false;
}

/*==================================================================================================================*/
static bool r1up_file_is_body_block(r1up_file_t* file, const char* id_string) {
	if (file == NULL || id_string == NULL) {
		return false;
	}

	return id_string == R1UP_FILE_BODY_BID;
}

/*==================================================================================================================*/
static bool r1up_file_is_footer_block(r1up_file_t* file, const char* id_string) {
	if (file == NULL || id_string == NULL) {
		return false;
	}

	r1up_file_block_id_t* search_id = r1up_file_get_block_id(file, id_string);
	if (search_id == NULL) {
		return false;
	}

	r1up_file_block_node_t* current = r1up_file_get_block_node(file, R1UP_FILE_FOOTER_BID);
	while (current != NULL) {
		if (&current->blck.id == search_id) {
			return true;
		}
		current = current->next;
	}

	return false;
}

/*==================================================================================================================*/
static bool r1up_file_string_exists_in_blocks_list(r1up_file_t* file, const char* id_string) {
	if (file == NULL || id_string == NULL) {
		return false;
	}

	r1up_file_block_id_t search_id = r1up_file_block_id_create_static(id_string);
	r1up_file_block_node_t* current = file->blocks_head;
	while (current != NULL) {
		if (r1up_file_block_id_equals(&current->blck.id, &search_id)) {
			return true;
		}
		current = current->next;
	}
	return false;
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_deinit_all(r1up_file_t* file) {
	if (file == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}
	bool error = false;

	// Free blocks list
	if (r1up_file_free_blocks_list(file) != R1UP_FILE_SUCCESS) {
		error = true;
	}

	// Free variables stream format
	if (r1up_file_deinit_variables_stream_format(file) != R1UP_FILE_SUCCESS) {
		error = true;
	}

	// Free variables stream data write buffer
	if (r1up_file_deinit_vsd_write_buffer(file) != R1UP_FILE_SUCCESS) {
		error = true;
	}

	// Free variables stream data read buffers
	if (r1up_file_deinit_vsd_read_buffer(file) != R1UP_FILE_SUCCESS) {
		error = true;
	}

	// Free body info cache arrays
	if (r1up_file_deinit_body_info(file) != R1UP_FILE_SUCCESS) {
		error = true;
	}

	// Free lap table markers array
	if (file->lap_table.markers != NULL) {
		if (AG_FREE(file->lap_table.markers) != AG_MAL_OK) {
			error = true;
		}
		file->lap_table.markers = NULL;
		file->lap_table.count = 0U;
		file->lap_table.capacity = 0U;
	}

	// Free FSAL handle
	if (file->fsal_handle_ptr != NULL) {
		if (fsal_close(file->fsal_handle_ptr) != FSAL_SUCCESS) {
			error = true;
		}
		if (AG_FREE(file->fsal_handle_ptr) != AG_MAL_OK) {
			error = true;
		}
		file->fsal_handle_ptr = NULL;
	}

	// Free the main file structure
	if (AG_FREE(file) != AG_MAL_OK) {
		error = true;
	}

	// Only check for leaks when we are the last ag_mal user.
	// init_count == 2 means: 1 from r1up_file_open + 1 from fsal_init;
	// when > 2, other tasks still hold valid allocations.
	if (ag_mal_get_init_count() <= 2U) {
		if (ag_mal_check_leaks() > 0) {
			R1UP_FILE_DEBUG_PRINT("Memory leak detected!\n");
			ag_mal_print_report();
		}
		assert(ag_mal_check_leaks() == 0);
	}

	// Deinitialize — reverse order of init in r1up_file_open():
	//   r1up_file_open: ag_mal_init() → fsal_init() (which calls ag_mal_init internally)
	//   r1up_file_close: ag_mal_deinit() → fsal_deinit() (which calls ag_mal_deinit internally)
	if (ag_mal_deinit() != AG_MAL_OK) {
		error = true;
	}

	if (fsal_deinit() != FSAL_SUCCESS) {
		error = true;
	}

	return error ? R1UP_FILE_ERROR_MEMORY : R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/* GPS CONVERSION HELPER FUNCTIONS                                          */
/*==================================================================================================================*/

ag_fixedpoint_float_t r1up_file_convert_gps_latitude_from_raw(uint8_t deg, int8_t min, uint16_t deci) {
	/* Calculate decimal part: (abs(min) + deci/10000) / 60 */
	ag_fixedpoint_float_t decimal_part =
		(fabs((ag_fixedpoint_float_t)min) + (ag_fixedpoint_float_t)deci / 10000.0) / 60.0;

	/* Apply workaround for R1UP format bug:
	 * If min < 0, the coordinate is negative but deg is still positive
	 * Formula from MATLAB script:
	 * if lat_min(i) < 0:
	 *     lat(i) = -(lat_deg(i) + ((abs(lat_min(i)) + lat_dec(i)/10000)/60))
	 * else:
	 *     lat(i) = lat_deg(i) + ((abs(lat_min(i)) + lat_dec(i)/10000)/60)
	 */
	if (min < 0) {
		return -((ag_fixedpoint_float_t)deg + decimal_part);
	} else {
		return (ag_fixedpoint_float_t)deg + decimal_part;
	}
}

ag_fixedpoint_float_t r1up_file_convert_gps_longitude_from_raw(uint8_t deg, int8_t min, uint16_t deci) {
	/* Calculate decimal part: (abs(min) + deci/10000) / 60 */
	ag_fixedpoint_float_t decimal_part =
		(fabs((ag_fixedpoint_float_t)min) + (ag_fixedpoint_float_t)deci / 10000.0) / 60.0;

	/* Apply workaround for R1UP format bug:
	 * If min < 0, the coordinate is negative but deg is still positive
	 * Formula from MATLAB script:
	 * if lon_min(i) < 0:
	 *     lon(i) = -(lon_deg(i) + ((abs(lon_min(i)) + lon_dec(i)/10000)/60))
	 * else:
	 *     lon(i) = lon_deg(i) + ((abs(lon_min(i)) + lon_dec(i)/10000)/60)
	 */
	if (min < 0) {
		return -((ag_fixedpoint_float_t)deg + decimal_part);
	} else {
		return (ag_fixedpoint_float_t)deg + decimal_part;
	}
}

/*==================================================================================================================*/
ag_fixedpoint_float_t r1up_file_convert_gps_latitude(ag_fixedpoint_float_t deg, ag_fixedpoint_float_t min,
													 ag_fixedpoint_float_t deci) {
	/* Calculate decimal part: (abs(min) + deci/10000) / 60 */
	ag_fixedpoint_float_t decimal_part = (fabs(min) + deci / 10000.0) / 60.0;

	/* Apply workaround for R1UP format bug */
	if (min < 0.0) {
		return -(deg + decimal_part);
	} else {
		return deg + decimal_part;
	}
}

/*==================================================================================================================*/
ag_fixedpoint_float_t r1up_file_convert_gps_longitude(ag_fixedpoint_float_t deg, ag_fixedpoint_float_t min,
													  ag_fixedpoint_float_t deci) {
	/* Calculate decimal part: (abs(min) + deci/10000) / 60 */
	ag_fixedpoint_float_t decimal_part = (fabs(min) + deci / 10000.0) / 60.0;

	/* Apply workaround for R1UP format bug */
	if (min < 0.0) {
		return -(deg + decimal_part);
	} else {
		return deg + decimal_part;
	}
}

/*==================================================================================================================*/
/* GPS COORDINATE RETRIEVAL - INTERNAL RANGE-BASED CORE FUNCTIONS */
/*==================================================================================================================*/

/**
 * @brief Internal: Extract latitude samples in a given index range and convert to decimal degrees.
 * @details Core function that all latitude getters delegate to. Performs the actual conversion
 *          from raw fixed-point deg/min/deci components to decimal degrees.
 *
 * @param file            Internal file structure (already validated by caller)
 * @param start_sample    First sample index (inclusive)
 * @param end_sample      Last sample index (inclusive)
 * @param[out] out_lat_dd Caller-allocated output buffer
 * @param buffer_size     Number of elements in out_lat_dd
 * @param[out] out_sample_count Optional: receives actual sample count written
 * @return R1UP_FILE_SUCCESS or error code
 *
 * @note Caller is responsible for parameter validation and body-read checks.
 */
static r1up_file_result_t r1up_file_get_gps_latitude_dd_range_internal(r1up_file_t* file, r1up_file_size_t start_sample,
																	   r1up_file_size_t end_sample,
																	   ag_fixedpoint_float_t* out_lat_dd,
																	   r1up_file_size_t buffer_size,
																	   r1up_file_size_t* out_sample_count) {
	/* Compute sample count */
	const r1up_file_size_t sample_count = (r1up_file_size_t)(end_sample - start_sample + 1U);

	/* Verify buffer is large enough */
	if (buffer_size < sample_count) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	/* Find positions for all three GPS latitude components */
	int32_t pos_deg = r1up_file_find_var_position(file, R1UP_FILE_VAR_ID_LAT_DEG);
	int32_t pos_min = r1up_file_find_var_position(file, R1UP_FILE_VAR_ID_LAT_MIN);
	int32_t pos_deci = r1up_file_find_var_position(file, R1UP_FILE_VAR_ID_LAT_DECI);

	if (pos_deg < 0 || pos_min < 0 || pos_deci < 0) {
		return R1UP_FILE_ERROR_NOT_FOUND; /* One or more GPS components not present */
	}

	/* Convert each sample in the range */
	r1up_file_size_t i;
	for (i = 0U; i < sample_count; i++) {
		ag_fixedpoint_float_t deg_f, min_f, deci_f;
		const r1up_file_size_t sample_idx = start_sample + i;

		r1up_file_convert_raw_sample_to_float(
			file, R1UP_FILE_VAR_ID_LAT_DEG,
			(const uint8_t*)file->vsd_read_buffers[pos_deg] + sample_idx * file->vsf[pos_deg].sz, &deg_f);
		r1up_file_convert_raw_sample_to_float(
			file, R1UP_FILE_VAR_ID_LAT_MIN,
			(const uint8_t*)file->vsd_read_buffers[pos_min] + sample_idx * file->vsf[pos_min].sz, &min_f);
		r1up_file_convert_raw_sample_to_float(
			file, R1UP_FILE_VAR_ID_LAT_DECI,
			(const uint8_t*)file->vsd_read_buffers[pos_deci] + sample_idx * file->vsf[pos_deci].sz, &deci_f);

		out_lat_dd[i] = r1up_file_convert_gps_latitude(deg_f, min_f, deci_f);
	}

	if (out_sample_count != NULL) {
		*out_sample_count = sample_count;
	}

	return R1UP_FILE_SUCCESS;
}

/**
 * @brief Internal: Extract longitude samples in a given index range and convert to decimal degrees.
 * @details Core function that all longitude getters delegate to. Performs the actual conversion
 *          from raw fixed-point deg/min/deci components to decimal degrees.
 *
 * @param file            Internal file structure (already validated by caller)
 * @param start_sample    First sample index (inclusive)
 * @param end_sample      Last sample index (inclusive)
 * @param[out] out_lon_dd Caller-allocated output buffer
 * @param buffer_size     Number of elements in out_lon_dd
 * @param[out] out_sample_count Optional: receives actual sample count written
 * @return R1UP_FILE_SUCCESS or error code
 *
 * @note Caller is responsible for parameter validation and body-read checks.
 */
static r1up_file_result_t r1up_file_get_gps_longitude_dd_range_internal(
	r1up_file_t* file, r1up_file_size_t start_sample, r1up_file_size_t end_sample, ag_fixedpoint_float_t* out_lon_dd,
	r1up_file_size_t buffer_size, r1up_file_size_t* out_sample_count) {
	/* Compute sample count */
	const r1up_file_size_t sample_count = (r1up_file_size_t)(end_sample - start_sample + 1U);

	/* Verify buffer is large enough */
	if (buffer_size < sample_count) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	/* Find positions for all three GPS longitude components */
	int32_t pos_deg = r1up_file_find_var_position(file, R1UP_FILE_VAR_ID_LONG_DEG);
	int32_t pos_min = r1up_file_find_var_position(file, R1UP_FILE_VAR_ID_LONG_MIN);
	int32_t pos_deci = r1up_file_find_var_position(file, R1UP_FILE_VAR_ID_LONG_DECI);

	if (pos_deg < 0 || pos_min < 0 || pos_deci < 0) {
		return R1UP_FILE_ERROR_NOT_FOUND; /* One or more GPS components not present */
	}

	/* Convert each sample in the range */
	r1up_file_size_t i;
	for (i = 0U; i < sample_count; i++) {
		ag_fixedpoint_float_t deg_f, min_f, deci_f;
		const r1up_file_size_t sample_idx = start_sample + i;

		r1up_file_convert_raw_sample_to_float(
			file, R1UP_FILE_VAR_ID_LONG_DEG,
			(const uint8_t*)file->vsd_read_buffers[pos_deg] + sample_idx * file->vsf[pos_deg].sz, &deg_f);
		r1up_file_convert_raw_sample_to_float(
			file, R1UP_FILE_VAR_ID_LONG_MIN,
			(const uint8_t*)file->vsd_read_buffers[pos_min] + sample_idx * file->vsf[pos_min].sz, &min_f);
		r1up_file_convert_raw_sample_to_float(
			file, R1UP_FILE_VAR_ID_LONG_DECI,
			(const uint8_t*)file->vsd_read_buffers[pos_deci] + sample_idx * file->vsf[pos_deci].sz, &deci_f);

		out_lon_dd[i] = r1up_file_convert_gps_longitude(deg_f, min_f, deci_f);
	}

	if (out_sample_count != NULL) {
		*out_sample_count = sample_count;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/* GPS COORDINATE RETRIEVAL - PUBLIC API WRAPPERS */
/*==================================================================================================================*/

r1up_file_result_t r1up_file_get_gps_latitude_dd(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_lat_dd,
												 r1up_file_size_t buffer_size) {
	/* Parameter validation */
	if (handle == NULL || out_lat_dd == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Delegate to range-based core: all samples = [0, total_sample_number - 1] */
	return r1up_file_get_gps_latitude_dd_range_internal(file, 0U, file->total_sample_number - 1U, out_lat_dd,
														buffer_size, NULL);
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_gps_longitude_dd(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_lon_dd,
												  r1up_file_size_t buffer_size) {
	/* Parameter validation */
	if (handle == NULL || out_lon_dd == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Delegate to range-based core: all samples = [0, total_sample_number - 1] */
	return r1up_file_get_gps_longitude_dd_range_internal(file, 0U, file->total_sample_number - 1U, out_lon_dd,
														 buffer_size, NULL);
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_gps_latitude_dd_by_lap(r1up_file_handle_t handle, uint32_t lap_number,
														ag_fixedpoint_float_t* out_lat_dd, r1up_file_size_t buffer_size,
														r1up_file_size_t* out_sample_count) {
	/* Parameter validation */
	if (handle == NULL || out_lat_dd == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Get the sample range for the requested lap segment */
	r1up_file_size_t start_sample = 0U;
	r1up_file_size_t end_sample = 0U;
	r1up_file_result_t res = r1up_file_get_lap_sample_range(handle, lap_number, &start_sample, &end_sample);
	if (res != R1UP_FILE_SUCCESS) {
		return res;
	}

	/* Delegate to range-based core */
	return r1up_file_get_gps_latitude_dd_range_internal(file, start_sample, end_sample, out_lat_dd, buffer_size,
														out_sample_count);
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_gps_longitude_dd_by_lap(r1up_file_handle_t handle, uint32_t lap_number,
														 ag_fixedpoint_float_t* out_lon_dd,
														 r1up_file_size_t buffer_size,
														 r1up_file_size_t* out_sample_count) {
	/* Parameter validation */
	if (handle == NULL || out_lon_dd == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Get the sample range for the requested lap segment */
	r1up_file_size_t start_sample = 0U;
	r1up_file_size_t end_sample = 0U;
	r1up_file_result_t res = r1up_file_get_lap_sample_range(handle, lap_number, &start_sample, &end_sample);
	if (res != R1UP_FILE_SUCCESS) {
		return res;
	}

	/* Delegate to range-based core */
	return r1up_file_get_gps_longitude_dd_range_internal(file, start_sample, end_sample, out_lon_dd, buffer_size,
														 out_sample_count);
}

/*==================================================================================================================*/
r1up_file_size_t r1up_file_get_body_sample_count(r1up_file_handle_t handle) {
	if (handle == NULL) {
		return 0;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Return total sample number calculated during file open/read */
	return file->total_sample_number;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_var_raw(r1up_file_handle_t handle, r1up_variable_id_t var_id,
										 r1up_var_raw_t* out_raw) {
	/* Parameter validation */
	if (handle == NULL || out_raw == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Find variable position via linear scan (O(N), N ≤ 28) */
	int32_t pos = r1up_file_find_var_position(file, var_id);
	if (pos < 0) {
		return R1UP_FILE_ERROR_NOT_FOUND; /* Variable not present in this file */
	}

	/* Get raw data buffer and type information from VSF */
	const void* raw_buffer = file->vsd_read_buffers[pos];
	r1up_type_id_t type_id = file->vsf[pos].type;

	/* Set type_id in output structure */
	out_raw->type_id = type_id;

	/* Assign buffer pointer to union based on runtime type */
	/* X-Macro pattern: Generate switch cases for all 13 types */
	switch (type_id) {
#define R1UP_TYPE_RAW_COPY_CASE(id_symbol, id_value, type_token, ...)                                         \
	case id_symbol: {                                                                                         \
		const R1UP_FILE_TYPE(type_token)* typed_buffer = (const R1UP_FILE_TYPE(type_token)*)raw_buffer;       \
		out_raw->value.R1UP_FILE_TYPE_AS_UNION_MEMBER(type_token) = typed_buffer; /* Assign buffer pointer */ \
		break;                                                                                                \
	}
		R1UP_FILE_TYPE_ID_TABLE(R1UP_TYPE_RAW_COPY_CASE)
#undef R1UP_TYPE_RAW_COPY_CASE

		default:
			/* Unknown type - should never happen if database is consistent */
			return R1UP_FILE_ERROR_CORRUPTED;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_var_type_id(r1up_file_handle_t handle, r1up_variable_id_t var_id,
											 r1up_type_id_t* out_type_id) {
	/* Parameter validation */
	if (handle == NULL || out_type_id == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if VSF has been loaded */
	if (file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Find variable position via linear scan (O(N), N ≤ 28) */
	int32_t pos = r1up_file_find_var_position(file, var_id);
	if (pos < 0) {
		return R1UP_FILE_ERROR_NOT_FOUND; /* Variable not present in this file */
	}

	/* Get type ID from VSF entry at this position */
	*out_type_id = (r1up_type_id_t)file->vsf[pos].type;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/* VARIABLE FLOAT RETRIEVAL - INTERNAL RANGE-BASED CORE FUNCTION */
/*==================================================================================================================*/

/**
 * @brief Internal: Extract variable samples in a given index range and convert to float.
 * @details Core function that all var_float getters delegate to. Performs the actual conversion
 *          from raw fixed-point to float based on runtime type information from VSF.
 *
 * @param file            Internal file structure (already validated by caller)
 * @param var_id          Variable ID to retrieve
 * @param start_sample    First sample index (inclusive)
 * @param end_sample      Last sample index (inclusive)
 * @param[out] out_float  Caller-allocated output buffer
 * @param buffer_size     Number of elements in out_float
 * @param[out] out_sample_count Optional: receives actual sample count written
 * @return R1UP_FILE_SUCCESS or error code
 *
 * @note Caller is responsible for parameter validation and body-read checks.
 */
static r1up_file_result_t r1up_file_get_var_float_range_internal(
	r1up_file_t* file, r1up_variable_id_t var_id, r1up_file_size_t start_sample, r1up_file_size_t end_sample,
	ag_fixedpoint_float_t* out_float, r1up_file_size_t buffer_size, r1up_file_size_t* out_sample_count) {
	/* Compute sample count */
	const r1up_file_size_t sample_count = (r1up_file_size_t)(end_sample - start_sample + 1U);

	/* Verify buffer is large enough */
	if (buffer_size < sample_count) {
		return R1UP_FILE_ERROR_INVALID_PARAM; /* Buffer too small */
	}

	/* Find variable position via linear scan (O(N), N ≤ 28) */
	int32_t pos = r1up_file_find_var_position(file, var_id);
	if (pos < 0) {
		return R1UP_FILE_ERROR_NOT_FOUND; /* Variable not present in this file */
	}

	/* Get raw data buffer and type information from VSF */
	const void* raw_buffer = file->vsd_read_buffers[pos];
	r1up_type_id_t type_id = file->vsf[pos].type;

	/* Convert samples [start_sample … end_sample] from raw fixed-point to float */
	/* X-Macro pattern: Generate switch cases for all 13 types */
	switch (type_id) {
#define R1UP_TYPE_FLOAT_RANGE_CASE(id_symbol, id_value, type_token, ...)                                \
	case id_symbol: {                                                                                   \
		const R1UP_FILE_TYPE(type_token)* typed_buffer = (const R1UP_FILE_TYPE(type_token)*)raw_buffer; \
		r1up_file_size_t i;                                                                             \
		for (i = 0U; i < sample_count; i++) {                                                           \
			out_float[i] = ag_fixedpoint_##type_token##_to_float(typed_buffer[start_sample + i]);       \
		}                                                                                               \
		break;                                                                                          \
	}
		R1UP_FILE_TYPE_ID_TABLE(R1UP_TYPE_FLOAT_RANGE_CASE)
#undef R1UP_TYPE_FLOAT_RANGE_CASE

		default:
			/* Unknown type - should never happen if database is consistent */
			return R1UP_FILE_ERROR_CORRUPTED;
	}

	/* Report actual sample count to caller (optional output) */
	if (out_sample_count != NULL) {
		*out_sample_count = sample_count;
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/* VARIABLE FLOAT RETRIEVAL - PUBLIC API WRAPPERS */
/*==================================================================================================================*/

r1up_file_result_t r1up_file_get_var_float(r1up_file_handle_t handle, r1up_variable_id_t var_id,
										   ag_fixedpoint_float_t* out_float, r1up_file_size_t buffer_size) {
	/* Parameter validation */
	if (handle == NULL || out_float == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Delegate to range-based core: all samples = [0, total_sample_number - 1] */
	return r1up_file_get_var_float_range_internal(file, var_id, 0U, file->total_sample_number - 1U, out_float,
												  buffer_size, NULL);
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_get_var_float_by_lap(r1up_file_handle_t handle, r1up_variable_id_t var_id,
												  uint32_t lap_number, ag_fixedpoint_float_t* out_float,
												  r1up_file_size_t buffer_size, r1up_file_size_t* out_sample_count) {
	/* Parameter validation */
	if (handle == NULL || out_float == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if body has been read */
	if (file->vsd_read_buffers == NULL || file->vsf == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Get the sample range for the requested lap segment */
	r1up_file_size_t start_sample = 0U;
	r1up_file_size_t end_sample = 0U;
	r1up_file_result_t res = r1up_file_get_lap_sample_range(handle, lap_number, &start_sample, &end_sample);
	if (res != R1UP_FILE_SUCCESS) {
		return res; /* Propagate: NOT_FOUND (no crossings) or INVALID_PARAM (bad lap_number) */
	}

	/* Delegate to range-based core */
	return r1up_file_get_var_float_range_internal(file, var_id, start_sample, end_sample, out_float, buffer_size,
												  out_sample_count);
}

/*==================================================================================================================*/
static r1up_file_result_t r1up_file_convert_raw_sample_to_float(r1up_file_t* file, r1up_variable_id_t var_id,
																const void* raw_value,
																ag_fixedpoint_float_t* out_value) {
	/* Parameter validation */
	if (file == NULL || raw_value == NULL || out_value == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	/* Find variable position via linear scan (O(N), N ≤ 28) */
	int32_t pos = r1up_file_find_var_position(file, var_id);
	if (pos < 0) {
		return R1UP_FILE_ERROR_NOT_FOUND; /* Variable not present in this file */
	}

	switch (file->vsf[pos].type) {
#define R1UP_TYPE_SINGLE_VALUE_TO_FLOAT_CASE(id_symbol, id_value, type_token, ...)                    \
	case id_symbol: {                                                                                 \
		const R1UP_FILE_TYPE(type_token)* typed_value = (const R1UP_FILE_TYPE(type_token)*)raw_value; \
		*out_value = ag_fixedpoint_##type_token##_to_float(*typed_value);                             \
		break;                                                                                        \
	}
		R1UP_FILE_TYPE_ID_TABLE(R1UP_TYPE_SINGLE_VALUE_TO_FLOAT_CASE)
#undef R1UP_TYPE_SINGLE_VALUE_TO_FLOAT_CASE
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_stream_body_begin(r1up_file_handle_t handle) {
	if (handle == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if file is in valid state */
	if (file->fsal_handle_ptr == NULL || file->varible_number == 0) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Allocate sample-and-hold buffer (one ag_fixedpoint_float_t per variable position) */
	/* Position-based indexing: stream_last_values[position] where position = 0..varible_number-1 */
	file->stream_last_values = (ag_fixedpoint_float_t*)AG_MALLOC(file->varible_number * sizeof(ag_fixedpoint_float_t));
	if (file->stream_last_values == NULL) {
		return R1UP_FILE_ERROR_MEMORY;
	}

	/* Initialize all values to 0.0 (sample 0 will overwrite with real values) */
	memset(file->stream_last_values, 0, file->varible_number * sizeof(ag_fixedpoint_float_t));

	/* Initialize streaming state */
	file->stream_current_sample = 0;
	file->stream_initialized = true;

	/* Position file pointer at start of body variable data */
	fsal_result_t fsal_res = fsal_seek(file->fsal_handle_ptr, file->body_offset_var_data_start, SEEK_SET);
	if (fsal_res != FSAL_SUCCESS) {
		AG_FREE(file->stream_last_values);
		file->stream_last_values = NULL;
		file->stream_initialized = false;
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	/* Enable buffering for efficient sequential reads */
	fsal_res = fsal_enable_buffering(file->fsal_handle_ptr, FSAL_BUFFER_FULL, 512U);
	if (fsal_res != FSAL_SUCCESS) {
		AG_FREE(file->stream_last_values);
		file->stream_last_values = NULL;
		file->stream_initialized = false;
		return r1up_file_convert_fsal_result_to_r1up(fsal_res);
	}

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
/**
 * @brief Read next row from file in streaming mode
 * @details Implements sample-and-hold logic: variables with frequency divisor > 1 are read only
 * at specific sample indices (sample % fd == 0), otherwise the previous value is held.
 * Uses runtime VSF type information to convert raw samples to float dynamically.
 * Works uniformly for both R1UP and legacy RER file formats via runtime VSF linear lookup.
 *
 * @param handle File handle obtained from r1up_file_open()
 * @param out_values Output array for variable values (indexed by position 0..var_count-1)
 * @param array_size Size of output array (must be >= variable count)
 * @return R1UP_FILE_SUCCESS on success, R1UP_FILE_ERROR_IO on EOF, or other error codes
 */
r1up_file_result_t r1up_file_stream_body_next_row(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_values,
												  r1up_file_size_t array_size) {
	/* Parameter validation */
	if (handle == NULL || out_values == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Check if streaming is initialized */
	if (!file->stream_initialized || file->stream_last_values == NULL) {
		return R1UP_FILE_ERROR_NOT_INITIALIZED;
	}

	/* Check array size */
	if (array_size < file->varible_number) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	/* Check if end of samples reached (return IO error to signal completion) */
	if (file->stream_current_sample >= file->total_sample_number) {
		return R1UP_FILE_ERROR_IO; /* EOF condition */
	}

	/* Process each variable in the file (VSF already contains only variables present in this file) */
	uint32_t var_idx;
	for (var_idx = 0; var_idx < file->varible_number; var_idx++) {
		r1up_file_vsf_t* vsf_entry = &file->vsf[var_idx];

		/* Check if this variable should be sampled at the current sample index (sample-and-hold logic) */
		if (file->stream_current_sample % vsf_entry->fd == 0) {
			/* Variable is sampled - read raw value from file */
			/* Allocate temporary buffer on stack for the raw value (max size is 4 bytes for q31_0/uq32_0) */
			uint8_t raw_buffer[4]; /* Size adequate for all supported types (8/16/32 bit) */
			size_t bytes_read = 0;

			fsal_result_t fsal_res = fsal_read(raw_buffer, 1, vsf_entry->sz, file->fsal_handle_ptr, &bytes_read);
			if (fsal_res != FSAL_SUCCESS || bytes_read != vsf_entry->sz) {
				return r1up_file_convert_fsal_result_to_r1up(fsal_res);
			}

			/* Convert raw value to float using runtime type information from VSF */
			ag_fixedpoint_float_t temp_float_value;
			r1up_file_result_t conv_res =
				r1up_file_convert_raw_sample_to_float(file, vsf_entry->id, raw_buffer, &temp_float_value);
			if (conv_res != R1UP_FILE_SUCCESS) {
				return conv_res;
			}

			/* Update sample-and-hold buffer with newly read value */
			file->stream_last_values[var_idx] = temp_float_value;
		}
		/* else: variable not sampled this iteration, keep previous value in stream_last_values (sample-and-hold) */
	}

	/* Copy all values to output array - already indexed by position */
	for (var_idx = 0; var_idx < file->varible_number; var_idx++) {
		out_values[var_idx] = file->stream_last_values[var_idx];
	}

	/* Increment sample counter */
	file->stream_current_sample++;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
r1up_file_result_t r1up_file_stream_body_end(r1up_file_handle_t handle) {
	if (handle == NULL) {
		return R1UP_FILE_ERROR_INVALID_PARAM;
	}

	r1up_file_t* file = (r1up_file_t*)handle;

	/* Disable buffering */
	if (file->fsal_handle_ptr != NULL) {
		fsal_enable_buffering(file->fsal_handle_ptr, FSAL_BUFFER_NONE, 0U);
	}

	/* Free sample-and-hold buffer if allocated */
	if (file->stream_last_values != NULL) {
		AG_FREE(file->stream_last_values);
		file->stream_last_values = NULL;
	}

	/* Reset streaming state */
	file->stream_initialized = false;
	file->stream_current_sample = 0;

	return R1UP_FILE_SUCCESS;
}

/*==================================================================================================================*/
#if (R1UP_FILE_BUILD_ON_MATLAB == 1)
/**
 * @brief MATLAB MEX interface for reading R1UP/ReRace files
 * @details Dynamically scans VSF to determine which variables are present in the file
 * and creates MATLAB structure fields accordingly. Works with both modern R1UP and
 * legacy ReRace .rer files.
 *
 * @param[in] prhs[0] Filename string (required)
 * @param[out] plhs[0] MATLAB structure with fields for each variable (raw and float versions)
 */
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
	// Check number of input parameters
	if (nrhs < 1) {
		mexErrMsgIdAndTxt("r1up_file:invalidNumInputs", "At least one parameter required: filename");
	}

	// Verify that the first parameter is a string
	if (!mxIsChar(prhs[0])) {
		mexErrMsgIdAndTxt("r1up_file:invalidInputType", "First parameter must be a string (filename)");
	}

	// Extract filename from mxArray
	char* filename = mxArrayToString(prhs[0]);
	if (filename == NULL) {
		mexErrMsgIdAndTxt("r1up_file:conversionFailed", "Unable to convert filename to string");
	}

	// Open file in read-only mode
	r1up_file_handle_t handle = r1up_file_open(filename, R1UP_FILE_MODE_READ_ONLY);

	// Free allocated memory for filename
	mxFree(filename);

	// Check if file was opened successfully
	if (handle == NULL) {
		mexErrMsgIdAndTxt("r1up_file:openFailed", "Unable to open file");
	}

	// Read file body
	r1up_file_result_t read_result = r1up_file_read_body(handle);
	if (read_result != R1UP_FILE_SUCCESS) {
		r1up_file_close(handle);
		mexErrMsgIdAndTxt("r1up_file:readBodyFailed", "Unable to read file body");
	}

	// Get internal file structure to access VSF
	r1up_file_t* file = (r1up_file_t*)handle;

	// Create MATLAB structure output if requested
	if (nlhs >= 1) {
		// Build field names array dynamically from VSF
		// We need: var_count raw fields + var_count float fields = 2 * var_count total
		const int num_vars = (int)file->varible_number;
		const int num_fields = num_vars * 2 + 2;  // raw + float + lap_count + lap_segments

		// Allocate array of field name pointers
		char** field_names = mxCalloc(num_fields, sizeof(char*));
		if (field_names == NULL) {
			r1up_file_close(handle);
			mexErrMsgIdAndTxt("r1up_file:allocationFailed", "Unable to allocate field names array");
		}

		// Macro to lookup variable name from ID using R1UP_FILE_VARIABLE_ID_TABLE
		// Uses stringification (#) on the short_name token to convert to string
#define VARIABLE_ID_TO_NAME_CASE(var_id_symbol, var_id_value, short_name_token, description) \
	case var_id_value:                                                                       \
		return #short_name_token;

		// Helper function to get variable name from Variable ID
		const char* get_variable_name(r1up_variable_id_t var_id) {
			switch (var_id) {
				R1UP_FILE_VARIABLE_ID_TABLE(VARIABLE_ID_TO_NAME_CASE)
				default:
					return "unknown_var";
			}
		}
#undef VARIABLE_ID_TO_NAME_CASE

		// Populate field names for raw and float versions
		for (int i = 0; i < num_vars; i++) {
			r1up_variable_id_t var_id = file->vsf[i].id;
			const char* var_name = get_variable_name(var_id);

			// Raw field name (e.g., "rpm")
			field_names[i] = mxCalloc(strlen(var_name) + 1, sizeof(char));
			strcpy(field_names[i], var_name);

			// Float field name (e.g., "rpm_float")
			field_names[num_vars + i] = mxCalloc(strlen(var_name) + 7, sizeof(char));  // +7 for "_float\0"
			sprintf(field_names[num_vars + i], "%s_float", var_name);
		}

		// Lap metadata field names
		field_names[num_vars * 2] = mxCalloc(10, sizeof(char));
		strcpy(field_names[num_vars * 2], "lap_count");
		field_names[num_vars * 2 + 1] = mxCalloc(13, sizeof(char));
		strcpy(field_names[num_vars * 2 + 1], "lap_segments");

		// Create MATLAB structure with one element
		plhs[0] = mxCreateStructMatrix(1, 1, num_fields, (const char**)field_names);
		if (plhs[0] == NULL) {
			// Free allocated field names
			for (int i = 0; i < num_fields; i++) {
				mxFree(field_names[i]);
			}
			mxFree(field_names);
			r1up_file_close(handle);
			mexErrMsgIdAndTxt("r1up_file:allocationFailed", "Unable to allocate output structure");
		}

		// Get total sample count
		r1up_file_size_t total_samples = r1up_file_get_body_sample_count(handle);

		// Determine MATLAB float class based on ag_fixedpoint configuration
#if (AG_FIXEDPOINT_FLOAT_TYPE == 32)
#define MATLAB_FLOAT_CLASS mxSINGLE_CLASS
#elif (AG_FIXEDPOINT_FLOAT_TYPE == 64)
#define MATLAB_FLOAT_CLASS mxDOUBLE_CLASS
#else
#error "AG_FIXEDPOINT_FLOAT_TYPE must be either 32 or 64"
#endif

		// Helper function to get MATLAB class from type ID
		mxClassID get_matlab_class_from_type(r1up_type_id_t type_id) {
#define TYPE_TO_MATLAB_CLASS_CASE(type_id_symbol, type_id_value, type_token, ...)           \
	case type_id_value: {                                                                   \
		typedef R1UP_FILE_TYPE(type_token) type_t;                                          \
		return (sizeof(type_t) == 1	  ? ((type_t)(-1) < 0 ? mxINT8_CLASS : mxUINT8_CLASS)   \
				: sizeof(type_t) == 2 ? ((type_t)(-1) < 0 ? mxINT16_CLASS : mxUINT16_CLASS) \
				: sizeof(type_t) == 4 ? ((type_t)(-1) < 0 ? mxINT32_CLASS : mxUINT32_CLASS) \
									  : mxDOUBLE_CLASS);                                    \
	}

			switch (type_id) {
				R1UP_FILE_TYPE_ID_TABLE(TYPE_TO_MATLAB_CLASS_CASE)
				default:
					return mxDOUBLE_CLASS;
			}
#undef TYPE_TO_MATLAB_CLASS_CASE
		}

		// Populate raw data fields
		for (int i = 0; i < num_vars; i++) {
			r1up_variable_id_t var_id = file->vsf[i].id;
			r1up_type_id_t type_id;

			// Get type ID for this variable
			if (r1up_file_get_var_type_id(handle, var_id, &type_id) != R1UP_FILE_SUCCESS) {
				continue;  // Skip if type lookup fails
			}

			// Get raw data using new API
			r1up_var_raw_t raw_data;
			if (r1up_file_get_var_raw(handle, var_id, &raw_data) != R1UP_FILE_SUCCESS) {
				continue;  // Skip if data retrieval fails
			}

			// Create MATLAB array with appropriate type
			mxClassID mx_class = get_matlab_class_from_type(type_id);
			mxArray* field_array = mxCreateNumericMatrix((mwSize)total_samples, 1, mx_class, mxREAL);

			if (field_array != NULL) {
				void* output_data = mxGetData(field_array);

				// Copy data based on type (using switch on type_id to get correct pointer from union)
#define COPY_RAW_DATA_CASE(type_id_symbol, type_id_value, type_token, ...)                                 \
	case type_id_value: {                                                                                  \
		const R1UP_FILE_TYPE(type_token)* src = raw_data.value.R1UP_FILE_TYPE_AS_UNION_MEMBER(type_token); \
		memcpy(output_data, src, total_samples * sizeof(R1UP_FILE_TYPE(type_token)));                      \
		break;                                                                                             \
	}

				switch (type_id) {
					R1UP_FILE_TYPE_ID_TABLE(COPY_RAW_DATA_CASE)
					default:
						break;
				}
#undef COPY_RAW_DATA_CASE

				// Set field in MATLAB structure
				mxSetFieldByNumber(plhs[0], 0, i, field_array);
			}
		}

		// Populate float converted data fields
		for (int i = 0; i < num_vars; i++) {
			r1up_variable_id_t var_id = file->vsf[i].id;

			// Allocate buffer for float data
			mxArray* field_array = mxCreateNumericMatrix((mwSize)total_samples, 1, MATLAB_FLOAT_CLASS, mxREAL);

			if (field_array != NULL) {
				ag_fixedpoint_float_t* float_buffer = (ag_fixedpoint_float_t*)mxGetData(field_array);

				// Convert to float using new API
				r1up_file_result_t res = r1up_file_get_var_float(handle, var_id, float_buffer, total_samples);

				if (res == R1UP_FILE_SUCCESS) {
					// Set field in MATLAB structure (offset by num_vars for float fields)
					mxSetFieldByNumber(plhs[0], 0, num_vars + i, field_array);
				} else {
					mxDestroyArray(field_array);
				}
			}
		}

#undef MATLAB_FLOAT_CLASS

		// ── Populate lap metadata fields ─────────────────────────────────────────────
		{
			uint32_t lap_count = 0U;
			(void)r1up_file_get_lap_count(handle, &lap_count);

			// lap_count → double scalar (MATLAB-native numeric type)
			mxArray* lap_count_arr = mxCreateDoubleScalar((double)lap_count);
			if (lap_count_arr != NULL) {
				mxSetFieldByNumber(plhs[0], 0, num_vars * 2, lap_count_arr);
			}

			// lap_segments → (total_segments × 2) double matrix, 1-based indices
			// Segments: outlap(0) + lap_count complete laps + inlap = lap_count + 2
			// When lap_count == 0: probe segment 0 to detect single-crossing case
			uint32_t total_segments = 0U;
			if (lap_count > 0U) {
				total_segments = lap_count + 2U;
			} else {
				r1up_file_size_t s0 = 0U, e0 = 0U;
				if (r1up_file_get_lap_sample_range(handle, 0U, &s0, &e0) == R1UP_FILE_SUCCESS) {
					total_segments = 2U;  // 1 crossing → outlap + inlap
				}
			}

			mxArray* lap_seg_arr = mxCreateDoubleMatrix((mwSize)total_segments, 2, mxREAL);
			if (lap_seg_arr != NULL && total_segments > 0U) {
				double* seg_data = mxGetPr(lap_seg_arr);
				// MATLAB column-major: col 1 = start_sample, col 2 = end_sample (both 1-based)
				uint32_t seg;
				for (seg = 0U; seg < total_segments; seg++) {
					r1up_file_size_t start_s = 0U, end_s = 0U;
					if (r1up_file_get_lap_sample_range(handle, seg, &start_s, &end_s) == R1UP_FILE_SUCCESS) {
						seg_data[seg] = (double)(start_s + 1U);					// +1 → MATLAB 1-based
						seg_data[total_segments + seg] = (double)(end_s + 1U);	// +1 → MATLAB 1-based
					}
				}
			}
			if (lap_seg_arr != NULL) {
				mxSetFieldByNumber(plhs[0], 0, num_vars * 2 + 1, lap_seg_arr);
			}
		}

		// Free allocated field names
		for (int i = 0; i < num_fields; i++) {
			mxFree(field_names[i]);
		}
		mxFree(field_names);
	}

	// Close file
	r1up_file_result_t result = r1up_file_close(handle);
	if (result != R1UP_FILE_SUCCESS) {
		mexWarnMsgIdAndTxt("r1up_file:closeFailed", "Error closing file");
	}
}
#endif
// End of r1up_file.c
