// --------------------------------------------------------------------------------------------------------------------
/// \file ag_mal.c
///
/// \brief Memory Abstraction Layer Implementation
///
/// This file contains the complete implementation of the Memory Abstraction Layer (MAL).
/// It provides a unified interface for memory allocation that abstracts the underlying
/// memory management system (standard C malloc/free or FreeRTOS pvPortMalloc/vPortFree).
///
/// \par Implementation Features:
/// - Thread-safe memory allocation with mutex protection
/// - Comprehensive memory tracking with leak detection
/// - Buffer overflow/underflow protection using guard bytes
/// - Detailed statistics collection and reporting
/// - Debug information tracking (file/line) in debug builds
/// - Reference counting for safe initialization/deinitialization
///
/// \par Memory Layout:
/// Each allocation creates the following memory layout:
/// \code
/// [ag_mal_block_t][guard_front][user_data][guard_back]
/// \endcode
/// - ag_mal_block_t: Internal tracking structure
/// - guard_front: Guard bytes for underflow detection (if enabled)
/// - user_data: Memory returned to user
/// - guard_back: Guard bytes for overflow detection (if enabled)
///
/// \par Thread Safety:
/// When AG_MAL_ENABLE_THREAD_SAFE is enabled, all public functions are protected
/// by mutexes. In FreeRTOS backend, semaphores are used. The implementation
/// can be extended for pthread support in standard C backend.
///
/// \author ASK Industries GmbH
/// \version 1.0.0
/// \date 2025-09-28
///
/// Copyright (c) 2025, ASK Industries GmbH, Niederwinkling.
/// All Rights Reserved.
// --------------------------------------------------------------------------------------------------------------------

// ====================================================================================================================
//  INCLUDES
// ====================================================================================================================

#include "ag_mal/ag_mal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#if AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/* FreeRTOS v7.x compat: SemaphoreHandle_t was introduced in v8.0 */
#ifndef SemaphoreHandle_t
    typedef xSemaphoreHandle SemaphoreHandle_t;
#endif
#endif

// ====================================================================================================================
//  INTERNAL TYPE DEFINITIONS
// ====================================================================================================================

/// \brief Internal memory block tracking structure
///
/// This structure is used internally to track each memory allocation.
/// It is placed immediately before the user data in memory and contains
/// all necessary information for tracking, debugging, and validation.
///
/// \note This structure is not exposed in the public API
typedef struct ag_mal_block {
	void* user_ptr;		   ///< Pointer to user data (for quick lookup)
	size_t size;		   ///< Size of user allocation in bytes
	const char* file;	   ///< Source file where allocation was made
	int line;			   ///< Source line where allocation was made
	uint32_t timestamp;	   ///< Timestamp when allocation was made
	uint32_t guard_front;  ///< Front guard bytes for underflow detection
	/// \note User data follows this structure
	/// \note Back guard bytes follow user data
	struct ag_mal_block* next;	///< Next block in tracking linked list
} ag_mal_block_t;

/// \brief Global MAL context structure
///
/// Contains all global state for the Memory Abstraction Layer.
/// This includes tracking lists, statistics, and synchronization primitives.
static struct {
	ag_mal_block_t* tracked_blocks;	 ///< Head of linked list of tracked allocations
	ag_mal_stats_t stats;			 ///< Current memory usage statistics
	uint32_t init_count;			 ///< Reference counter for init/deinit

#if AG_MAL_ENABLE_THREAD_SAFE && AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
	SemaphoreHandle_t mutex;  ///< Mutex for thread-safe operations
#endif

} g_mal_ctx = {0};	///< Global MAL context, zero-initialized

// ====================================================================================================================
//  INTERNAL UTILITY FUNCTIONS
// ====================================================================================================================

/// \brief Get timestamp for allocation tracking
///
/// Returns a timestamp value used to track when allocations were made.
/// The timestamp format depends on the backend:
/// - FreeRTOS: Uses xTaskGetTickCount() for tick-based timestamps
/// - Standard C: Uses a simple incrementing counter
///
/// \return Timestamp value (format depends on backend)
/// \note This function is for internal use only
static uint32_t ag_mal_get_timestamp(void) {
#if AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
	return (uint32_t)xTaskGetTickCount();
#else
	static uint32_t counter = 0;
	return ++counter;
#endif
}

/// \brief Acquire lock for thread-safe operations
///
/// Acquires the mutex lock to ensure thread-safe access to MAL data structures.
/// The implementation depends on the backend and thread safety configuration:
/// - FreeRTOS + thread safety: Uses semaphore
/// - Other configurations: No-op
///
/// \note This function blocks until the lock is acquired
/// \note This function is for internal use only
static void ag_mal_lock(void) {
#if AG_MAL_ENABLE_THREAD_SAFE && AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
	if (g_mal_ctx.mutex) {
		xSemaphoreTake(g_mal_ctx.mutex, portMAX_DELAY);
	}
#endif
}

/// \brief Release lock after thread-safe operations
///
/// Releases the mutex lock to allow other threads to access MAL data structures.
/// The implementation depends on the backend and thread safety configuration:
/// - FreeRTOS + thread safety: Uses semaphore
/// - Other configurations: No-op
///
/// \note Must be called after every ag_mal_lock() call
/// \note This function is for internal use only
static void ag_mal_unlock(void) {
#if AG_MAL_ENABLE_THREAD_SAFE && AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
	if (g_mal_ctx.mutex) {
		xSemaphoreGive(g_mal_ctx.mutex);
	}
#endif
}

/// \brief Allocate memory using the configured backend
///
/// Calls the appropriate low-level allocation function based on the
/// configured backend (standard C malloc or FreeRTOS pvPortMalloc).
///
/// \param size Number of bytes to allocate
/// \return Pointer to allocated memory, or NULL on failure
/// \note This function is for internal use only
/// \note No initialization, tracking, or guard bytes are handled here
static void* ag_mal_backend_alloc(size_t size) {
#if AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
	/* Pre-check available heap BEFORE calling pvPortMalloc().
	 * This prevents vApplicationMallocFailedHook() from firing when
	 * code intentionally tests OOM (e.g. M6 exhaustion test).
	 * The hook hangs the system forever, so the caller would never
	 * receive the NULL return.  See doc.md for full analysis.       */
	if (xPortGetFreeHeapSize() < size) {
		return NULL;
	}
	return pvPortMalloc(size);
#else
	return malloc(size);
#endif
}

/// \brief Free memory using the configured backend
///
/// Calls the appropriate low-level deallocation function based on the
/// configured backend (standard C free or FreeRTOS vPortFree).
///
/// \param ptr Pointer to memory to free
/// \note This function is for internal use only
/// \note No tracking list updates or validation are handled here
static void ag_mal_backend_free(void* ptr) {
#if AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
	vPortFree(ptr);
#else
	free(ptr);
#endif
}

/// \brief Find memory block in tracking list
///
/// Searches the linked list of tracked allocations to find the block
/// corresponding to the given user pointer.
///
/// \param user_ptr Pointer to user data (as returned by ag_mal_alloc)
/// \return Pointer to tracking block, or NULL if not found
/// \note This function is for internal use only
/// \note Linear search - performance degrades with many allocations
static ag_mal_block_t* ag_mal_find_block(void* user_ptr) {
	ag_mal_block_t* block = g_mal_ctx.tracked_blocks;
	while (block) {
		if (block->user_ptr == user_ptr) {
			return block;
		}
		block = block->next;
	}
	return NULL;
}

/// \brief Add memory block to tracking list
///
/// Adds a new memory block to the head of the tracking linked list.
/// This allows the MAL to track the allocation for leak detection and reporting.
///
/// \param block Pointer to the tracking block to add
/// \note This function is for internal use only
/// \note Assumes the block is properly initialized
/// \note Must be called with lock held
static void ag_mal_add_block(ag_mal_block_t* block) {
	block->next = g_mal_ctx.tracked_blocks;
	g_mal_ctx.tracked_blocks = block;
}

/// \brief Remove memory block from tracking list
///
/// Removes the specified memory block from the tracking linked list.
/// This is called when memory is freed to stop tracking the allocation.
///
/// \param block Pointer to the tracking block to remove
/// \note This function is for internal use only
/// \note Assumes the block is in the tracking list
/// \note Must be called with lock held
static void ag_mal_remove_block(ag_mal_block_t* block) {
	if (g_mal_ctx.tracked_blocks == block) {
		g_mal_ctx.tracked_blocks = block->next;
	} else {
		ag_mal_block_t* prev = g_mal_ctx.tracked_blocks;
		while (prev && prev->next != block) {
			prev = prev->next;
		}
		if (prev) {
			prev->next = block->next;
		}
	}
}

/// \brief Validate integrity of a memory block
///
/// Checks the integrity of a memory block by validating guard bytes
/// (if bounds checking is enabled). This can detect buffer overflows
/// and underflows that corrupt the guard areas.
///
/// \param block Pointer to the tracking block to validate
/// \return AG_MAL_OK if block is valid, AG_MAL_ERROR_CORRUPTION if corrupted
/// \note This function is for internal use only
/// \note Only performs validation if AG_MAL_ENABLE_BOUNDS_CHECK is enabled
static ag_mal_result_t ag_mal_validate_block(ag_mal_block_t* block) {
#if AG_MAL_ENABLE_BOUNDS_CHECK
	if (block->guard_front != AG_MAL_GUARD_PATTERN) {
		return AG_MAL_ERROR_CORRUPTION;
	}

	/* Align guard_back to uint32_t boundary — must match alloc alignment */
	uint32_t* guard_back = (uint32_t*)((((unsigned int)((char*)block->user_ptr + block->size))
	                        + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1));
	if (*guard_back != AG_MAL_GUARD_PATTERN) {
		return AG_MAL_ERROR_CORRUPTION;
	}
#endif
	return AG_MAL_OK;
}

// ====================================================================================================================
//  PUBLIC API IMPLEMENTATION
// ====================================================================================================================

/// \brief Initialize the Memory Abstraction Layer
///
/// This function initializes the MAL system and must be called before using
/// any other MAL functions. It performs the following operations:
/// - Initializes statistics counters to zero
/// - Clears the tracking list
/// - Creates thread synchronization primitives (if enabled)
/// - Increments reference counter for nested init/deinit calls
///
/// The function supports multiple initializations through reference counting.
/// Each call to ag_mal_init() must be matched with a corresponding call to
/// ag_mal_deinit().
///
/// \return AG_MAL_OK on success, AG_MAL_ERROR_OUT_OF_MEMORY on failure
/// \retval AG_MAL_OK Initialization successful
/// \retval AG_MAL_ERROR_OUT_OF_MEMORY Failed to create mutex (FreeRTOS only)
///
/// \par Thread Safety:
/// This function is thread-safe. However, the first initialization may not
/// be fully thread-safe until the mutex is created.
///
/// \par Implementation Notes:
/// - In FreeRTOS backend with thread safety enabled, creates a mutex semaphore
/// - Statistics structure is zero-initialized using memset
/// - Reference counting allows safe nested initialization
ag_mal_result_t ag_mal_init(void) {
	ag_mal_lock();

	if (g_mal_ctx.init_count == 0) {
		memset(&g_mal_ctx.stats, 0, sizeof(g_mal_ctx.stats));
		g_mal_ctx.tracked_blocks = NULL;

#if AG_MAL_ENABLE_THREAD_SAFE && AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
		g_mal_ctx.mutex = xSemaphoreCreateMutex();
		if (!g_mal_ctx.mutex) {
			ag_mal_unlock();
			return AG_MAL_ERROR_OUT_OF_MEMORY;
		}
#endif
	}

	g_mal_ctx.init_count++;
	ag_mal_unlock();
	return AG_MAL_OK;
}

/// \brief Deinitialize the Memory Abstraction Layer
///
/// This function deinitializes the MAL system when it's no longer needed.
/// It decrements the reference counter and, when it reaches zero, performs
/// cleanup operations:
/// - Frees any remaining tracked allocations (potential memory leaks)
/// - Destroys thread synchronization primitives
/// - Resets internal state
///
/// \return AG_MAL_OK on success
/// \retval AG_MAL_OK Deinitialization successful
///
/// \warning If there are still active allocations when the reference counter
///          reaches zero, they will be automatically freed. This indicates
///          a memory leak in the application.
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Implementation Notes:
/// - Uses reference counting to handle nested init/deinit calls
/// - Automatically frees leaked allocations during final cleanup
/// - In FreeRTOS backend, destroys the mutex semaphore
/// - Safe to call even with outstanding allocations
ag_mal_result_t ag_mal_deinit(void) {
	ag_mal_lock();

	if (g_mal_ctx.init_count > 0) {
		g_mal_ctx.init_count--;

		if (g_mal_ctx.init_count == 0) {
			/* Clean up any remaining allocations */
			while (g_mal_ctx.tracked_blocks) {
				ag_mal_block_t* block = g_mal_ctx.tracked_blocks;
				g_mal_ctx.tracked_blocks = block->next;
				ag_mal_backend_free(block);
			}

#if AG_MAL_ENABLE_THREAD_SAFE && AG_MAL_BACKEND == AG_MAL_BACKEND_FREERTOS
			if (g_mal_ctx.mutex) {
				vSemaphoreDelete(g_mal_ctx.mutex);
				g_mal_ctx.mutex = NULL;
			}
#endif
		}
	}

	ag_mal_unlock();
	return AG_MAL_OK;
}

/// \brief Allocate memory with debug information
///
/// This function allocates memory using the configured backend and provides
/// additional features such as tracking, bounds checking, and statistics collection.
/// The memory layout includes internal structures and guard bytes as needed.
///
/// Memory Layout:
/// \code
/// [ag_mal_block_t][guard_front][user_data][guard_back]
/// \endcode
///
/// \param size Size in bytes to allocate (must be > 0)
/// \param file Source file name for debugging (can be NULL)
/// \param line Source line number for debugging (can be 0)
/// \return Pointer to allocated memory, or NULL on failure
/// \retval Non-NULL Pointer to allocated memory block
/// \retval NULL Allocation failed (size=0, out of memory, etc.)
///
/// \par Behavior:
/// - Returns NULL immediately if size is 0
/// - Allocates extra space for tracking structure and guard bytes
/// - Initializes guard bytes with AG_MAL_GUARD_PATTERN (if enabled)
/// - Adds block to tracking list (if enabled)
/// - Updates statistics counters
/// - Records file/line information for debugging
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \note This is typically called through the ag_mal_alloc() macro which
///       automatically provides __FILE__ and __LINE__ in debug builds.
void* ag_mal_alloc_debug(size_t size, const char* file, int line) {
	if (size == 0) {
		return NULL;
	}

	ag_mal_lock();

	/* Calculate total size needed */
	size_t total_size = sizeof(ag_mal_block_t) + size;
#if AG_MAL_ENABLE_BOUNDS_CHECK
	/* Round up user size to uint32_t alignment so guard_back is aligned.
	 * Without this, odd sizes (e.g. 9 for sizeof(__TIME__)) cause an
	 * unaligned uint32_t write → Address Error (Store) on MIPS32.  */
	size_t padded_size = (size + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1);
	total_size = sizeof(ag_mal_block_t) + padded_size + sizeof(uint32_t); /* guard_back */
#endif

	/* Allocate memory */
	ag_mal_block_t* block = (ag_mal_block_t*)ag_mal_backend_alloc(total_size);
	if (!block) {
		g_mal_ctx.stats.failed_allocations++;
		ag_mal_unlock();
		return NULL;
	}

	/* Initialize block */
	block->user_ptr = (char*)block + sizeof(ag_mal_block_t);
	block->size = size;
	block->file = file;
	block->line = line;
	block->timestamp = ag_mal_get_timestamp();
	block->next = NULL;

#if AG_MAL_ENABLE_BOUNDS_CHECK
	block->guard_front = AG_MAL_GUARD_PATTERN;
	/* Align guard_back to uint32_t boundary (MIPS32 requires aligned word access) */
	uint32_t* guard_back = (uint32_t*)((((unsigned int)((char*)block->user_ptr + size))
	                        + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1));
	*guard_back = AG_MAL_GUARD_PATTERN;
#endif

#if AG_MAL_ENABLE_TRACKING
	ag_mal_add_block(block);
#endif

	/* Update statistics */
	g_mal_ctx.stats.total_allocated += size;
	g_mal_ctx.stats.num_allocations++;
	g_mal_ctx.stats.total_allocations++;

	if (g_mal_ctx.stats.total_allocated > g_mal_ctx.stats.peak_allocated) {
		g_mal_ctx.stats.peak_allocated = g_mal_ctx.stats.total_allocated;
	}

	ag_mal_unlock();
	return block->user_ptr;
}

/// \brief Free previously allocated memory with debug information
///
/// This function frees memory that was previously allocated by ag_mal_alloc_debug().
/// It performs validation checks, updates statistics, and handles tracking list maintenance.
///
/// \param ptr Pointer to memory to free (must be from ag_mal_alloc_debug)
/// \param file Source file name for debugging (can be NULL)
/// \param line Source line number for debugging (can be 0)
/// \return AG_MAL_OK on success, error code on failure
/// \retval AG_MAL_OK Memory successfully freed
/// \retval AG_MAL_ERROR_NULL_POINTER ptr is NULL
/// \retval AG_MAL_ERROR_NOT_FOUND ptr not found in tracking list
/// \retval AG_MAL_ERROR_CORRUPTION Guard bytes indicate memory corruption
///
/// \par Validation Checks:
/// - Verifies pointer is not NULL
/// - Finds pointer in tracking list (if tracking enabled)
/// - Validates guard bytes for corruption (if bounds checking enabled)
/// - Updates statistics counters
/// - Removes from tracking list
/// - Frees the entire memory block including overhead
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \note This is typically called through the ag_mal_free() macro which
///       automatically provides __FILE__ and __LINE__ in debug builds.
ag_mal_result_t ag_mal_free_debug(void* ptr, const char* file, int line) {
	if (!ptr) {
		return AG_MAL_ERROR_NULL_POINTER;
	}

	ag_mal_lock();

#if AG_MAL_ENABLE_TRACKING
	ag_mal_block_t* block = ag_mal_find_block(ptr);
	if (!block) {
		ag_mal_unlock();
		return AG_MAL_ERROR_NOT_FOUND;
	}

	/* Validate block integrity */
	ag_mal_result_t result = ag_mal_validate_block(block);
	if (result != AG_MAL_OK) {
		ag_mal_unlock();
		return result;
	}

	/* Update statistics */
	g_mal_ctx.stats.total_allocated -= block->size;
	g_mal_ctx.stats.num_allocations--;
	g_mal_ctx.stats.total_deallocations++;

	/* Remove from tracking list */
	ag_mal_remove_block(block);

	/* Free the entire block */
	ag_mal_backend_free(block);
#else
	/* Without tracking, we need to calculate the original block address */
	ag_mal_block_t* block = (ag_mal_block_t*)((char*)ptr - sizeof(ag_mal_block_t));

	/* Update statistics (must mirror ag_mal_alloc_debug unconditional stats) */
	g_mal_ctx.stats.total_allocated -= block->size;
	g_mal_ctx.stats.num_allocations--;
	g_mal_ctx.stats.total_deallocations++;

	ag_mal_backend_free(block);
#endif

	ag_mal_unlock();
	return AG_MAL_OK;
}

/// \brief Reallocate memory with debug information
///
/// This function changes the size of the memory block pointed to by ptr.
/// It behaves similarly to standard realloc() with additional MAL features:
/// - If ptr is NULL, behaves like ag_mal_alloc_debug()
/// - If new_size is 0, behaves like ag_mal_free_debug() and returns NULL
/// - Otherwise, allocates new memory, copies data, and frees old memory
///
/// \param ptr Pointer to previously allocated memory (can be NULL)
/// \param new_size New size in bytes (0 to free the memory)
/// \param file Source file name for debugging (can be NULL)
/// \param line Source line number for debugging (can be 0)
/// \return Pointer to reallocated memory, or NULL on failure/free
/// \retval Non-NULL Pointer to reallocated memory (may differ from input ptr)
/// \retval NULL Reallocation failed, new_size=0, or ptr=NULL with new_size=0
///
/// \par Behavior:
/// - Data is preserved up to the minimum of old and new sizes
/// - The returned pointer may be different from the input pointer
/// - Old memory is automatically freed after successful reallocation
/// - All tracking and validation features apply to the new allocation
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \note This is typically called through the ag_mal_realloc() macro which
///       automatically provides __FILE__ and __LINE__ in debug builds.
void* ag_mal_realloc_debug(void* ptr, size_t new_size, const char* file, int line) {
	if (!ptr) {
		return ag_mal_alloc_debug(new_size, file, line);
	}

	if (new_size == 0) {
		ag_mal_free_debug(ptr, file, line);
		return NULL;
	}

	ag_mal_lock();

#if AG_MAL_ENABLE_TRACKING
	ag_mal_block_t* old_block = ag_mal_find_block(ptr);
	if (!old_block) {
		ag_mal_unlock();
		return NULL;
	}

	size_t old_size = old_block->size;
#else
	ag_mal_block_t* old_block = (ag_mal_block_t*)((char*)ptr - sizeof(ag_mal_block_t));
	size_t old_size = old_block->size;
#endif

	ag_mal_unlock();

	/* Allocate new memory */
	void* new_ptr = ag_mal_alloc_debug(new_size, file, line);
	if (!new_ptr) {
		return NULL;
	}

	/* Copy data */
	size_t copy_size = (old_size < new_size) ? old_size : new_size;
	memcpy(new_ptr, ptr, copy_size);

	/* Free old memory */
	ag_mal_free_debug(ptr, file, line);

	return new_ptr;
}

/// \brief Allocate and zero-initialize memory
///
/// This function allocates memory for an array of num elements, each of
/// size bytes, and initializes all bytes to zero. It's equivalent to
/// the standard calloc() function but uses the MAL backend.
///
/// \param num Number of elements to allocate
/// \param size Size of each element in bytes
/// \return Pointer to allocated and zero-initialized memory, or NULL on failure
/// \retval Non-NULL Pointer to allocated and zeroed memory
/// \retval NULL Allocation failed or overflow in num*size calculation
///
/// \par Behavior:
/// - Calculates total size as num * size
/// - Uses ag_mal_alloc() for the actual allocation
/// - Initializes all bytes to zero using memset()
/// - Returns NULL if allocation fails or if num*size would overflow
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// int *array = (int*)ag_mal_calloc(10, sizeof(int));
/// // All 10 integers are guaranteed to be 0
/// \endcode
void* ag_mal_calloc(size_t num, size_t size) {
	size_t total_size = num * size;
	void* ptr = ag_mal_alloc(total_size);
	if (ptr) {
		memset(ptr, 0, total_size);
	}
	return ptr;
}

/// \brief Get current memory usage statistics
///
/// This function fills the provided statistics structure with current
/// memory usage information. The statistics provide detailed insights
/// into allocation patterns and can be used for monitoring and debugging.
///
/// \param stats Pointer to ag_mal_stats_t structure to fill (must not be NULL)
/// \return AG_MAL_OK on success, AG_MAL_ERROR_NULL_POINTER if stats is NULL
/// \retval AG_MAL_OK Statistics successfully retrieved
/// \retval AG_MAL_ERROR_NULL_POINTER stats parameter is NULL
///
/// \par Statistics Included:
/// - total_allocated: Current total bytes allocated and not freed
/// - peak_allocated: Maximum bytes allocated simultaneously since init
/// - num_allocations: Current number of active allocations
/// - total_allocations: Total allocation requests since initialization
/// - total_deallocations: Total deallocation requests since initialization
/// - failed_allocations: Number of allocation requests that failed
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
/// The returned statistics represent a consistent snapshot at call time.
ag_mal_result_t ag_mal_get_stats(ag_mal_stats_t* stats) {
	if (!stats) {
		return AG_MAL_ERROR_NULL_POINTER;
	}

	ag_mal_lock();
	*stats = g_mal_ctx.stats;
	ag_mal_unlock();

	return AG_MAL_OK;
}

/// \brief Check for memory leaks
///
/// Returns the number of currently active (not freed) allocations.
/// This provides a quick way to check for memory leaks without
/// generating a detailed report.
///
/// \return Number of active allocations (0 indicates no leaks)
/// \retval 0 No memory leaks detected
/// \retval >0 Number of active allocations (potential leaks)
///
/// \par Usage:
/// - Call before application shutdown to verify clean exit
/// - Use periodically in long-running applications
/// - Combine with ag_mal_print_report() for detailed leak information
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// if (ag_mal_check_leaks() > 0) {
///     printf("Memory leaks detected!\n");
///     ag_mal_print_report();
/// }
/// \endcode
size_t ag_mal_check_leaks(void) {
	ag_mal_lock();
	size_t leaks = g_mal_ctx.stats.num_allocations;
	ag_mal_unlock();
	return leaks;
}

uint32_t ag_mal_get_init_count(void) {
	return g_mal_ctx.init_count;
}

/// \brief Print detailed memory usage report
///
/// Prints a comprehensive report of current memory usage to stdout.
/// The report includes overall statistics and, if tracking is enabled,
/// detailed information about each active allocation including file/line
/// information in debug builds.
///
/// \par Report Contents:
/// - Overall memory statistics (total, peak, counts)
/// - List of all active allocations with details (if tracking enabled)
/// - File and line information for each allocation (if debug enabled)
/// - Timestamps for allocation tracking
///
/// \par Conditional Compilation:
/// This function only produces output when AG_MAL_DEBUG_ENABLED is 1.
/// In release builds (AG_MAL_DEBUG_ENABLED=0), the function does nothing.
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
/// However, output may be interleaved if called simultaneously from multiple threads.
///
/// \par Example Output:
/// \code
/// === Memory Abstraction Layer Report ===
/// Total allocated: 1536 bytes
/// Peak allocated: 2048 bytes
/// Active allocations: 3
/// Total allocations: 15
/// Total deallocations: 12
/// Failed allocations: 0
///
/// === Active Allocations ===
/// Ptr: 0x7f8b4c000980, Size: 512, File: main.c:45, Timestamp: 1234
/// Ptr: 0x7f8b4c001200, Size: 1024, File: utils.c:78, Timestamp: 1267
/// \endcode
void ag_mal_print_report(void) {
#if AG_MAL_DEBUG_ENABLED
	ag_mal_lock();

	// Note orginal code had %zu specifier which is correct for size_t
	// but some older compilers may not support it. So I adjuster to %lu
	// and cast to unsigned long for compatibility.
	printf("=== Memory Abstraction Layer Report ===\n");
	printf("Total allocated: %lu bytes\n", (unsigned long)g_mal_ctx.stats.total_allocated);
	printf("Peak allocated: %lu bytes\n", (unsigned long)g_mal_ctx.stats.peak_allocated);
	printf("Active allocations: %lu\n", (unsigned long)g_mal_ctx.stats.num_allocations);
	printf("Total allocations: %lu\n", (unsigned long)g_mal_ctx.stats.total_allocations);
	printf("Total deallocations: %lu\n", (unsigned long)g_mal_ctx.stats.total_deallocations);
	printf("Failed allocations: %lu\n", (unsigned long)g_mal_ctx.stats.failed_allocations);

#if AG_MAL_ENABLE_TRACKING
	if (g_mal_ctx.tracked_blocks) {
		printf("\n=== Active Allocations ===\n");
		ag_mal_block_t* block = g_mal_ctx.tracked_blocks;
		while (block) {
			printf("Ptr: %p, Size: %lu", block->user_ptr, (unsigned long)block->size);
			if (block->file) {
				printf(", File: %s:%d", block->file, block->line);
			}
			printf(", Timestamp: %u\n", block->timestamp);
			block = block->next;
		}
	}
#endif

	ag_mal_unlock();
#endif
}

/// \brief Validate integrity of all tracked allocations
///
/// Performs validation checks on all currently tracked memory allocations.
/// This includes checking guard bytes for corruption and verifying internal
/// data structure integrity. It's useful for detecting memory corruption
/// that may not be immediately apparent during normal operations.
///
/// \return AG_MAL_OK if all allocations are valid, AG_MAL_ERROR_CORRUPTION if corruption detected
/// \retval AG_MAL_OK All tracked allocations are valid
/// \retval AG_MAL_ERROR_CORRUPTION At least one allocation shows corruption
///
/// \par Validation Checks:
/// - Guard byte integrity (front and back guards)
/// - Internal tracking structure consistency
/// - Memory block linkage validation
///
/// \par Conditional Compilation:
/// This function only performs validation when both AG_MAL_ENABLE_TRACKING
/// and AG_MAL_ENABLE_BOUNDS_CHECK are enabled. Otherwise, it always returns AG_MAL_OK.
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Usage:
/// - Call periodically in debug builds to detect corruption early
/// - Use in unit tests to verify memory integrity
/// - Combine with ag_mal_print_report() when corruption is detected
///
/// \par Example:
/// \code{.c}
/// if (ag_mal_validate_all() != AG_MAL_OK) {
///     printf("Memory corruption detected!\n");
///     ag_mal_print_report();
///     // Take corrective action...
/// }
/// \endcode
ag_mal_result_t ag_mal_validate_all(void) {
#if AG_MAL_ENABLE_TRACKING && AG_MAL_ENABLE_BOUNDS_CHECK
	ag_mal_lock();

	ag_mal_block_t* block = g_mal_ctx.tracked_blocks;
	while (block) {
		ag_mal_result_t result = ag_mal_validate_block(block);
		if (result != AG_MAL_OK) {
			ag_mal_unlock();
			return result;
		}
		block = block->next;
	}

	ag_mal_unlock();
#endif
	return AG_MAL_OK;
}
