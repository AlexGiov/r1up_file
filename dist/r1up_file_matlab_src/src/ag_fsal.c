/**
 * @file fsal.c
 * @brief File System Abstraction Layer (FSAL) - Implementation
 *
 * This file contains the implementation of the File System Abstraction Layer.
 * It provides a unified interface to different file system backends, enabling
 * seamless portability between desktop and embedded environments.
 *
 * @author Alex Giov
 * @date September 27, 2025
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2025, Race1Up Project.
 * All rights reserved.
 *
 * @par Implementation Notes:
 * - Backend selection is handled at compile time for zero runtime overhead
 * - Error handling provides consistent error codes across all backends
 * - Memory management is optimized for embedded systems
 * - Thread safety is configurable through compile-time options
 *
 * @par Backend Support:
 * - Standard C Library: Full POSIX-compliant implementation
 * - FatFS: Optimized for embedded systems with minimal overhead
 * - Custom: Extensible framework for user-defined implementations
 */

/*============================================================================*/
/* INCLUDES                                                                   */
/*============================================================================*/

#include "ag_fsal/ag_fsal.h"

#include <assert.h>
#include <string.h>

#include "ag_fsal_cfg.h"

/* Memory backend include */
#if (FSAL_CFG_MEMORY_BACKEND == 1)
#include "ag_mal/ag_mal.h"
#else
#include <stdlib.h> /* For malloc/free */
#endif

/* Conditional includes based on configured backend */
#if (FSAL_CFG_BACKEND == 0)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#elif (FSAL_CFG_BACKEND == 1)
#include "ff.h"
#endif

#if FSAL_CFG_THREAD_SAFE_ENABLED
#if (FSAL_CFG_MEMORY_BACKEND == 1) /* FreeRTOS backend -> use FreeRTOS mutex */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#define FSAL_MUTEX_TYPE xSemaphoreHandle
#define FSAL_MUTEX_CREATE() xSemaphoreCreateMutex()
#define FSAL_MUTEX_DELETE(m) vSemaphoreDelete(m)
#define FSAL_MUTEX_TAKE(m) xSemaphoreTake((m), portMAX_DELAY)
#define FSAL_MUTEX_GIVE(m) xSemaphoreGive(m)
#else /* Standard C / POSIX fallback (stub -- extend with pthread if needed) */
#define FSAL_MUTEX_TYPE int
#define FSAL_MUTEX_CREATE() (0)
#define FSAL_MUTEX_DELETE(m) ((void)(m))
#define FSAL_MUTEX_TAKE(m) ((void)(m))
#define FSAL_MUTEX_GIVE(m) ((void)(m))
#endif
#else /* !FSAL_CFG_THREAD_SAFE_ENABLED — taskENTER/EXIT_CRITICAL not available */
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif

/*============================================================================*/
/* PRIVATE CONSTANTS AND MACROS                                              */
/*============================================================================*/

/**
 * @defgroup FSAL_Private_Constants Private Constants
 * @brief Internal constants used by the implementation
 * @{
 */

/** @brief Handle validation magic number */
#define FSAL_HANDLE_MAGIC 0x46534C48U /* "FSLH" in hex */

/** @brief Invalid handle ID */
#define FSAL_INVALID_HANDLE_ID 0U

#if FSAL_CFG_DEBUG_ENABLED
/** @brief Debug output prefix */
#define FSAL_DEBUG_PREFIX "[FSAL] "
#endif

/*
 * Internal memory abstraction macros.
 * Route malloc/free to either standard C library or ag_mal,
 * depending on FSAL_CFG_MEMORY_BACKEND. Always use these macros
 * inside the FSAL implementation instead of calling malloc/free directly.
 */
#if (FSAL_CFG_MEMORY_BACKEND == 1)
#define FSAL_MALLOC(size) ag_mal_alloc(size)
#if FSAL_CFG_DEBUG_ENABLED
#define FSAL_FREE(ptr)                             \
	do {                                           \
		ag_mal_result_t _mal_r = ag_mal_free(ptr); \
		assert(_mal_r == AG_MAL_OK);               \
		(void)_mal_r;                              \
	} while (0)
#else
#define FSAL_FREE(ptr) (void)ag_mal_free(ptr)
#endif
#else
#define FSAL_MALLOC(size) malloc(size)
#define FSAL_FREE(ptr) free(ptr)
#endif /* FSAL_CFG_MEMORY_BACKEND */

/** @} */ /* End of FSAL_Private_Constants group */

/*============================================================================*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*============================================================================*/

/**
 * @defgroup FSAL_Private_Types Private Types
 * @brief Internal type definitions
 * @{
 */

/**
 * @brief Library state structure
 *
 * Contains global state information for the FSAL library.
 */
typedef struct {
	bool is_initialized;	   /**< Library initialization status */
	uint32_t init_count;	   /**< Reference count for nested init/deinit */
	uint32_t next_handle_id;   /**< Next handle ID to assign */
	uint32_t open_files_count; /**< Number of currently open files */

#if FSAL_CFG_THREAD_SAFE_ENABLED
	FSAL_MUTEX_TYPE mutex; /**< Protects library state (open_files_count, handle IDs) */
#endif

#if FSAL_CFG_STATISTICS_ENABLED
	struct {
		uint64_t total_bytes_read;	  /**< Total bytes read across all files */
		uint64_t total_bytes_written; /**< Total bytes written across all files */
		uint32_t total_opens;		  /**< Total number of file opens */
		uint32_t total_closes;		  /**< Total number of file closes */
		uint32_t current_open_files;  /**< Currently open files */
		uint32_t peak_open_files;	  /**< Peak number of open files */
	} global_stats;
#endif
} fsal_library_state_t;

/** @} */ /* End of FSAL_Private_Types group */

/*============================================================================*/
/* PRIVATE VARIABLES                                                          */
/*============================================================================*/

/**
 * @defgroup FSAL_Private_Variables Private Variables
 * @brief Internal module variables
 * @{
 */

/** @brief Global library state */
static fsal_library_state_t s_fsal_state = {.is_initialized = false, .next_handle_id = 1U, .open_files_count = 0U};

/** @brief Library version string */
static const char s_version_string[] = FSAL_VERSION_STRING;

/** @brief Error code to string mapping */
static const char* const s_error_strings[] = {[FSAL_SUCCESS] = "Success",
											  [FSAL_ERROR_INVALID_PARAM] = "Invalid parameter",
											  [FSAL_ERROR_NOT_FOUND] = "File not found",
											  [FSAL_ERROR_ACCESS_DENIED] = "Access denied",
											  [FSAL_ERROR_NO_SPACE] = "No space left on device",
											  [FSAL_ERROR_IO] = "I/O error",
											  [FSAL_ERROR_MEMORY] = "Memory allocation failed",
											  [FSAL_ERROR_CORRUPTED] = "File or filesystem corrupted",
											  [FSAL_ERROR_UNSUPPORTED] = "Operation not supported",
											  [FSAL_ERROR_TIMEOUT] = "Operation timed out",
											  [FSAL_ERROR_BUSY] = "Resource busy",
											  [FSAL_ERROR_NOT_INITIALIZED] = "Library not initialized",
											  [FSAL_ERROR_ALREADY_EXISTS] = "File already exists",
											  [FSAL_ERROR_TOO_MANY_OPEN] = "Too many open files"};

#if (FSAL_CFG_BACKEND == 2)
/** @brief Custom backend interface (for custom implementations) */
static const fsal_custom_interface_t* s_custom_interface = NULL;
#endif

/** @} */ /* End of FSAL_Private_Variables group */

/*============================================================================*/
/* PRIVATE FUNCTION DECLARATIONS                                              */
/*============================================================================*/

/**
 * @defgroup FSAL_Private_Functions Private Functions
 * @brief Internal helper functions
 * @{
 */

/**
 * @brief Convert FSAL mode to backend-specific mode string
 *
 * @param mode FSAL file mode
 * @return Backend-specific mode string, or NULL if invalid
 */
static const char* fsal_mode_to_string(fsal_mode_t mode);

/**
 * @brief Initialize a file handle structure
 *
 * @param handle Pointer to handle structure to initialize
 * @param mode File access mode
 */
static void fsal_init_handle(fsal_handle_t* handle, fsal_mode_t mode);

/**
 * @brief Validate a file handle
 *
 * @param handle Pointer to handle structure to validate
 * @return true if handle is valid, false otherwise
 */
static bool fsal_is_handle_valid(const fsal_handle_t* handle);

#if (FSAL_CFG_BACKEND == 1)
/**
 * @brief Convert FSAL mode to FatFS mode flags
 *
 * @param mode FSAL file mode
 * @return FatFS mode flags
 */
static BYTE fsal_mode_to_fatfs(fsal_mode_t mode);

/**
 * @brief Convert FatFS result code to FSAL result
 *
 * @param fresult FatFS result code
 * @return Corresponding FSAL result code
 */
static fsal_result_t fsal_fatfs_to_result(FRESULT fresult);
#endif

#if (FSAL_CFG_BACKEND == 0)
/**
 * @brief Convert errno to FSAL result code
 *
 * @param error_code errno value
 * @return Corresponding FSAL result code
 */
static fsal_result_t fsal_errno_to_result(int error_code);
#endif

#if FSAL_CFG_STATISTICS_ENABLED
/**
 * @brief Update handle statistics
 *
 * @param handle Pointer to handle structure
 * @param bytes_read Number of bytes read (0 if not a read operation)
 * @param bytes_written Number of bytes written (0 if not a write operation)
 * @param is_read_op True if this is a read operation
 * @param is_write_op True if this is a write operation
 */
static void fsal_update_stats(fsal_handle_t* handle, size_t bytes_read, size_t bytes_written, bool is_read_op,
							  bool is_write_op);
#endif

#if FSAL_CFG_READ_BUFFERING_ENABLED
/**
 * @brief Check if requested data is available in buffer
 */
static bool fsal_is_data_in_buffer(const fsal_handle_t* handle, long file_offset, size_t size);

/**
 * @brief Copy data from buffer to user buffer
 */
static size_t fsal_copy_from_buffer(fsal_handle_t* handle, void* dest, long file_offset, size_t size);

/**
 * @brief Fill buffer by reading from underlying backend
 */
static fsal_result_t fsal_fill_buffer(fsal_handle_t* handle, long file_offset, size_t read_size);

/**
 * @brief Invalidate buffer contents
 */
static void fsal_invalidate_buffer(fsal_handle_t* handle);

/**
 * @brief Update access pattern statistics for adaptive buffering
 */
static void fsal_update_access_pattern(fsal_handle_t* handle, long current_offset);

/**
 * @brief Calculate optimal read-ahead size based on access pattern
 */
static size_t fsal_calculate_readahead_size(const fsal_handle_t* handle, size_t requested_size);

/**
 * @brief Perform direct read from backend (bypassing buffer)
 */
static fsal_result_t fsal_read_direct(void* buffer, size_t size, size_t count, fsal_handle_t* handle,
									  size_t* bytes_read);
#endif /* FSAL_CFG_READ_BUFFERING_ENABLED */

/** @} */ /* End of FSAL_Private_Functions group */

/*============================================================================*/
/* PUBLIC FUNCTION IMPLEMENTATIONS                                            */
/*============================================================================*/

fsal_result_t fsal_init(void) {
	/* Reference-counted: subsequent calls just increment and return.
	 * Use critical section so the read-modify-write is atomic
	 * (the FSAL mutex may not exist yet on the very first call). */
	taskENTER_CRITICAL();
	if (s_fsal_state.is_initialized) {
		s_fsal_state.init_count++;
		taskEXIT_CRITICAL();
		return FSAL_SUCCESS;
	}
	taskEXIT_CRITICAL();

#if (FSAL_CFG_MEMORY_BACKEND == 1)
	/* Initialize Memory Abstraction Layer */
	if (ag_mal_init() != AG_MAL_OK) {
		return FSAL_ERROR_MEMORY;
	}
#endif

#if FSAL_CFG_THREAD_SAFE_ENABLED
	/* Initialize thread synchronization (FreeRTOS mutex) */
	s_fsal_state.mutex = FSAL_MUTEX_CREATE();
	if (s_fsal_state.mutex == NULL) {
		return FSAL_ERROR_MEMORY;
	}
#endif

#if (FSAL_CFG_BACKEND == 1)
	/* FatFS-specific initialization */
	/* Note: Actual FatFS mounting should be done by the application */
	/* before calling fsal_init() */
#elif (FSAL_CFG_BACKEND == 2)
	/* Custom backend validation */
	if (s_custom_interface == NULL) {
		return FSAL_ERROR_NOT_INITIALIZED;
	}
	/* Validate that all required function pointers are set */
	if (s_custom_interface->open == NULL || s_custom_interface->close == NULL || s_custom_interface->read == NULL ||
		s_custom_interface->write == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Initialize library state */
	s_fsal_state.next_handle_id = 1U;
	s_fsal_state.open_files_count = 0U;

#if FSAL_CFG_STATISTICS_ENABLED
	/* Initialize global statistics */
	memset(&s_fsal_state.global_stats, 0, sizeof(s_fsal_state.global_stats));
#endif

	/* Mark as initialized with reference count = 1 */
	s_fsal_state.init_count = 1U;
	s_fsal_state.is_initialized = true;

#if FSAL_CFG_DEBUG_ENABLED
	/* Debug output if enabled */
	/* Add your debug logging here */
#endif

	return FSAL_SUCCESS;
}

fsal_result_t fsal_deinit(void) {
	/* Use critical section for the ref-count check/update */
	taskENTER_CRITICAL();
	if (!s_fsal_state.is_initialized) {
		taskEXIT_CRITICAL();
		return FSAL_SUCCESS;
	}

	if (s_fsal_state.init_count > 1U) {
		s_fsal_state.init_count--;
		taskEXIT_CRITICAL();
		return FSAL_SUCCESS;
	}
	/* Last user — mark uninitialised inside the critical section
	 * so no concurrent caller can slip in. */
	s_fsal_state.is_initialized = false;
	s_fsal_state.init_count = 0U;
	taskEXIT_CRITICAL();

	/* Last user — full cleanup */

	/* Warn if files are still open */
	if (s_fsal_state.open_files_count > 0) {
#if FSAL_CFG_DEBUG_ENABLED
		/* Log warning about open files */
#endif
		/* In debug mode, we might want to return an error */
#if FSAL_CFG_DEBUG_ENABLED
		return FSAL_ERROR_BUSY;
#endif
	}

#if FSAL_CFG_THREAD_SAFE_ENABLED
	if (s_fsal_state.mutex != NULL) {
		FSAL_MUTEX_DELETE(s_fsal_state.mutex);
		s_fsal_state.mutex = NULL;
	}
#endif

	/* (is_initialized and init_count already cleared in the
	 *  critical section above.) */

#if (FSAL_CFG_MEMORY_BACKEND == 1)
	/* Deinitialize Memory Abstraction Layer */
	ag_mal_deinit();
#endif

	return FSAL_SUCCESS;
}

const char* fsal_get_version(void) { return s_version_string; }

uint8_t fsal_get_backend(void) { return (uint8_t)FSAL_CFG_BACKEND; }

fsal_result_t fsal_open(const char* path, fsal_mode_t mode, fsal_handle_t* handle) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (path == NULL || handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
	if (mode < FSAL_MODE_READ || mode > FSAL_MODE_CREATE_NEW) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Check if library is initialized */
	if (!s_fsal_state.is_initialized) {
		return FSAL_ERROR_NOT_INITIALIZED;
	}

	/* Check if we've reached the maximum number of open files */
	if (s_fsal_state.open_files_count >= FSAL_CFG_MAX_OPEN_FILES) {
		return FSAL_ERROR_TOO_MANY_OPEN;
	}

	/* Initialize handle structure */
	fsal_init_handle(handle, mode);

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (mode == FSAL_MODE_CREATE_NEW) {
		/* Atomic create: fail if file already exists. */
		int fd = -1;
#ifdef _WIN32
		fd = _open(path, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
		fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0666);
#endif
		if (fd < 0) {
			result = fsal_errno_to_result(errno);
		} else {
			/* Convert file descriptor to FILE* so rest of FSAL can stay unchanged. */

#ifdef _WIN32
			handle->native_handle = _fdopen(fd, "wb");
#else
			handle->native_handle = fdopen(fd, "wb");
#endif
			if (handle->native_handle == NULL) {
#ifdef _WIN32
				_close(fd);
#else
				close(fd);
#endif
				result = fsal_errno_to_result(errno);
			}
		}
	} else {
		const char* mode_str = fsal_mode_to_string(mode);
		if (mode_str == NULL) {
			return FSAL_ERROR_INVALID_PARAM;
		}

		handle->native_handle = fopen(path, mode_str);
		if (handle->native_handle == NULL) {
			result = fsal_errno_to_result(errno);
		}
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	BYTE fatfs_mode = fsal_mode_to_fatfs(mode);
	if (fatfs_mode == 0) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	FRESULT fresult = f_open(&handle->native_handle, path, fatfs_mode);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	result = s_custom_interface->open(path, mode, &handle->native_handle);

#else
#error "Invalid FSAL_CFG_BACKEND value"
#endif

	if (result == FSAL_SUCCESS) {
		handle->is_open = true;
#if FSAL_CFG_THREAD_SAFE_ENABLED
		FSAL_MUTEX_TAKE(s_fsal_state.mutex);
#endif
		handle->handle_id = s_fsal_state.next_handle_id++;
		s_fsal_state.open_files_count++;

#if FSAL_CFG_STATISTICS_ENABLED
		s_fsal_state.global_stats.total_opens++;
		s_fsal_state.global_stats.current_open_files++;
		if (s_fsal_state.global_stats.current_open_files > s_fsal_state.global_stats.peak_open_files) {
			s_fsal_state.global_stats.peak_open_files = s_fsal_state.global_stats.current_open_files;
		}
#endif
#if FSAL_CFG_THREAD_SAFE_ENABLED
		FSAL_MUTEX_GIVE(s_fsal_state.mutex);
#endif

#if FSAL_CFG_DEBUG_ENABLED
		/* Store truncated path for debugging */
		strncpy(handle->debug_path, path, sizeof(handle->debug_path) - 1);
		handle->debug_path[sizeof(handle->debug_path) - 1] = '\0';
		/* Set open timestamp */
		/* handle->open_timestamp = get_current_time_ms(); */
#endif
	}

	return result;
}

fsal_result_t fsal_close(fsal_handle_t* handle) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (fclose(handle->native_handle) != 0) {
		result = fsal_errno_to_result(errno);
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FRESULT fresult = f_close(&handle->native_handle);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	result = s_custom_interface->close(handle->native_handle);
#endif

	if (result == FSAL_SUCCESS) {
		handle->is_open = false;
		handle->handle_id = FSAL_INVALID_HANDLE_ID;
#if FSAL_CFG_THREAD_SAFE_ENABLED
		FSAL_MUTEX_TAKE(s_fsal_state.mutex);
#endif
		s_fsal_state.open_files_count--;

#if FSAL_CFG_READ_BUFFERING_ENABLED
		/* Free buffer memory if allocated */
		if (handle->buffer.data != NULL) {
			FSAL_FREE(handle->buffer.data);
			handle->buffer.data = NULL;
			handle->buffer.capacity = 0;
			handle->buffer.valid_bytes = 0;
			handle->buffer.enabled = false;
		}
#endif

#if FSAL_CFG_STATISTICS_ENABLED
		s_fsal_state.global_stats.total_closes++;
		s_fsal_state.global_stats.current_open_files--;
#endif
#if FSAL_CFG_THREAD_SAFE_ENABLED
		FSAL_MUTEX_GIVE(s_fsal_state.mutex);
#endif
	}

	return result;
}

fsal_result_t fsal_read(void* buffer, size_t size, size_t count, fsal_handle_t* handle, size_t* bytes_read) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (buffer == NULL || handle == NULL || size == 0) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	size_t total_bytes = size * count;
	size_t total_read = 0;
	fsal_result_t result = FSAL_SUCCESS;

#if FSAL_CFG_READ_BUFFERING_ENABLED
	/* Use buffered read if enabled */
	if (handle->buffer.enabled && handle->buffer.mode != FSAL_BUFFER_NONE) {
		/* Get current file position */
		long current_pos = -1;

#if (FSAL_CFG_BACKEND == 0)
		current_pos = ftell(handle->native_handle);
#elif (FSAL_CFG_BACKEND == 1)
		current_pos = f_tell(&handle->native_handle);
#endif

#if FSAL_CFG_DEBUG_ENABLED
		printf("[FSAL_READ] Buffered read: total_bytes=%lu, current_pos=%ld\n", (unsigned long)total_bytes,
			   current_pos);
#endif

		if (current_pos < 0) {
			/* Position unknown - fall back to direct read */
#if FSAL_CFG_DEBUG_ENABLED
			printf("[FSAL_READ] Position unknown, falling back to direct read\n");
#endif
			result = fsal_read_direct(buffer, size, count, handle, &total_read);
			goto read_complete;
		}

		/* Update access pattern statistics */
		fsal_update_access_pattern(handle, current_pos);

		/* Try to read from buffer first */
		uint8_t* dest = (uint8_t*)buffer;

		while (total_read < total_bytes) {
			long read_offset = current_pos + total_read;
			size_t remaining = total_bytes - total_read;

			/* Check if data is in buffer */
			if (fsal_is_data_in_buffer(handle, read_offset, remaining)) {
				/* Buffer hit - copy from buffer */
				size_t copied = fsal_copy_from_buffer(handle, dest + total_read, read_offset, remaining);
				total_read += copied;
			} else {
				/* Buffer miss - need to fill buffer */
#if FSAL_CFG_STATISTICS_ENABLED
				handle->stats.buffer_misses++;
#endif

				/* Calculate read-ahead size */
				size_t readahead_size = fsal_calculate_readahead_size(handle, remaining);

				/* Fill buffer from current position */
				result = fsal_fill_buffer(handle, read_offset, readahead_size);

				if (result != FSAL_SUCCESS || handle->buffer.valid_bytes == 0) {
					/* Buffer fill failed or EOF - try direct read for remaining bytes */
					size_t direct_read = 0;
					result = fsal_read_direct(dest + total_read, 1, remaining, handle, &direct_read);
					total_read += direct_read;
					break;
				}

				/* Now copy from freshly filled buffer */
				size_t copied = fsal_copy_from_buffer(handle, dest + total_read, read_offset, remaining);

				if (copied == 0) {
					/* No data copied - likely EOF */
					break;
				}

				total_read += copied;
			}
		}

	read_complete:
		if (bytes_read != NULL) {
			*bytes_read = total_read;
		}

		/* Update file position to match what user has actually read */
		if (total_read > 0) {
#if (FSAL_CFG_BACKEND == 0)
			fseek(handle->native_handle, current_pos + total_read, SEEK_SET);
#elif (FSAL_CFG_BACKEND == 1)
			f_lseek(&handle->native_handle, current_pos + total_read);
#endif
		}

#if FSAL_CFG_STATISTICS_ENABLED
		if (result == FSAL_SUCCESS || total_read > 0) {
			fsal_update_stats(handle, total_read, 0, true, false);
		}
#endif

		return (total_read > 0) ? FSAL_SUCCESS : result;
	}
#endif /* FSAL_CFG_READ_BUFFERING_ENABLED */

	/* Direct read (no buffering or buffering disabled) */
	size_t actual_read = 0;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	actual_read = fread(buffer, 1, total_bytes, handle->native_handle);
	if (actual_read != total_bytes && ferror(handle->native_handle)) {
		result = FSAL_ERROR_IO;
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	UINT fatfs_read = 0;
	FRESULT fresult = f_read(&handle->native_handle, buffer, total_bytes, &fatfs_read);
	actual_read = fatfs_read;
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	result = s_custom_interface->read(handle->native_handle, buffer, size, count, &actual_read);
#endif

	if (bytes_read != NULL) {
		*bytes_read = actual_read;
	}

#if FSAL_CFG_STATISTICS_ENABLED
	if (result == FSAL_SUCCESS) {
		fsal_update_stats(handle, actual_read, 0, true, false);
	}
#endif

	return result;
}

fsal_result_t fsal_write(const void* buffer, size_t size, size_t count, fsal_handle_t* handle, size_t* bytes_written) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (buffer == NULL || handle == NULL || size == 0) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

#if FSAL_CFG_READ_BUFFERING_ENABLED
	/* Invalidate read buffer on write - data may be stale */
	fsal_invalidate_buffer(handle);
#endif

	size_t total_bytes = size * count;
	size_t actual_written = 0;
	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	actual_written = fwrite(buffer, 1, total_bytes, handle->native_handle);
	if (actual_written != total_bytes) {
		result = FSAL_ERROR_IO;
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	UINT fatfs_written = 0;
	FRESULT fresult = f_write(&handle->native_handle, buffer, total_bytes, &fatfs_written);
	actual_written = fatfs_written;
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	result = s_custom_interface->write(handle->native_handle, buffer, size, count, &actual_written);
#endif

	if (bytes_written != NULL) {
		*bytes_written = actual_written;
	}

#if FSAL_CFG_STATISTICS_ENABLED
	if (result == FSAL_SUCCESS) {
		fsal_update_stats(handle, 0, actual_written, false, true);
	}
#endif

	return result;
}

fsal_result_t fsal_seek(fsal_handle_t* handle, long offset, int origin) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
	if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (fseek(handle->native_handle, offset, origin) != 0) {
		result = fsal_errno_to_result(errno);
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FSIZE_t new_pos;
	switch (origin) {
		case SEEK_SET:
			new_pos = offset;
			break;
		case SEEK_CUR:
			new_pos = f_tell(&handle->native_handle) + offset;
			break;
		case SEEK_END:
			new_pos = f_size(&handle->native_handle) + offset;
			break;
		default:
			return FSAL_ERROR_INVALID_PARAM;
	}

	FRESULT fresult = f_lseek(&handle->native_handle, new_pos);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	result = s_custom_interface->seek(handle->native_handle, offset, origin);
#endif

#if FSAL_CFG_READ_BUFFERING_ENABLED
	/* Invalidate buffer after seek - position may have changed */
	if (result == FSAL_SUCCESS) {
		fsal_invalidate_buffer(handle);
	}
#endif

#if FSAL_CFG_STATISTICS_ENABLED
	if (result == FSAL_SUCCESS) {
		handle->stats.seek_ops++;
	}
#endif

	return result;
}

fsal_result_t fsal_tell(fsal_handle_t* handle, long* position) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (handle == NULL || position == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	long pos = ftell(handle->native_handle);
	if (pos == -1L) {
		result = fsal_errno_to_result(errno);
	} else {
		*position = pos;
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	*position = (long)f_tell(&handle->native_handle);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	result = s_custom_interface->tell(handle->native_handle, position);
#endif

	return result;
}

fsal_result_t fsal_flush(fsal_handle_t* handle) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (fflush(handle->native_handle) != 0) {
		result = fsal_errno_to_result(errno);
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FRESULT fresult = f_sync(&handle->native_handle);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	if (s_custom_interface->flush != NULL) {
		result = s_custom_interface->flush(handle->native_handle);
	}
	/* If flush is not implemented in custom backend, treat as success */
#endif

	return result;
}

const char* fsal_get_error_string(fsal_result_t result) {
	if (result < 0 || result >= (sizeof(s_error_strings) / sizeof(s_error_strings[0]))) {
		return "Unknown error";
	}
	return s_error_strings[result];
}

fsal_result_t fsal_stat(const char* path, fsal_file_info_t* info) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (path == NULL || info == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Check if library is initialized */
	if (!s_fsal_state.is_initialized) {
		return FSAL_ERROR_NOT_INITIALIZED;
	}

	/* Initialize info structure */
	memset(info, 0, sizeof(fsal_file_info_t));

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	struct stat file_stat;
	if (stat(path, &file_stat) == 0) {
		info->exists = true;
		info->size = (size_t)file_stat.st_size;
		info->creation_time = (uint32_t)file_stat.st_ctime;
		info->modification_time = (uint32_t)file_stat.st_mtime;
		info->is_directory = (file_stat.st_mode & S_IFDIR) != 0;
		info->is_readonly = (file_stat.st_mode & S_IWRITE) == 0;
	} else {
		info->exists = false;
		result = fsal_errno_to_result(errno);
		/* If file doesn't exist, that's not necessarily an error for stat */
		if (result == FSAL_ERROR_NOT_FOUND) {
			result = FSAL_SUCCESS;
		}
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FILINFO fno;
	FRESULT fresult = f_stat(path, &fno);

	if (fresult == FR_OK) {
		info->exists = true;
		info->size = (size_t)fno.fsize;
		/* FatFS date/time conversion (simplified) */
		info->modification_time = fno.fdate | (fno.ftime << 16);
		info->creation_time = info->modification_time; /* FatFS doesn't separate creation time */
		info->is_directory = (fno.fattrib & AM_DIR) != 0;
		info->is_readonly = (fno.fattrib & AM_RDO) != 0;
	} else {
		info->exists = false;
		result = fsal_fatfs_to_result(fresult);
		/* If file doesn't exist, that's not necessarily an error for stat */
		if (result == FSAL_ERROR_NOT_FOUND) {
			result = FSAL_SUCCESS;
		}
	}

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	if (s_custom_interface->stat != NULL) {
		result = s_custom_interface->stat(path, info);
	} else {
		/* If stat is not implemented in custom backend, try to open the file */
		/* This is a fallback that only checks existence */
		void* handle = NULL;
		result = s_custom_interface->open(path, FSAL_MODE_READ, &handle);
		if (result == FSAL_SUCCESS) {
			info->exists = true;
			s_custom_interface->close(handle);
		} else if (result == FSAL_ERROR_NOT_FOUND) {
			info->exists = false;
			result = FSAL_SUCCESS;
		}
	}
#endif

	return result;
}

fsal_result_t fsal_exists(const char* path, bool* exists) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (path == NULL || exists == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Use fsal_stat to check existence */
	fsal_file_info_t info;
	fsal_result_t result = fsal_stat(path, &info);

	if (result == FSAL_SUCCESS) {
		*exists = info.exists;
	}

	return result;
}

fsal_result_t fsal_remove(const char* path) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (path == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Check if library is initialized */
	if (!s_fsal_state.is_initialized) {
		return FSAL_ERROR_NOT_INITIALIZED;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (remove(path) != 0) {
		result = fsal_errno_to_result(errno);
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FRESULT fresult = f_unlink(path);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	/* Custom backend would need to implement a remove function */
	/* For now, we return unsupported if not available */
	result = FSAL_ERROR_UNSUPPORTED;
#endif

	return result;
}

fsal_result_t fsal_rename(const char* old_path, const char* new_path) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (old_path == NULL || new_path == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Check if library is initialized */
	if (!s_fsal_state.is_initialized) {
		return FSAL_ERROR_NOT_INITIALIZED;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (rename(old_path, new_path) != 0) {
		result = fsal_errno_to_result(errno);
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FRESULT fresult = f_rename(old_path, new_path);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation */
	/* Custom backend would need to implement a rename function */
	/* For now, we return unsupported if not available */
	result = FSAL_ERROR_UNSUPPORTED;
#endif

	return result;
}

fsal_result_t fsal_search(fsal_handle_t* handle, const void* pattern, size_t pattern_size, bool reverse_search,
						  long* found_position, bool seek_to_match) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (handle == NULL || pattern == NULL || found_position == NULL || pattern_size == 0) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	/* Save current file position to restore later if needed */
	long original_pos = -1;

#if (FSAL_CFG_BACKEND == 0)
	original_pos = ftell(handle->native_handle);
#elif (FSAL_CFG_BACKEND == 1)
	original_pos = (long)f_tell(&handle->native_handle);
#elif (FSAL_CFG_BACKEND == 2)
	fsal_result_t tell_result = s_custom_interface->tell(handle->native_handle, &original_pos);
	if (tell_result != FSAL_SUCCESS) {
		return tell_result;
	}
#endif

	if (original_pos < 0) {
		return FSAL_ERROR_IO;
	}

	/* Get file size */
	long file_size = 0;

#if (FSAL_CFG_BACKEND == 0)
	if (fseek(handle->native_handle, 0, SEEK_END) != 0) {
		return FSAL_ERROR_IO;
	}
	file_size = ftell(handle->native_handle);
	if (file_size < 0) {
		fseek(handle->native_handle, original_pos, SEEK_SET);
		return FSAL_ERROR_IO;
	}
#elif (FSAL_CFG_BACKEND == 1)
	file_size = (long)f_size(&handle->native_handle);
#elif (FSAL_CFG_BACKEND == 2)
	/* Custom backend - try to seek to end and tell */
	if (s_custom_interface->seek != NULL && s_custom_interface->tell != NULL) {
		fsal_result_t seek_result = s_custom_interface->seek(handle->native_handle, 0, SEEK_END);
		if (seek_result != FSAL_SUCCESS) {
			return FSAL_ERROR_IO;
		}
		long end_pos;
		fsal_result_t tell_result = s_custom_interface->tell(handle->native_handle, &end_pos);
		if (tell_result != FSAL_SUCCESS) {
			return FSAL_ERROR_IO;
		}
		file_size = end_pos;
		/* Restore position */
		s_custom_interface->seek(handle->native_handle, original_pos, SEEK_SET);
	} else {
		return FSAL_ERROR_UNSUPPORTED;
	}
#endif

	/* Check if pattern can possibly exist in file */
	if ((long)pattern_size > file_size) {
		/* Restore position and return not found */
#if (FSAL_CFG_BACKEND == 0)
		fseek(handle->native_handle, original_pos, SEEK_SET);
#elif (FSAL_CFG_BACKEND == 1)
		f_lseek(&handle->native_handle, original_pos);
#endif
		return FSAL_ERROR_NOT_FOUND;
	}

	/* Allocate search buffer */
	size_t search_buffer_size = 4096; /* Use reasonable buffer size */
	if (search_buffer_size < pattern_size * 2) {
		search_buffer_size = pattern_size * 2;
	}

	uint8_t* search_buffer = (uint8_t*)FSAL_MALLOC(search_buffer_size);
	if (search_buffer == NULL) {
		/* Restore position and return error */
#if (FSAL_CFG_BACKEND == 0)
		fseek(handle->native_handle, original_pos, SEEK_SET);
#elif (FSAL_CFG_BACKEND == 1)
		f_lseek(&handle->native_handle, original_pos);
#endif
		return FSAL_ERROR_MEMORY;
	}

	const uint8_t* pattern_bytes = (const uint8_t*)pattern;
	fsal_result_t search_result = FSAL_ERROR_NOT_FOUND;
	long match_position = -1;

	if (!reverse_search) {
		/* Forward search: from beginning to end */
		long current_offset = 0;
		size_t overlap = pattern_size - 1;

		while (current_offset < file_size) {
			/* Calculate how many bytes to read */
			size_t bytes_to_read = search_buffer_size;
			if (current_offset + (long)bytes_to_read > file_size) {
				bytes_to_read = (size_t)(file_size - current_offset);
			}

			/* Read chunk using direct read to avoid buffer conflicts */
			size_t bytes_read = 0;

#if (FSAL_CFG_BACKEND == 0)
			if (fseek(handle->native_handle, current_offset, SEEK_SET) != 0) {
				FSAL_FREE(search_buffer);
				fseek(handle->native_handle, original_pos, SEEK_SET);
				return FSAL_ERROR_IO;
			}
			bytes_read = fread(search_buffer, 1, bytes_to_read, handle->native_handle);
			if (bytes_read == 0 && ferror(handle->native_handle)) {
				FSAL_FREE(search_buffer);
				fseek(handle->native_handle, original_pos, SEEK_SET);
				return FSAL_ERROR_IO;
			}
#elif (FSAL_CFG_BACKEND == 1)
			if (f_lseek(&handle->native_handle, current_offset) != FR_OK) {
				FSAL_FREE(search_buffer);
				f_lseek(&handle->native_handle, original_pos);
				return FSAL_ERROR_IO;
			}
			UINT fatfs_read = 0;
			FRESULT fresult = f_read(&handle->native_handle, search_buffer, bytes_to_read, &fatfs_read);
			bytes_read = fatfs_read;
			if (fresult != FR_OK) {
				FSAL_FREE(search_buffer);
				f_lseek(&handle->native_handle, original_pos);
				return FSAL_ERROR_IO;
			}
#endif

			if (bytes_read == 0) {
				break; /* EOF reached */
			}

			/* Search for pattern in buffer */
			if (bytes_read >= pattern_size) {
				size_t i;
				for (i = 0; i <= bytes_read - pattern_size; i++) {
					if (memcmp(&search_buffer[i], pattern_bytes, pattern_size) == 0) {
						/* Pattern found! */
						match_position = current_offset + (long)i;
						search_result = FSAL_SUCCESS;
						goto search_complete;
					}
				}
			}

			/* Move to next chunk with overlap to catch patterns spanning chunks */
			if (bytes_read > overlap) {
				current_offset += (long)(bytes_read - overlap);
			} else {
				break; /* Not enough data for overlap or would stay in same position */
			}
		}
	} else {
		/* Backward search: from end to beginning */
		long current_offset = file_size;
		size_t overlap = pattern_size - 1;

		while (current_offset > 0) {
			/* Calculate read position and size */
			size_t bytes_to_read = search_buffer_size;
			if (current_offset < (long)bytes_to_read) {
				bytes_to_read = (size_t)current_offset;
			}

			current_offset -= (long)bytes_to_read;

			/* Read chunk */
			size_t bytes_read = 0;

#if (FSAL_CFG_BACKEND == 0)
			if (fseek(handle->native_handle, current_offset, SEEK_SET) != 0) {
				FSAL_FREE(search_buffer);
				fseek(handle->native_handle, original_pos, SEEK_SET);
				return FSAL_ERROR_IO;
			}
			bytes_read = fread(search_buffer, 1, bytes_to_read, handle->native_handle);
			if (bytes_read == 0 && ferror(handle->native_handle)) {
				FSAL_FREE(search_buffer);
				fseek(handle->native_handle, original_pos, SEEK_SET);
				return FSAL_ERROR_IO;
			}
#elif (FSAL_CFG_BACKEND == 1)
			if (f_lseek(&handle->native_handle, current_offset) != FR_OK) {
				FSAL_FREE(search_buffer);
				f_lseek(&handle->native_handle, original_pos);
				return FSAL_ERROR_IO;
			}
			UINT fatfs_read = 0;
			FRESULT fresult = f_read(&handle->native_handle, search_buffer, bytes_to_read, &fatfs_read);
			bytes_read = fatfs_read;
			if (fresult != FR_OK) {
				FSAL_FREE(search_buffer);
				f_lseek(&handle->native_handle, original_pos);
				return FSAL_ERROR_IO;
			}
#endif

			if (bytes_read == 0) {
				break;
			}

			/* Search backward in buffer (find last occurrence in this chunk) */
			if (bytes_read >= pattern_size) {
				long i;
				for (i = (long)bytes_read - (long)pattern_size; i >= 0; i--) {
					if (memcmp(&search_buffer[i], pattern_bytes, pattern_size) == 0) {
						/* Pattern found! */
						match_position = current_offset + i;
						search_result = FSAL_SUCCESS;
						goto search_complete;
					}
				}
			}

			/* Adjust for overlap */
			if (current_offset > 0 && overlap > 0) {
				current_offset += (long)overlap;
				if (current_offset > file_size) {
					current_offset = file_size;
				}
			}
		}
	}

search_complete:
	/* Free search buffer */
	FSAL_FREE(search_buffer);

#if FSAL_CFG_READ_BUFFERING_ENABLED
	/* Invalidate read buffer (similar to fsal_seek behavior) */
	fsal_invalidate_buffer(handle);
#endif

	/* Position file pointer based on result */
	if (search_result == FSAL_SUCCESS && seek_to_match) {
		/* Pattern found and user wants to seek to it */
		*found_position = match_position;

#if (FSAL_CFG_BACKEND == 0)
		if (fseek(handle->native_handle, match_position, SEEK_SET) != 0) {
			/* Seek failed - restore original position */
			fseek(handle->native_handle, original_pos, SEEK_SET);
			return FSAL_ERROR_IO;
		}
#elif (FSAL_CFG_BACKEND == 1)
		if (f_lseek(&handle->native_handle, match_position) != FR_OK) {
			/* Seek failed - restore original position */
			f_lseek(&handle->native_handle, original_pos);
			return FSAL_ERROR_IO;
		}
#elif (FSAL_CFG_BACKEND == 2)
		if (s_custom_interface->seek != NULL) {
			fsal_result_t seek_result = s_custom_interface->seek(handle->native_handle, match_position, SEEK_SET);
			if (seek_result != FSAL_SUCCESS) {
				s_custom_interface->seek(handle->native_handle, original_pos, SEEK_SET);
				return FSAL_ERROR_IO;
			}
		}
#endif
	} else {
		/* Pattern not found OR user doesn't want to seek - restore original position */
#if (FSAL_CFG_BACKEND == 0)
		fseek(handle->native_handle, original_pos, SEEK_SET);
#elif (FSAL_CFG_BACKEND == 1)
		f_lseek(&handle->native_handle, original_pos);
#elif (FSAL_CFG_BACKEND == 2)
		if (s_custom_interface->seek != NULL) {
			s_custom_interface->seek(handle->native_handle, original_pos, SEEK_SET);
		}
#endif

		if (search_result == FSAL_SUCCESS) {
			*found_position = match_position;
		}
	}

	return search_result;
}

#if FSAL_CFG_READ_BUFFERING_ENABLED
fsal_result_t fsal_enable_buffering(fsal_handle_t* handle, fsal_buffer_mode_t mode, size_t buffer_size) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!fsal_is_handle_valid(handle)) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	/* Check if file is opened for reading */
	if (handle->mode != FSAL_MODE_READ && handle->mode != FSAL_MODE_READ_WRITE &&
		handle->mode != FSAL_MODE_CREATE_READ_WRITE) {
		return FSAL_ERROR_UNSUPPORTED;
	}

	/* If disabling buffering */
	if (mode == FSAL_BUFFER_NONE) {
		if (handle->buffer.data != NULL) {
			FSAL_FREE(handle->buffer.data);
			handle->buffer.data = NULL;
		}
		handle->buffer.enabled = false;
		handle->buffer.capacity = 0;
		handle->buffer.valid_bytes = 0;
		handle->buffer.mode = FSAL_BUFFER_NONE;
		return FSAL_SUCCESS;
	}

	/* Use default buffer size if not specified */
	if (buffer_size == 0) {
		buffer_size = FSAL_CFG_READ_BUFFER_SIZE;
	}

	/* If buffer already exists and size matches, just change mode */
	if (handle->buffer.data != NULL && handle->buffer.capacity == buffer_size) {
		handle->buffer.mode = mode;
		handle->buffer.enabled = true;
		fsal_invalidate_buffer(handle);
		return FSAL_SUCCESS;
	}

	/* Free existing buffer if different size */
	if (handle->buffer.data != NULL) {
		FSAL_FREE(handle->buffer.data);
		handle->buffer.data = NULL;
	}

	/* Allocate new buffer */
	handle->buffer.data = (uint8_t*)FSAL_MALLOC(buffer_size);
	if (handle->buffer.data == NULL) {
		handle->buffer.enabled = false;
		handle->buffer.capacity = 0;
		return FSAL_ERROR_MEMORY;
	}

	/* Initialize buffer state */
	handle->buffer.capacity = buffer_size;
	handle->buffer.valid_bytes = 0;
	handle->buffer.buffer_file_offset = -1;
	handle->buffer.mode = mode;
	handle->buffer.enabled = true;
	handle->buffer.sequential_reads = 0;
	handle->buffer.random_reads = 0;
	handle->buffer.last_read_offset = -1;

	return FSAL_SUCCESS;
}
#endif /* FSAL_CFG_READ_BUFFERING_ENABLED */

/*============================================================================*/
/* PRIVATE FUNCTION IMPLEMENTATIONS                                           */
/*============================================================================*/

static const char* fsal_mode_to_string(fsal_mode_t mode) {
	switch (mode) {
		case FSAL_MODE_READ:
			return "rb";
		case FSAL_MODE_WRITE:
			return "wb";
		case FSAL_MODE_APPEND:
			return "ab";
		case FSAL_MODE_READ_WRITE:
			return "r+b";
		case FSAL_MODE_CREATE_READ_WRITE:
			return "w+b";
		case FSAL_MODE_CREATE_NEW:
			return "wbx";
		default:
			return NULL;
	}
}

static void fsal_init_handle(fsal_handle_t* handle, fsal_mode_t mode) {
	memset(handle, 0, sizeof(fsal_handle_t));
	handle->mode = mode;
	handle->is_open = false;
	handle->handle_id = FSAL_INVALID_HANDLE_ID;

#if FSAL_CFG_STATISTICS_ENABLED
	memset(&handle->stats, 0, sizeof(handle->stats));
#endif
}

static bool fsal_is_handle_valid(const fsal_handle_t* handle) {
	return (handle != NULL && handle->is_open && handle->handle_id != FSAL_INVALID_HANDLE_ID);
}

#if (FSAL_CFG_BACKEND == 0)
static fsal_result_t fsal_errno_to_result(int error_code) {
	switch (error_code) {
		case 0:
			return FSAL_SUCCESS;
		case ENOENT:
			return FSAL_ERROR_NOT_FOUND;
		case EACCES:
			return FSAL_ERROR_ACCESS_DENIED;
		case ENOSPC:
			return FSAL_ERROR_NO_SPACE;
		case ENOMEM:
			return FSAL_ERROR_MEMORY;
		case EINVAL:
			return FSAL_ERROR_INVALID_PARAM;
		case EEXIST:
			return FSAL_ERROR_ALREADY_EXISTS;
		case EMFILE:
			return FSAL_ERROR_TOO_MANY_OPEN;
		case ENFILE:
			return FSAL_ERROR_TOO_MANY_OPEN;
		default:
			return FSAL_ERROR_IO;
	}
}
#endif

#if (FSAL_CFG_BACKEND == 1)
static BYTE fsal_mode_to_fatfs(fsal_mode_t mode) {
	switch (mode) {
		case FSAL_MODE_READ:
			return FA_READ;
		case FSAL_MODE_WRITE:
			return FA_WRITE | FA_CREATE_ALWAYS;
		case FSAL_MODE_APPEND:
			return FA_WRITE | FA_OPEN_APPEND;
		case FSAL_MODE_READ_WRITE:
			return FA_READ | FA_WRITE | FA_OPEN_EXISTING;
		case FSAL_MODE_CREATE_READ_WRITE:
			return FA_READ | FA_WRITE | FA_CREATE_ALWAYS;
		case FSAL_MODE_CREATE_NEW:
			return FA_WRITE | FA_CREATE_NEW;
		default:
			return 0;
	}
}

static fsal_result_t fsal_fatfs_to_result(FRESULT fresult) {
	switch (fresult) {
		case FR_OK:
			return FSAL_SUCCESS;
		case FR_NO_FILE:
			return FSAL_ERROR_NOT_FOUND;
		case FR_NO_PATH:
			return FSAL_ERROR_NOT_FOUND;
		case FR_DENIED:
			return FSAL_ERROR_ACCESS_DENIED;
		case FR_EXIST:
			return FSAL_ERROR_ALREADY_EXISTS;
		case FR_INVALID_NAME:
			return FSAL_ERROR_INVALID_PARAM;
		case FR_INVALID_OBJECT:
			return FSAL_ERROR_INVALID_PARAM;
		case FR_INVALID_PARAMETER:
			return FSAL_ERROR_INVALID_PARAM;
#ifdef FR_NO_SPACE_LEFT
		case FR_NO_SPACE_LEFT:
			return FSAL_ERROR_NO_SPACE;
#endif
		case FR_DISK_ERR:
			return FSAL_ERROR_IO;
		case FR_INT_ERR:
			return FSAL_ERROR_CORRUPTED;
		case FR_NOT_READY:
			return FSAL_ERROR_IO;
		case FR_WRITE_PROTECTED:
			return FSAL_ERROR_ACCESS_DENIED;
		case FR_TOO_MANY_OPEN_FILES:
			return FSAL_ERROR_TOO_MANY_OPEN;
		case FR_LOCKED:
			return FSAL_ERROR_BUSY;
		default:
			return FSAL_ERROR_IO;
	}
}
#endif

#if FSAL_CFG_STATISTICS_ENABLED
static void fsal_update_stats(fsal_handle_t* handle, size_t bytes_read, size_t bytes_written, bool is_read_op,
							  bool is_write_op) {
	if (handle == NULL) return;

	/* Update handle-specific statistics */
	handle->stats.bytes_read += bytes_read;
	handle->stats.bytes_written += bytes_written;

	if (is_read_op) {
		handle->stats.read_ops++;
	}
	if (is_write_op) {
		handle->stats.write_ops++;
	}

	/* Update global statistics */
	s_fsal_state.global_stats.total_bytes_read += bytes_read;
	s_fsal_state.global_stats.total_bytes_written += bytes_written;
}
#endif

#if FSAL_CFG_READ_BUFFERING_ENABLED

static bool fsal_is_data_in_buffer(const fsal_handle_t* handle, long file_offset, size_t size) {
	if (!handle->buffer.enabled || handle->buffer.data == NULL || handle->buffer.valid_bytes == 0) {
		return false;
	}

	/* Check if requested range is within buffer */
	long buffer_start = handle->buffer.buffer_file_offset;
	long buffer_end = buffer_start + (long)handle->buffer.valid_bytes;
	long request_end = file_offset + (long)size;

	return (file_offset >= buffer_start && request_end <= buffer_end);
}

static size_t fsal_copy_from_buffer(fsal_handle_t* handle, void* dest, long file_offset, size_t size) {
	if (!handle->buffer.enabled || handle->buffer.data == NULL) {
		return 0;
	}

	/* Calculate offset within buffer */
	long buffer_offset = file_offset - handle->buffer.buffer_file_offset;

	if (buffer_offset < 0 || buffer_offset >= (long)handle->buffer.valid_bytes) {
		return 0;
	}

	/* Calculate how many bytes we can copy */
	size_t available = handle->buffer.valid_bytes - (size_t)buffer_offset;
	size_t to_copy = (size < available) ? size : available;

	/* Copy data */
	memcpy(dest, handle->buffer.data + buffer_offset, to_copy);

#if FSAL_CFG_STATISTICS_ENABLED
	handle->stats.buffer_hits++;
#endif

	return to_copy;
}

static fsal_result_t fsal_fill_buffer(fsal_handle_t* handle, long file_offset, size_t read_size) {
	if (!handle->buffer.enabled || handle->buffer.data == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	/* Limit read size to buffer capacity */
	if (read_size > handle->buffer.capacity) {
		read_size = handle->buffer.capacity;
	}

	/* Perform actual read from backend */
	size_t bytes_read = 0;
	fsal_result_t result;

#if (FSAL_CFG_BACKEND == 0)
	/* Save current position */
	long saved_pos = ftell(handle->native_handle);

	/* Seek to desired position */
	if (fseek(handle->native_handle, file_offset, SEEK_SET) != 0) {
		return FSAL_ERROR_IO;
	}

	/* Read into buffer */
	bytes_read = fread(handle->buffer.data, 1, read_size, handle->native_handle);

	if (ferror(handle->native_handle)) {
		result = FSAL_ERROR_IO;
		/* Restore position on error */
		fseek(handle->native_handle, saved_pos, SEEK_SET);
		return result;
	}

	result = FSAL_SUCCESS;

#elif (FSAL_CFG_BACKEND == 1)
	/* Save current position */
	FSIZE_t saved_pos = f_tell(&handle->native_handle);

	/* Seek to desired position */
	FRESULT fresult = f_lseek(&handle->native_handle, file_offset);
	if (fresult != FR_OK) {
		return fsal_fatfs_to_result(fresult);
	}

	/* Read into buffer */
	UINT fatfs_read = 0;
	fresult = f_read(&handle->native_handle, handle->buffer.data, read_size, &fatfs_read);
	bytes_read = fatfs_read;
	result = fsal_fatfs_to_result(fresult);

	/* Restore position */
	f_lseek(&handle->native_handle, saved_pos);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom backend - may not support buffered reads efficiently */
	result = FSAL_ERROR_UNSUPPORTED;
#endif

	if (result == FSAL_SUCCESS) {
		handle->buffer.buffer_file_offset = file_offset;
		handle->buffer.valid_bytes = bytes_read;
	}

	return result;
}

static void fsal_invalidate_buffer(fsal_handle_t* handle) {
	if (handle->buffer.enabled && handle->buffer.data != NULL) {
		handle->buffer.valid_bytes = 0;
		handle->buffer.buffer_file_offset = -1;
	}
}

static void fsal_update_access_pattern(fsal_handle_t* handle, long current_offset) {
	if (!handle->buffer.enabled) {
		return;
	}

	/* Check if this is a sequential read */
	long expected_offset = handle->buffer.last_read_offset;
	long distance = current_offset - expected_offset;

	if (distance >= 0 && (size_t)distance <= FSAL_CFG_SEQUENTIAL_TOLERANCE) {
		/* Sequential access detected */
		handle->buffer.sequential_reads++;
		if (handle->buffer.random_reads > 0) {
			handle->buffer.random_reads--;
		}
	} else {
		/* Random access detected */
		handle->buffer.random_reads++;
		if (handle->buffer.sequential_reads > 0) {
			handle->buffer.sequential_reads--;
		}
	}

	handle->buffer.last_read_offset = current_offset;
}

static size_t fsal_calculate_readahead_size(const fsal_handle_t* handle, size_t requested_size) {
	if (!handle->buffer.enabled) {
		return requested_size;
	}

	size_t readahead_size = requested_size;

	switch (handle->buffer.mode) {
		case FSAL_BUFFER_NONE:
			/* No read-ahead */
			readahead_size = requested_size;
			break;

		case FSAL_BUFFER_FULL:
			/* Always read full buffer */
			readahead_size = handle->buffer.capacity;
			break;

		case FSAL_BUFFER_ADAPTIVE:
			/* Adaptive based on access pattern */
			if (handle->buffer.sequential_reads >= FSAL_CFG_READAHEAD_THRESHOLD) {
				/* Sequential pattern detected - aggressive read-ahead */
				readahead_size = requested_size * FSAL_CFG_READAHEAD_MULTIPLIER;
				if (readahead_size > handle->buffer.capacity) {
					readahead_size = handle->buffer.capacity;
				}
			} else {
				/* Random pattern or insufficient history - minimal read-ahead */
				readahead_size = requested_size;
			}
			break;
	}

	return readahead_size;
}

static fsal_result_t fsal_read_direct(void* buffer, size_t size, size_t count, fsal_handle_t* handle,
									  size_t* bytes_read) {
	size_t total_bytes = size * count;
	size_t actual_read = 0;
	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	actual_read = fread(buffer, 1, total_bytes, handle->native_handle);
	if (actual_read != total_bytes && ferror(handle->native_handle)) {
		result = FSAL_ERROR_IO;
	}

#elif (FSAL_CFG_BACKEND == 1)
	UINT fatfs_read = 0;
	FRESULT fresult = f_read(&handle->native_handle, buffer, total_bytes, &fatfs_read);
	actual_read = fatfs_read;
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	result = s_custom_interface->read(handle->native_handle, buffer, size, count, &actual_read);
#endif

	if (bytes_read != NULL) {
		*bytes_read = actual_read;
	}

	return result;
}

#endif /* FSAL_CFG_READ_BUFFERING_ENABLED */

/*============================================================================*/
/* DIRECTORY OPERATIONS                                                       */
/*============================================================================*/

fsal_result_t fsal_opendir(const char* path, fsal_dir_handle_t* dir_handle) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (path == NULL || dir_handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	/* Check if library is initialized */
	if (!s_fsal_state.is_initialized) {
		return FSAL_ERROR_NOT_INITIALIZED;
	}

	memset(dir_handle, 0, sizeof(fsal_dir_handle_t));

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	DIR* native_dir = opendir(path);
	if (native_dir == NULL) {
		result = fsal_errno_to_result(errno);
	} else {
		dir_handle->native_handle = native_dir;
		dir_handle->is_open = true;
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FRESULT fresult = f_opendir(&dir_handle->native_handle, path);
	if (fresult == FR_OK) {
		dir_handle->is_open = true;
	} else {
		result = fsal_fatfs_to_result(fresult);
	}

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation - not supported */
	result = FSAL_ERROR_UNSUPPORTED;
#else
#error "Invalid FSAL_CFG_BACKEND value"
#endif

	return result;
}

fsal_result_t fsal_readdir(fsal_dir_handle_t* dir_handle, fsal_dir_entry_t* entry) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (dir_handle == NULL || entry == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!dir_handle->is_open) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	memset(entry, 0, sizeof(fsal_dir_entry_t));

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	struct dirent* de;
	for (;;) {
		errno = 0;
		de = readdir((DIR*)dir_handle->native_handle);
		if (de == NULL) {
			result = (errno != 0) ? fsal_errno_to_result(errno) : FSAL_ERROR_NOT_FOUND;
			break;
		}
		/* Skip "." and ".." entries */
		if (de->d_name[0] == '.' && (de->d_name[1] == '\0' ||
			(de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
			continue;
		}
		strncpy(entry->name, de->d_name, FSAL_MAX_PATH_LENGTH - 1);
		entry->name[FSAL_MAX_PATH_LENGTH - 1] = '\0';
#ifdef _DIRENT_HAVE_D_TYPE
		entry->is_directory = (de->d_type == DT_DIR);
#endif
		break;
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FILINFO fno;
	for (;;) {
		FRESULT fresult = f_readdir(&dir_handle->native_handle, &fno);
		if (fresult != FR_OK) {
			result = fsal_fatfs_to_result(fresult);
			break;
		}
		if (fno.fname[0] == '\0') {
			/* End of directory */
			result = FSAL_ERROR_NOT_FOUND;
			break;
		}
		/* Skip "." entries */
		if (fno.fname[0] == '.') {
			continue;
		}
		strncpy(entry->name, fno.fname, FSAL_MAX_PATH_LENGTH - 1);
		entry->name[FSAL_MAX_PATH_LENGTH - 1] = '\0';
		entry->size = (size_t)fno.fsize;
		entry->is_directory = (fno.fattrib & AM_DIR) != 0;
		break;
	}

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation - not supported */
	result = FSAL_ERROR_UNSUPPORTED;
#else
#error "Invalid FSAL_CFG_BACKEND value"
#endif

	return result;
}

fsal_result_t fsal_closedir(fsal_dir_handle_t* dir_handle) {
	/* Parameter validation */
#if FSAL_CFG_PARAM_VALIDATION_ENABLED
	if (dir_handle == NULL) {
		return FSAL_ERROR_INVALID_PARAM;
	}
#endif

	if (!dir_handle->is_open) {
		return FSAL_ERROR_INVALID_PARAM;
	}

	fsal_result_t result = FSAL_SUCCESS;

#if (FSAL_CFG_BACKEND == 0)
	/* Standard C Library implementation */
	if (closedir((DIR*)dir_handle->native_handle) != 0) {
		result = fsal_errno_to_result(errno);
	}

#elif (FSAL_CFG_BACKEND == 1)
	/* FatFS implementation */
	FRESULT fresult = f_closedir(&dir_handle->native_handle);
	result = fsal_fatfs_to_result(fresult);

#elif (FSAL_CFG_BACKEND == 2)
	/* Custom implementation - not supported */
	result = FSAL_ERROR_UNSUPPORTED;
#else
#error "Invalid FSAL_CFG_BACKEND value"
#endif

	dir_handle->is_open = false;

	return result;
}

/* Additional utility functions implementation would continue here... */
/* For brevity, I'm including the core functions. The remaining utility */
/* functions would follow the same pattern. */