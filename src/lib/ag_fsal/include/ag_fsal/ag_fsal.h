/**
 * @file fsal.h
 * @brief File System Abstraction Layer (FSAL) - Public API
 *
 * This header file provides the public interface for the File System
 * Abstraction Layer. FSAL enables seamless portability between different
 * file system backends, including standard C library, FatFS for embedded
 * systems, and custom implementations.
 *
 * @author Alex Giov
 * @date September 27, 2025
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2025, Race1Up Project.
 * All rights reserved.
 *
 * @par Key Features:
 * - Unified API across multiple file system backends
 * - Zero-overhead abstraction through compile-time selection
 * - Comprehensive error handling and reporting
 * - Thread-safe operations (configurable)
 * - Debug and statistics support
 *
 * @par Supported Backends:
 * - **Standard C Library**: Desktop and server environments
 * - **FatFS**: Embedded systems with SD cards, flash memory
 * - **Custom**: User-defined implementations via function pointers
 *
 * @par Usage Example:
 * @code
 * #include "fsal.h"
 *
 * // Initialize the library
 * fsal_result_t result = fsal_init();
 * if (result != FSAL_SUCCESS) {
 *     // Handle initialization error
 *     return -1;
 * }
 *
 * // Open a file
 * fsal_handle_t handle;
 * result = fsal_open("data.bin", FSAL_MODE_READ, &handle);
 * if (result == FSAL_SUCCESS) {
 *     // Read data
 *     char buffer[256];
 *     size_t bytes_read;
 *     fsal_read(buffer, 1, sizeof(buffer), &handle, &bytes_read);
 *
 *     // Close file
 *     fsal_close(&handle);
 * }
 *
 * // Cleanup
 * fsal_deinit();
 * @endcode
 */

#ifndef AG_FSAL_H
#define AG_FSAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* INCLUDES                                                                   */
/*============================================================================*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ag_fsal_cfg.h"

/* Conditional includes based on configured backend */
#if (FSAL_CFG_BACKEND == 0)
#include <stdio.h>
#elif (FSAL_CFG_BACKEND == 1)
#include "ff.h" /* FatFS header - ensure this is available in your project */
#endif

/*============================================================================*/
/* CONSTANTS AND MACROS                                                       */
/*============================================================================*/

/**
 * @defgroup FSAL_Constants FSAL Constants
 * @brief Constants and macros for FSAL operations
 * @{
 */

/** @brief Maximum file path length supported by FSAL */
#define FSAL_MAX_PATH_LENGTH FSAL_CFG_MAX_PATH_LENGTH

/** @brief Invalid file handle constant */
#define FSAL_INVALID_HANDLE NULL

/* File seek origins - compatible with standard C library */
#ifndef SEEK_SET
#define SEEK_SET 0 /**< Seek from beginning of file */
#define SEEK_CUR 1 /**< Seek from current position */
#define SEEK_END 2 /**< Seek from end of file */
#endif

/** @} */ /* End of FSAL_Constants group */

/*============================================================================*/
/* TYPE DEFINITIONS                                                           */
/*============================================================================*/

/**
 * @defgroup FSAL_Types FSAL Type Definitions
 * @brief Type definitions for FSAL operations
 * @{
 */

/**
 * @brief FSAL operation result codes
 *
 * This enumeration defines all possible return values from FSAL functions.
 * Always check the return value to ensure proper error handling.
 */
typedef enum {
	FSAL_SUCCESS = 0,			/**< Operation completed successfully */
	FSAL_ERROR_INVALID_PARAM,	/**< Invalid parameter passed to function */
	FSAL_ERROR_NOT_FOUND,		/**< Specified file does not exist */
	FSAL_ERROR_ACCESS_DENIED,	/**< File access permissions denied */
	FSAL_ERROR_NO_SPACE,		/**< Insufficient disk space */
	FSAL_ERROR_IO,				/**< Input/output operation failed */
	FSAL_ERROR_MEMORY,			/**< Memory allocation failed */
	FSAL_ERROR_CORRUPTED,		/**< File or file system corrupted */
	FSAL_ERROR_UNSUPPORTED,		/**< Operation not supported by backend */
	FSAL_ERROR_TIMEOUT,			/**< Operation timed out */
	FSAL_ERROR_BUSY,			/**< Resource is busy/locked */
	FSAL_ERROR_NOT_INITIALIZED, /**< Library not initialized */
	FSAL_ERROR_ALREADY_EXISTS,	/**< File already exists */
	FSAL_ERROR_TOO_MANY_OPEN	/**< Maximum open files limit reached */
} fsal_result_t;

/**
 * @brief File access modes
 *
 * These modes define how a file should be opened and accessed.
 */
typedef enum {
	FSAL_MODE_READ = 0,			 /**< Read-only access */
	FSAL_MODE_WRITE,			 /**< Write-only access (truncates existing file) */
	FSAL_MODE_APPEND,			 /**< Append mode (writes at end of file) */
	FSAL_MODE_READ_WRITE,		 /**< Read-write access to existing file */
	FSAL_MODE_CREATE_READ_WRITE, /**< Create new file or open existing for read-write */
	FSAL_MODE_CREATE_NEW		 /**< Create new file only, fail if it already exists */
} fsal_mode_t;

#if FSAL_CFG_READ_BUFFERING_ENABLED
/**
 * @brief Read buffer modes
 *
 * These modes control how the read buffer behaves for a file handle.
 */
typedef enum {
	FSAL_BUFFER_NONE = 0, /**< No buffering - direct I/O to backend */
	FSAL_BUFFER_FULL,	  /**< Full buffering with fixed read-ahead */
	FSAL_BUFFER_ADAPTIVE  /**< Adaptive buffering - adjusts to access pattern */
} fsal_buffer_mode_t;
#endif

/**
 * @brief File handle structure
 *
 * This structure contains all information needed to manage an open file
 * across different file system backends. The structure is designed to be
 * opaque to the user, but the internal layout is optimized for each backend.
 */
typedef struct {
	/* Backend-specific handle */
#if (FSAL_CFG_BACKEND == 0)
	FILE* native_handle; /**< Standard C library file pointer */
#elif (FSAL_CFG_BACKEND == 1)
	FIL native_handle; /**< FatFS file handle */
#elif (FSAL_CFG_BACKEND == 2)
	void* native_handle; /**< Custom backend handle */
#endif

	/* Common handle information */
	fsal_mode_t mode;	/**< File access mode */
	bool is_open;		/**< File open status */
	uint32_t handle_id; /**< Unique handle identifier */

#if FSAL_CFG_READ_BUFFERING_ENABLED
	/* Read buffering support */
	struct {
		uint8_t* data;			 /**< Buffer data (dynamically allocated) */
		size_t capacity;		 /**< Buffer capacity in bytes */
		size_t valid_bytes;		 /**< Number of valid bytes in buffer */
		long buffer_file_offset; /**< File offset of first byte in buffer */
		fsal_buffer_mode_t mode; /**< Buffering mode */
		bool enabled;			 /**< Buffering enabled flag */

		/* Statistics for adaptive mode */
		uint32_t sequential_reads; /**< Counter for sequential read operations */
		uint32_t random_reads;	   /**< Counter for random read operations */
		long last_read_offset;	   /**< File offset of last read operation */
	} buffer;
#endif

#if FSAL_CFG_STATISTICS_ENABLED
	/* Statistics (optional) */
	struct {
		size_t bytes_read;	  /**< Total bytes read from this file */
		size_t bytes_written; /**< Total bytes written to this file */
		uint32_t read_ops;	  /**< Number of read operations */
		uint32_t write_ops;	  /**< Number of write operations */
		uint32_t seek_ops;	  /**< Number of seek operations */
#if FSAL_CFG_READ_BUFFERING_ENABLED
		uint32_t buffer_hits;	/**< Number of buffer hits */
		uint32_t buffer_misses; /**< Number of buffer misses */
#endif
	} stats;
#endif

#if FSAL_CFG_DEBUG_ENABLED
	/* Debug information (optional) */
	char debug_path[64];	 /**< Truncated file path for debugging */
	uint32_t open_timestamp; /**< When the file was opened (ms) */
#endif
} fsal_handle_t;

/**
 * @brief File information structure
 *
 * Contains metadata about a file. Used by fsal_stat() and similar functions.
 */
typedef struct {
	size_t size;				/**< File size in bytes */
	uint32_t creation_time;		/**< File creation timestamp (if available) */
	uint32_t modification_time; /**< Last modification timestamp (if available) */
	bool is_directory;			/**< True if this is a directory */
	bool is_readonly;			/**< True if file is read-only */
	bool exists;				/**< True if file exists */
} fsal_file_info_t;

/**
 * @brief Directory entry information
 *
 * Contains metadata about a single directory entry returned by fsal_readdir().
 */
typedef struct {
	char name[FSAL_MAX_PATH_LENGTH]; /**< Entry name (file or directory name, not full path) */
	size_t size;					 /**< File size in bytes (0 for directories) */
	bool is_directory;				 /**< True if this entry is a directory */
} fsal_dir_entry_t;

/**
 * @brief Directory handle structure
 *
 * Opaque handle used for directory iteration via fsal_opendir/fsal_readdir/fsal_closedir.
 */
typedef struct {
#if (FSAL_CFG_BACKEND == 0)
	void* native_handle; /**< Platform DIR* (cast to void* for portability) */
#elif (FSAL_CFG_BACKEND == 1)
	DIR native_handle; /**< FatFS directory object */
#elif (FSAL_CFG_BACKEND == 2)
	void* native_handle; /**< Custom backend handle */
#endif
	bool is_open; /**< Directory open status */
} fsal_dir_handle_t;

#if (FSAL_CFG_BACKEND == 2)
/**
 * @brief Custom backend function pointers
 *
 * When using custom backend (FSAL_CFG_BACKEND == 2), these function
 * pointers must be set to point to your custom implementation.
 */
typedef struct {
	fsal_result_t (*open)(const char* path, fsal_mode_t mode, void** handle);
	fsal_result_t (*close)(void* handle);
	fsal_result_t (*read)(void* handle, void* buffer, size_t size, size_t count, size_t* bytes_read);
	fsal_result_t (*write)(void* handle, const void* buffer, size_t size, size_t count, size_t* bytes_written);
	fsal_result_t (*seek)(void* handle, long offset, int origin);
	fsal_result_t (*tell)(void* handle, long* position);
	fsal_result_t (*flush)(void* handle);
	fsal_result_t (*stat)(const char* path, fsal_file_info_t* info);
} fsal_custom_interface_t;
#endif

/** @} */ /* End of FSAL_Types group */

/*============================================================================*/
/* FUNCTION DECLARATIONS                                                      */
/*============================================================================*/

/**
 * @defgroup FSAL_Core_Functions FSAL Core Functions
 * @brief Core library management functions
 * @{
 */

/**
 * @brief Initialize the FSAL library
 *
 * This function must be called before using any other FSAL functions.
 * It initializes internal data structures and prepares the selected
 * backend for use.
 *
 * @return FSAL_SUCCESS on successful initialization, error code otherwise
 *
 * @note This function is thread-safe and can be called multiple times safely.
 * @note For embedded systems, ensure the underlying file system (e.g., FatFS)
 *       is properly mounted before calling this function.
 *
 * @see fsal_deinit()
 */
fsal_result_t fsal_init(void);

/**
 * @brief Deinitialize the FSAL library
 *
 * Cleans up resources allocated by the FSAL library. Should be called
 * when the library is no longer needed.
 *
 * @return FSAL_SUCCESS on successful cleanup, error code otherwise
 *
 * @warning Ensure all file handles are closed before calling this function.
 * @warning After calling this function, no other FSAL functions should be used
 *          until fsal_init() is called again.
 *
 * @see fsal_init()
 */
fsal_result_t fsal_deinit(void);

/**
 * @brief Get FSAL library version information
 *
 * Returns the current version of the FSAL library as a string.
 *
 * @return Pointer to null-terminated version string (e.g., "1.0.0")
 *
 * @note The returned pointer is valid for the lifetime of the application.
 * @note This function can be called before fsal_init().
 */
const char* fsal_get_version(void);

/**
 * @brief Get current backend information
 *
 * Returns information about the currently configured backend.
 *
 * @return Backend identifier (0=Standard C, 1=FatFS, 2=Custom)
 *
 * @note This function can be called before fsal_init().
 */
uint8_t fsal_get_backend(void);

/** @} */ /* End of FSAL_Core_Functions group */

/**
 * @defgroup FSAL_File_Functions FSAL File Operations
 * @brief File manipulation and I/O functions
 * @{
 */

/**
 * @brief Open a file
 *
 * Opens a file with the specified access mode and returns a handle for
 * subsequent operations.
 *
 * @param[in] path Path to the file to open
 * @param[in] mode File access mode
 * @param[out] handle Pointer to handle structure to initialize
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre path != NULL
 * @pre handle != NULL
 * @pre fsal_init() has been called successfully
 *
 * @note The handle must be closed with fsal_close() to free resources.
 * @note File paths are interpreted according to the backend:
 *       - Standard C: Native OS paths
 *       - FatFS: May require drive prefix (e.g., "0:filename.txt")
 *       - Custom: Backend-defined format
 *
 * @par Example:
 * @code
 * fsal_handle_t handle;
 * fsal_result_t result = fsal_open("data.bin", FSAL_MODE_READ, &handle);
 * if (result == FSAL_SUCCESS) {
 *     // File opened successfully, use handle for I/O operations
 *     // ... perform operations ...
 *     fsal_close(&handle);
 * }
 * @endcode
 */
fsal_result_t fsal_open(const char* path, fsal_mode_t mode, fsal_handle_t* handle);

/**
 * @brief Close a file
 *
 * Closes a file opened with fsal_open() and releases associated resources.
 *
 * @param[in,out] handle Pointer to handle structure to close
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre handle != NULL
 * @pre handle->is_open == true
 *
 * @note After calling this function, the handle should not be used for
 *       any other operations.
 * @note This function automatically flushes any pending write operations.
 */
fsal_result_t fsal_close(fsal_handle_t* handle);

/**
 * @brief Read data from a file
 *
 * Reads data from a file opened with fsal_open().
 *
 * @param[out] buffer Buffer to store read data
 * @param[in] size Size of each element to read
 * @param[in] count Number of elements to read
 * @param[in] handle Pointer to open file handle
 * @param[out] bytes_read Number of bytes actually read (optional, can be NULL)
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre buffer != NULL
 * @pre handle != NULL && handle->is_open == true
 * @pre size > 0
 *
 * @note The total number of bytes to read is size * count
 * @note If bytes_read is less than size * count, it may indicate end-of-file
 *       or a read error. Check the return value to distinguish between these cases.
 */
fsal_result_t fsal_read(void* buffer, size_t size, size_t count, fsal_handle_t* handle, size_t* bytes_read);

/**
 * @brief Write data to a file
 *
 * Writes data to a file opened with fsal_open().
 *
 * @param[in] buffer Buffer containing data to write
 * @param[in] size Size of each element to write
 * @param[in] count Number of elements to write
 * @param[in] handle Pointer to open file handle
 * @param[out] bytes_written Number of bytes actually written (optional, can be NULL)
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre buffer != NULL
 * @pre handle != NULL && handle->is_open == true
 * @pre size > 0
 * @pre File opened in a mode that allows writing
 *
 * @note The total number of bytes to write is size * count
 * @note Data may be buffered by the underlying file system. Use fsal_flush()
 *       to ensure data is written to storage.
 */
fsal_result_t fsal_write(const void* buffer, size_t size, size_t count, fsal_handle_t* handle, size_t* bytes_written);

/**
 * @brief Set file position indicator
 *
 * Sets the file position indicator for subsequent read/write operations.
 *
 * @param[in] handle Pointer to open file handle
 * @param[in] offset Offset from the origin
 * @param[in] origin Position from where offset is added (SEEK_SET, SEEK_CUR, SEEK_END)
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre handle != NULL && handle->is_open == true
 * @pre origin is one of SEEK_SET, SEEK_CUR, or SEEK_END
 *
 * @note Setting position beyond end-of-file may have backend-specific behavior
 */
fsal_result_t fsal_seek(fsal_handle_t* handle, long offset, int origin);

/**
 * @brief Get current file position
 *
 * Gets the current file position indicator.
 *
 * @param[in] handle Pointer to open file handle
 * @param[out] position Current file position
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre handle != NULL && handle->is_open == true
 * @pre position != NULL
 */
fsal_result_t fsal_tell(fsal_handle_t* handle, long* position);

/**
 * @brief Flush file buffers
 *
 * Forces any buffered write data to be written to storage.
 *
 * @param[in] handle Pointer to open file handle
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre handle != NULL && handle->is_open == true
 *
 * @note This function ensures data consistency but may impact performance.
 *       Use judiciously in performance-critical applications.
 */
fsal_result_t fsal_flush(fsal_handle_t* handle);

/** @} */ /* End of FSAL_File_Functions group */

/**
 * @defgroup FSAL_Utility_Functions FSAL Utility Functions
 * @brief Additional utility and information functions
 * @{
 */

/**
 * @brief Get file information
 *
 * Retrieves information about a file without opening it.
 *
 * @param[in] path Path to the file
 * @param[out] info File information structure
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre path != NULL
 * @pre info != NULL
 * @pre fsal_init() has been called successfully
 *
 * @note This function works with both files and directories (backend permitting)
 */
fsal_result_t fsal_stat(const char* path, fsal_file_info_t* info);

/**
 * @brief Check if file exists
 *
 * Convenience function to check if a file exists.
 *
 * @param[in] path Path to the file to check
 * @param[out] exists True if file exists, false otherwise
 *
 * @return FSAL_SUCCESS if check completed, error code on failure
 *
 * @pre path != NULL
 * @pre exists != NULL
 */
fsal_result_t fsal_exists(const char* path, bool* exists);

/**
 * @brief Remove a file
 *
 * Deletes a file from the file system. The file must not be open.
 *
 * @param[in] path Path to the file to remove
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre path != NULL
 * @pre fsal_init() has been called successfully
 * @pre File is not currently open
 *
 * @note This function cannot remove directories. Use appropriate directory
 *       removal functions for directories.
 * @note After successful removal, the file no longer exists on the file system.
 *
 * @par Example:
 * @code
 * fsal_result_t result = fsal_remove("temp.txt");
 * if (result == FSAL_SUCCESS) {
 *     printf("File removed successfully\n");
 * } else if (result == FSAL_ERROR_NOT_FOUND) {
 *     printf("File does not exist\n");
 * }
 * @endcode
 */
fsal_result_t fsal_remove(const char* path);

/**
 * @brief Rename or move a file
 *
 * Renames a file or moves it to a different location. The file must not be open.
 *
 * @param[in] old_path Current path to the file
 * @param[in] new_path New path for the file
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre old_path != NULL
 * @pre new_path != NULL
 * @pre fsal_init() has been called successfully
 * @pre File at old_path is not currently open
 *
 * @note If new_path already exists, behavior is backend-specific:
 *       - Standard C: May overwrite existing file
 *       - FatFS: Returns error if file exists
 *       - Custom: Depends on implementation
 * @note Both paths must be on the same file system/volume for most backends.
 *
 * @par Example:
 * @code
 * fsal_result_t result = fsal_rename("old_name.txt", "new_name.txt");
 * if (result == FSAL_SUCCESS) {
 *     printf("File renamed successfully\n");
 * } else if (result == FSAL_ERROR_NOT_FOUND) {
 *     printf("Source file does not exist\n");
 * } else if (result == FSAL_ERROR_ALREADY_EXISTS) {
 *     printf("Destination file already exists\n");
 * }
 * @endcode
 */
fsal_result_t fsal_rename(const char* old_path, const char* new_path);

/**
 * @defgroup FSAL_Directory_Functions FSAL Directory Operations
 * @brief Directory listing and iteration functions
 * @{
 */

/**
 * @brief Open a directory for iteration
 *
 * Opens a directory and prepares it for sequential reading via fsal_readdir().
 *
 * @param[in] path Path to the directory to open
 * @param[out] dir_handle Pointer to directory handle structure to initialize
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre path != NULL
 * @pre dir_handle != NULL
 * @pre fsal_init() has been called successfully
 *
 * @note The handle must be closed with fsal_closedir() to free resources.
 *
 * @see fsal_readdir(), fsal_closedir()
 */
fsal_result_t fsal_opendir(const char* path, fsal_dir_handle_t* dir_handle);

/**
 * @brief Read the next entry from an open directory
 *
 * Reads the next directory entry. Returns FSAL_ERROR_NOT_FOUND when all
 * entries have been read (end of directory).
 *
 * @param[in] dir_handle Pointer to an open directory handle
 * @param[out] entry Pointer to directory entry structure to fill
 *
 * @return FSAL_SUCCESS if an entry was read, FSAL_ERROR_NOT_FOUND at end of directory,
 *         other error codes on failure
 *
 * @pre dir_handle != NULL && dir_handle->is_open == true
 * @pre entry != NULL
 *
 * @note Entries for "." and ".." are automatically skipped.
 *
 * @see fsal_opendir(), fsal_closedir()
 */
fsal_result_t fsal_readdir(fsal_dir_handle_t* dir_handle, fsal_dir_entry_t* entry);

/**
 * @brief Close a directory handle
 *
 * Closes a directory opened with fsal_opendir() and releases associated resources.
 *
 * @param[in,out] dir_handle Pointer to directory handle to close
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre dir_handle != NULL
 *
 * @note After calling this function, the handle should not be used for
 *       any other operations.
 *
 * @see fsal_opendir(), fsal_readdir()
 */
fsal_result_t fsal_closedir(fsal_dir_handle_t* dir_handle);

/** @} */ /* End of FSAL_Directory_Functions group */

/**
 * @brief Search for a byte pattern within a file
 *
 * Searches for the first occurrence of a byte pattern in a file.
 * The search can be performed forward (from beginning to end) or backward
 * (from end to beginning). The file pointer can optionally be positioned
 * at the found pattern.
 *
 * @param[in] handle Pointer to an open file handle
 * @param[in] pattern Pointer to the byte pattern to search for
 * @param[in] pattern_size Size of the pattern in bytes
 * @param[in] reverse_search If true, search from end to beginning; if false, from beginning to end
 * @param[out] found_position Position of the first byte of the found pattern (valid only if FSAL_SUCCESS)
 * @param[in] seek_to_match If true and pattern found, position file pointer at the match
 *
 * @return FSAL_SUCCESS if pattern found, FSAL_ERROR_NOT_FOUND if not found, other errors on failure
 *
 * @retval FSAL_SUCCESS Pattern found, position stored in found_position
 * @retval FSAL_ERROR_NOT_FOUND Pattern not found in file
 * @retval FSAL_ERROR_INVALID_PARAM Invalid handle, pattern, or pattern_size is 0
 * @retval FSAL_ERROR_IO I/O error during search
 *
 * @pre handle != NULL
 * @pre pattern != NULL
 * @pre pattern_size > 0
 * @pre found_position != NULL
 * @pre File handle is open
 *
 * @note The file pointer is preserved unless seek_to_match=true AND pattern is found.
 * @note Search always starts from absolute file positions (offset 0 or EOF), not current position.
 * @note This function invalidates any active read buffer (similar to fsal_seek).
 * @note For large files, this operation may take significant time.
 *
 * @warning If read buffering is enabled, it will be invalidated during the search.
 *
 * @par Behavior Summary:
 * | Scenario | Return | File Pointer |
 * |----------|--------|-------------|
 * | Found + seek_to_match=true | FSAL_SUCCESS | At first byte of pattern |
 * | Found + seek_to_match=false | FSAL_SUCCESS | Original position (unchanged) |
 * | Not found (any seek_to_match) | FSAL_ERROR_NOT_FOUND | Original position (unchanged) |
 *
 * @par Example - Search for magic number:
 * @code
 * fsal_handle_t file;
 * fsal_open("data.bin", FSAL_MODE_READ, &file);
 *
 * // Search for PNG magic number from beginning
 * uint8_t png_magic[] = {0x89, 0x50, 0x4E, 0x47};
 * long position;
 * fsal_result_t result = fsal_search(&file, png_magic, sizeof(png_magic),
 *                                     false, &position, true);
 *
 * if (result == FSAL_SUCCESS) {
 *     printf("PNG header found at offset %ld\n", position);
 *     // File pointer is now at the PNG header
 *     // Can read PNG data immediately
 * } else {
 *     printf("Not a valid PNG file\n");
 * }
 *
 * fsal_close(&file);
 * @endcode
 *
 * @par Example - Search for footer from end:
 * @code
 * // Search for "END" marker from end of file
 * const char* end_marker = "END";
 * long end_pos;
 * fsal_result_t result = fsal_search(&file, end_marker, 3, true, &end_pos, false);
 *
 * if (result == FSAL_SUCCESS) {
 *     printf("END marker found at offset %ld\n", end_pos);
 *     // File pointer unchanged, can continue reading from current position
 * }
 * @endcode
 *
 * @see fsal_read(), fsal_seek(), fsal_tell()
 */
fsal_result_t fsal_search(fsal_handle_t* handle, const void* pattern, size_t pattern_size, bool reverse_search,
						  long* found_position, bool seek_to_match);

#if FSAL_CFG_READ_BUFFERING_ENABLED
/**
 * @brief Enable read buffering for a file handle
 *
 * Activates read buffering with the specified mode and buffer size.
 * Buffering significantly improves performance for repeated small reads
 * by reducing the number of actual I/O operations to the underlying backend.
 *
 * @param[in,out] handle Pointer to an open file handle
 * @param[in] mode Buffering mode to use
 * @param[in] buffer_size Size of buffer to allocate (0 = use default)
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @retval FSAL_SUCCESS Buffering enabled successfully
 * @retval FSAL_ERROR_INVALID_PARAM handle is NULL or invalid
 * @retval FSAL_ERROR_MEMORY Failed to allocate buffer memory
 * @retval FSAL_ERROR_UNSUPPORTED Buffering not supported for this mode
 *
 * @pre handle != NULL
 * @pre File handle is open
 * @pre File was opened in read or read-write mode
 *
 * @note Buffering can be enabled at any time after opening a file.
 * @note If buffer_size is 0, FSAL_CFG_READ_BUFFER_SIZE is used.
 * @note Enabling buffering allocates memory that persists until file close.
 * @note Write-only files cannot use read buffering.
 *
 * @warning Calling this function when buffering is already enabled will
 *          invalidate the existing buffer and reallocate with new settings.
 *
 * @par Buffering Modes:
 * - **FSAL_BUFFER_NONE**: Disables buffering, frees buffer memory
 * - **FSAL_BUFFER_FULL**: Always performs read-ahead up to buffer capacity
 * - **FSAL_BUFFER_ADAPTIVE**: Adjusts read-ahead based on access pattern
 *
 * @par Example - Sequential Read Optimization:
 * @code
 * fsal_handle_t file;
 * fsal_open("large_file.bin", FSAL_MODE_READ, &file);
 *
 * // Enable adaptive buffering (detects sequential access)
 * fsal_enable_buffering(&file, FSAL_BUFFER_ADAPTIVE, 0);
 *
 * // Read small chunks - buffering optimizes this automatically
 * char data[64];
 * size_t bytes_read;
 * for (int i = 0; i < 1000; i++) {
 *     fsal_read(data, 1, 64, &file, &bytes_read);
 *     // Only the first read hits disk, subsequent reads use buffer
 * }
 *
 * fsal_close(&file); // Buffer is automatically freed
 * @endcode
 *
 * @par Example - Disable Buffering:
 * @code
 * // Temporarily disable buffering for random access
 * fsal_enable_buffering(&file, FSAL_BUFFER_NONE, 0);
 * fsal_seek(&file, random_offset, SEEK_SET);
 * fsal_read(data, 1, size, &file, &bytes_read);
 *
 * // Re-enable for sequential reads
 * fsal_enable_buffering(&file, FSAL_BUFFER_FULL, 8192);
 * @endcode
 *
 * @see fsal_read(), fsal_seek(), fsal_close()
 */
fsal_result_t fsal_enable_buffering(fsal_handle_t* handle, fsal_buffer_mode_t mode, size_t buffer_size);
#endif /* FSAL_CFG_READ_BUFFERING_ENABLED */

/**
 * @brief Get error string for result code
 *
 * Returns a human-readable description of an FSAL result code.
 *
 * @param[in] result FSAL result code
 *
 * @return Pointer to null-terminated error description string
 *
 * @note The returned pointer is valid for the lifetime of the application.
 * @note This function can be called without initialization.
 */
const char* fsal_get_error_string(fsal_result_t result);

#if FSAL_CFG_STATISTICS_ENABLED
/**
 * @brief Get file handle statistics
 *
 * Retrieves I/O statistics for a file handle (if statistics are enabled).
 *
 * @param[in] handle Pointer to file handle
 * @param[out] bytes_read Total bytes read from this file
 * @param[out] bytes_written Total bytes written to this file
 * @param[out] operation_count Total number of I/O operations
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre handle != NULL
 * @pre At least one output parameter != NULL
 *
 * @note This function is only available if FSAL_CFG_STATISTICS_ENABLED is 1
 */
fsal_result_t fsal_get_stats(fsal_handle_t* handle, size_t* bytes_read, size_t* bytes_written,
							 uint32_t* operation_count);

/**
 * @brief Reset file handle statistics
 *
 * Resets all statistics counters for a file handle to zero.
 *
 * @param[in] handle Pointer to file handle
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre handle != NULL
 *
 * @note This function is only available if FSAL_CFG_STATISTICS_ENABLED is 1
 */
fsal_result_t fsal_reset_stats(fsal_handle_t* handle);
#endif /* FSAL_CFG_STATISTICS_ENABLED */

#if (FSAL_CFG_BACKEND == 2)
/**
 * @brief Set custom backend interface
 *
 * Sets the function pointers for custom backend implementation.
 * This function is only available when FSAL_CFG_BACKEND is set to 2.
 *
 * @param[in] interface Pointer to custom interface structure
 *
 * @return FSAL_SUCCESS on success, appropriate error code on failure
 *
 * @pre interface != NULL
 * @pre All required function pointers in interface are non-NULL
 * @pre Must be called before fsal_init()
 *
 * @note This function is only available if FSAL_CFG_BACKEND is 2
 */
fsal_result_t fsal_set_custom_interface(const fsal_custom_interface_t* interface);
#endif /* FSAL_CFG_BACKEND == 2 */

/** @} */ /* End of FSAL_Utility_Functions group */

#ifdef __cplusplus
}
#endif

#endif /* AG_FSAL_H */

/**
 * @defgroup FSAL_Library File System Abstraction Layer (FSAL)
 * @brief Complete file system abstraction library for portable applications
 *
 * The File System Abstraction Layer (FSAL) provides a unified, high-performance
 * interface for file operations across multiple platforms and file systems:
 *
 * @par Supported Backends:
 * - **Standard C Library**: Full-featured desktop and server environments
 * - **FatFS**: Embedded systems with SD cards, eMMC, and flash memory
 * - **Custom**: User-defined implementations for specialized requirements
 *
 * @par Key Advantages:
 * - **Zero Overhead**: Compile-time backend selection with no runtime cost
 * - **Consistent API**: Same function calls work across all backends
 * - **Comprehensive Error Handling**: Detailed error codes and descriptions
 * - **Debug Support**: Optional statistics and diagnostic information
 * - **Thread Safety**: Configurable synchronization for multi-threaded use
 * - **Embedded Optimized**: Minimal memory footprint and stack usage
 *
 * @par Typical Usage Pattern:
 * 1. Configure backend in fsal_cfg.h
 * 2. Initialize library with fsal_init()
 * 3. Open files with fsal_open()
 * 4. Perform I/O with fsal_read()/fsal_write()
 * 5. Close files with fsal_close()
 * 6. Cleanup with fsal_deinit()
 *
 * @par Performance Considerations:
 * - Use appropriate buffer sizes for your application
 * - Consider enabling read-ahead/write-behind for sequential access
 * - Disable debug features in production builds
 * - Tune memory limits based on available RAM
 *
 * @par Error Handling Best Practices:
 * - Always check return codes from FSAL functions
 * - Use fsal_get_error_string() for user-friendly error messages
 * - Implement proper cleanup in error paths
 * - Consider retry logic for transient errors
 *
 * @warning Always call fsal_init() before using other FSAL functions
 * @warning Ensure proper cleanup with fsal_deinit() before program exit
 */