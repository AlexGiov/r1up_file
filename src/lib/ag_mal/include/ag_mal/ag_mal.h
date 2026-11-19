// --------------------------------------------------------------------------------------------------------------------
/// \file ag_mal.h
///
/// \brief Memory Abstraction Layer Public API
///
/// This file defines the public interface for the Memory Abstraction Layer (MAL).
/// The MAL provides a unified interface for memory allocation that can work with
/// different backends (standard C malloc/free or FreeRTOS) while offering advanced
/// features like memory tracking, leak detection, and bounds checking.
///
/// \par Key Features:
/// - Backend abstraction (Standard C ↔ FreeRTOS)
/// - Memory leak detection and reporting
/// - Buffer overflow/underflow protection
/// - Detailed memory usage statistics
/// - Thread-safe operations
/// - Debug information with file/line tracking
///
/// \par Usage Example:
/// \code{.c}
/// #include "ag_mal.h"
///
/// int main(void) {
///     // Initialize the MAL
///     ag_mal_init();
///     
///     // Allocate memory
///     char *buffer = AG_MALLOC(1024);
///     if (buffer) {
///         // Use the buffer
///         strcpy(buffer, "Hello MAL!");
///         
///         // Free the memory
///         AG_FREE(buffer);
///     }
///     
///     // Check for leaks
///     if (ag_mal_check_leaks() == 0) {
///         printf("No memory leaks detected!\n");
///     }
///     
///     // Clean up
///     ag_mal_deinit();
///     return 0;
/// }
/// \endcode
///
/// \author ASK Industries GmbH
/// \version 1.0.0
/// \date 2025-09-28
///
/// Copyright (c) 2025, ASK Industries GmbH, Niederwinkling.
/// All Rights Reserved.
// --------------------------------------------------------------------------------------------------------------------

#ifndef AG_MAL_H
#define AG_MAL_H

// ====================================================================================================================
//  INCLUDES
// ====================================================================================================================

#include <stddef.h>
#include <stdint.h>
#include "ag_mal_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

// ====================================================================================================================
//  TYPE DEFINITIONS
// ====================================================================================================================

/// \defgroup MAL_Types Type Definitions
/// \{

/// \brief MAL function return codes
///
/// All MAL functions that can fail return one of these result codes.
/// Check the return value to ensure operations completed successfully.
typedef enum {
    AG_MAL_OK = 0,                    ///< Operation completed successfully
    AG_MAL_ERROR_NULL_POINTER,        ///< NULL pointer passed as parameter
    AG_MAL_ERROR_INVALID_SIZE,        ///< Invalid size parameter (e.g., zero or too large)
    AG_MAL_ERROR_OUT_OF_MEMORY,       ///< Memory allocation failed (out of memory)
    AG_MAL_ERROR_CORRUPTION,          ///< Memory corruption detected (guard bytes modified)
    AG_MAL_ERROR_NOT_FOUND,           ///< Memory block not found in tracking list
    AG_MAL_ERROR_LIMIT_EXCEEDED       ///< Maximum number of tracked allocations exceeded
} ag_mal_result_t;

/// \brief Memory usage statistics structure
///
/// This structure contains detailed information about memory usage patterns.
/// Use ag_mal_get_stats() to populate this structure with current statistics.
typedef struct {
    size_t total_allocated;      ///< Total bytes currently allocated and not freed
    size_t peak_allocated;       ///< Peak memory usage in bytes since initialization
    size_t num_allocations;      ///< Current number of active (not freed) allocations
    size_t total_allocations;    ///< Total number of allocation requests since init
    size_t total_deallocations;  ///< Total number of free requests since initialization
    size_t failed_allocations;   ///< Number of allocation requests that failed
} ag_mal_stats_t;

/// \brief Memory block information for debugging
///
/// Contains detailed information about a specific memory allocation.
/// Used internally for tracking and debugging purposes.
typedef struct {
    void *ptr;              ///< Pointer to the allocated memory block
    size_t size;            ///< Size of the allocation in bytes
    const char *file;       ///< Source file where allocation was made (debug builds)
    int line;               ///< Source line where allocation was made (debug builds)
    uint32_t timestamp;     ///< Timestamp when allocation was made
} ag_mal_block_info_t;

/// \}

// ====================================================================================================================
//  PUBLIC API FUNCTIONS
// ====================================================================================================================

/// \defgroup MAL_Init Initialization and Cleanup
/// \{

/// \brief Initialize the Memory Abstraction Layer
///
/// This function must be called before using any other MAL functions.
/// It initializes internal data structures, statistics counters, and thread
/// synchronization primitives (if enabled).
///
/// The function supports multiple initializations - each call increments
/// an internal reference counter, and ag_mal_deinit() must be called
/// the same number of times to fully deinitialize.
///
/// \return AG_MAL_OK on success, AG_MAL_ERROR_OUT_OF_MEMORY if initialization fails
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// if (ag_mal_init() != AG_MAL_OK) {
///     printf("Failed to initialize MAL\n");
///     return -1;
/// }
/// \endcode
ag_mal_result_t ag_mal_init(void);

/// \brief Deinitialize the Memory Abstraction Layer
///
/// This function should be called when the MAL is no longer needed.
/// It decrements the internal reference counter and, when it reaches zero,
/// performs cleanup including:
/// - Freeing any remaining tracked allocations
/// - Destroying thread synchronization primitives
/// - Resetting internal state
///
/// \return AG_MAL_OK on success, error code otherwise
///
/// \warning Any memory still allocated when the reference counter reaches
///          zero will be automatically freed, which may indicate a memory leak.
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// // Check for leaks before cleanup
/// if (ag_mal_check_leaks() > 0) {
///     ag_mal_print_report();
/// }
/// ag_mal_deinit();
/// \endcode
ag_mal_result_t ag_mal_deinit(void);

/// \}

/// \defgroup MAL_Alloc Memory Allocation Functions
/// \{

/// \brief Allocate memory with debug information
///
/// This is the low-level allocation function that includes debug information.
/// Typically not called directly - use the ag_mal_alloc() macro instead.
///
/// The function allocates the requested memory plus overhead for:
/// - Internal tracking structure (if tracking enabled)
/// - Guard bytes for overflow detection (if bounds checking enabled)
///
/// \param size Size in bytes to allocate (must be > 0)
/// \param file Source file name for debugging (can be NULL)
/// \param line Source line number for debugging (can be 0)
/// \return Pointer to allocated memory, or NULL on failure
///
/// \note The returned pointer points to usable memory, not the internal structure
/// \note Guard bytes (if enabled) are automatically initialized
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
void *ag_mal_alloc_debug(size_t size, const char *file, int line);

/// \brief Free previously allocated memory with debug information
///
/// This is the low-level deallocation function that includes debug information.
/// Typically not called directly - use the ag_mal_free() macro instead.
///
/// The function performs the following operations:
/// - Validates the pointer is in the tracking list (if tracking enabled)
/// - Checks guard bytes for corruption (if bounds checking enabled)
/// - Updates statistics counters
/// - Frees the memory including internal overhead
///
/// \param ptr Pointer to memory to free (must be previously allocated by MAL)
/// \param file Source file name for debugging (can be NULL)
/// \param line Source line number for debugging (can be 0)
/// \return AG_MAL_OK on success, error code on failure
///
/// \retval AG_MAL_ERROR_NULL_POINTER if ptr is NULL
/// \retval AG_MAL_ERROR_NOT_FOUND if ptr was not allocated by MAL
/// \retval AG_MAL_ERROR_CORRUPTION if guard bytes indicate corruption
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
ag_mal_result_t ag_mal_free_debug(void *ptr, const char *file, int line);

/// \brief Reallocate memory with debug information
///
/// This is the low-level reallocation function that includes debug information.
/// Typically not called directly - use the ag_mal_realloc() macro instead.
///
/// The function behaves similarly to standard realloc():
/// - If ptr is NULL, behaves like ag_mal_alloc()
/// - If new_size is 0, behaves like ag_mal_free() and returns NULL
/// - Otherwise, allocates new memory, copies data, and frees old memory
///
/// \param ptr Pointer to previously allocated memory (can be NULL)
/// \param new_size New size in bytes (0 to free)
/// \param file Source file name for debugging (can be NULL)
/// \param line Source line number for debugging (can be 0)
/// \return Pointer to reallocated memory, or NULL on failure/free
///
/// \note Data is preserved up to the minimum of old and new sizes
/// \note The returned pointer may be different from the input pointer
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
void *ag_mal_realloc_debug(void *ptr, size_t new_size, const char *file, int line);

/// \}

/// \defgroup MAL_Stats Statistics and Monitoring
/// \{

/// \brief Get current memory usage statistics
///
/// Fills the provided statistics structure with current memory usage information.
/// The statistics are collected in real-time and provide detailed insights into
/// memory allocation patterns.
///
/// \param stats Pointer to ag_mal_stats_t structure to fill (must not be NULL)
/// \return AG_MAL_OK on success, AG_MAL_ERROR_NULL_POINTER if stats is NULL
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
/// The returned statistics represent a consistent snapshot at the time of the call.
///
/// \par Example:
/// \code{.c}
/// ag_mal_stats_t stats;
/// if (ag_mal_get_stats(&stats) == AG_MAL_OK) {
///     printf("Current usage: %zu bytes\n", stats.total_allocated);
///     printf("Peak usage: %zu bytes\n", stats.peak_allocated);
///     printf("Active allocations: %zu\n", stats.num_allocations);
/// }
/// \endcode
ag_mal_result_t ag_mal_get_stats(ag_mal_stats_t *stats);

/// \brief Check for memory leaks
///
/// Returns the number of currently active (not freed) allocations.
/// A return value of 0 indicates no memory leaks.
///
/// This is a quick way to check for memory leaks without generating
/// a detailed report. For more information about the leaks, use
/// ag_mal_print_report().
///
/// \return Number of active allocations (0 = no leaks)
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// size_t leaks = ag_mal_check_leaks();
/// if (leaks > 0) {
///     printf("Warning: %zu memory leaks detected!\n", leaks);
///     ag_mal_print_report();
/// }
/// \endcode
size_t ag_mal_check_leaks(void);

/// \brief Return the current init reference count
///
/// Returns the number of outstanding ag_mal_init() calls that have not
/// yet been matched by ag_mal_deinit().  Useful to determine whether
/// other users still hold the library open before asserting zero leaks.
///
/// \return Current init_count value (0 = fully deinitialised)
uint32_t ag_mal_get_init_count(void);

/// \brief Print detailed memory usage report
///
/// Prints a comprehensive report of current memory usage to stdout.
/// The report includes:
/// - Overall statistics (total allocated, peak usage, etc.)
/// - List of all active allocations with details (if tracking enabled)
/// - File and line information for each allocation (if debug enabled)
///
/// This function only produces output when AG_MAL_DEBUG_ENABLED is true.
/// In release builds, the function does nothing.
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
/// However, output may be interleaved if multiple threads call it simultaneously.
///
/// \par Example Output:
/// \code
/// === Memory Abstraction Layer Report ===
/// Total allocated: 1024 bytes
/// Peak allocated: 2048 bytes
/// Active allocations: 2
/// Total allocations: 5
/// Total deallocations: 3
/// Failed allocations: 0
///
/// === Active Allocations ===
/// Ptr: 0x7f8b4c000980, Size: 512, File: main.c:45, Timestamp: 1234
/// Ptr: 0x7f8b4c001200, Size: 512, File: utils.c:78, Timestamp: 1267
/// \endcode
void ag_mal_print_report(void);

/// \brief Validate integrity of all tracked allocations
///
/// Performs validation checks on all currently tracked memory allocations.
/// This includes:
/// - Checking guard bytes for corruption (if bounds checking enabled)
/// - Verifying internal data structure integrity
/// - Detecting any inconsistencies in the tracking system
///
/// This function is useful for detecting memory corruption that may not
/// be immediately apparent during normal operations.
///
/// \return AG_MAL_OK if all allocations are valid, AG_MAL_ERROR_CORRUPTION if corruption detected
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// if (ag_mal_validate_all() != AG_MAL_OK) {
///     printf("Memory corruption detected!\n");
///     ag_mal_print_report();
///     // Handle corruption...
/// }
/// \endcode
ag_mal_result_t ag_mal_validate_all(void);

/// \}

// ====================================================================================================================
//  CONVENIENCE MACROS
// ====================================================================================================================

/// \defgroup MAL_Macros Convenience Macros
/// \{

/// \brief Allocate memory with automatic debug info
///
/// In debug builds (AG_MAL_DEBUG_ENABLED=1), automatically passes __FILE__ and __LINE__.
/// In release builds, passes NULL and 0 for better performance.
///
/// \param size Size in bytes to allocate
/// \return Pointer to allocated memory or NULL on failure
#if AG_MAL_DEBUG_ENABLED
    #define ag_mal_alloc(size)           ag_mal_alloc_debug(size, __FILE__, __LINE__)
#else
    #define ag_mal_alloc(size)           ag_mal_alloc_debug(size, NULL, 0)
#endif

/// \brief Free memory with automatic debug info
///
/// In debug builds (AG_MAL_DEBUG_ENABLED=1), automatically passes __FILE__ and __LINE__.
/// In release builds, passes NULL and 0 for better performance.
///
/// \param ptr Pointer to memory to free
/// \return AG_MAL_OK on success, error code on failure
#if AG_MAL_DEBUG_ENABLED
    #define ag_mal_free(ptr)             ag_mal_free_debug(ptr, __FILE__, __LINE__)
#else
    #define ag_mal_free(ptr)             ag_mal_free_debug(ptr, NULL, 0)
#endif

/// \brief Reallocate memory with automatic debug info
///
/// In debug builds (AG_MAL_DEBUG_ENABLED=1), automatically passes __FILE__ and __LINE__.
/// In release builds, passes NULL and 0 for better performance.
///
/// \param ptr Pointer to previously allocated memory
/// \param size New size in bytes
/// \return Pointer to reallocated memory or NULL on failure
#if AG_MAL_DEBUG_ENABLED
    #define ag_mal_realloc(ptr, size)    ag_mal_realloc_debug(ptr, size, __FILE__, __LINE__)
#else
    #define ag_mal_realloc(ptr, size)    ag_mal_realloc_debug(ptr, size, NULL, 0)
#endif

/// \}

/// \defgroup MAL_Compat Compatibility Macros
/// These macros provide easy migration from standard memory functions
/// \{

/// \brief Drop-in replacement for malloc()
/// \param size Size in bytes to allocate
/// \return Pointer to allocated memory or NULL on failure
#define AG_MALLOC(size)       ag_mal_alloc(size)

/// \brief Drop-in replacement for free()
/// \param ptr Pointer to memory to free
#define AG_FREE(ptr)          ag_mal_free(ptr)

/// \brief Drop-in replacement for realloc()
/// \param ptr Pointer to previously allocated memory
/// \param size New size in bytes
/// \return Pointer to reallocated memory or NULL on failure
#define AG_REALLOC(ptr, size) ag_mal_realloc(ptr, size)

/// \brief Drop-in replacement for calloc()
/// \param num Number of elements
/// \param size Size of each element
/// \return Pointer to allocated and zeroed memory or NULL on failure
#define AG_CALLOC(num, size)  ag_mal_calloc(num, size)

/// \}

/// \defgroup MAL_Utility Utility Functions
/// \{

/// \brief Allocate and zero-initialize memory
///
/// Equivalent to standard calloc() function. Allocates memory for an array
/// of num elements, each of size bytes, and initializes all bytes to zero.
///
/// The total allocation size is num * size bytes. If this multiplication
/// would overflow, the function returns NULL.
///
/// \param num Number of elements to allocate
/// \param size Size of each element in bytes
/// \return Pointer to allocated and zero-initialized memory, or NULL on failure
///
/// \par Thread Safety:
/// This function is thread-safe when AG_MAL_ENABLE_THREAD_SAFE is enabled.
///
/// \par Example:
/// \code{.c}
/// // Allocate array of 10 integers, all initialized to 0
/// int *array = (int*)ag_mal_calloc(10, sizeof(int));
/// if (array) {
///     // All elements are guaranteed to be 0
///     assert(array[0] == 0);
///     assert(array[9] == 0);
///     AG_FREE(array);
/// }
/// \endcode
void *ag_mal_calloc(size_t num, size_t size);

/// \}

#ifdef __cplusplus
}
#endif

#endif /* AG_MAL_H */
