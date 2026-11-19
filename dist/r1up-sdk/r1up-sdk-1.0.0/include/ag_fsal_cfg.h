/**
 * @file fsal_cfg.h
 * @brief File System Abstraction Layer (FSAL) - Configuration
 *
 * This header file contains compile-time configuration options for the
 * File System Abstraction Layer. The FSAL provides a unified interface
 * to different file system backends, enabling seamless portability between
 * desktop and embedded environments.
 *
 * @author Alex Giov
 * @date September 27, 2025
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2025, Race1Up Project.
 * All rights reserved.
 *
 * @par Supported Backends:
 * - Standard C Library (stdio.h) - Desktop/PC environments
 * - FatFS - Embedded systems with SD cards, flash memory
 * - Custom - User-defined implementation via function pointers
 *
 * @par Configuration Guide:
 * Choose the appropriate backend for your target platform:
 * - Set FSAL_CFG_BACKEND to 0 for desktop development
 * - Set FSAL_CFG_BACKEND to 1 for embedded systems with FatFS
 * - Set FSAL_CFG_BACKEND to 2 for custom implementations
 *
 * @warning Changing configuration requires recompilation of all dependent code.
 */

#ifndef AG_FSAL_CFG_H
#define AG_FSAL_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* BACKEND SELECTION                                                          */
/*============================================================================*/

/**
 * @defgroup FSAL_Backend_Config Backend Configuration
 * @brief File system backend selection and core settings
 * @{
 */

/**
 * @brief File system backend selection
 *
 * Select the underlying file system implementation:
 * - 0: Standard C Library (fopen, fread, fwrite, fclose)
 * - 1: FatFS Library (f_open, f_read, f_write, f_close)
 * - 2: Custom implementation (user-defined function pointers)
 */
#ifndef FSAL_CFG_BACKEND
#define FSAL_CFG_BACKEND 0
#endif

/**
 * @brief Library version information
 */
#define FSAL_VERSION_MAJOR 1
#define FSAL_VERSION_MINOR 0
#define FSAL_VERSION_PATCH 0
#define FSAL_VERSION_STRING "1.0.0"

/** @} */ /* End of FSAL_Backend_Config group */

/*============================================================================*/
/* FEATURE CONFIGURATION                                                      */
/*============================================================================*/

/**
 * @defgroup FSAL_Feature_Config Feature Configuration
 * @brief Enable/disable optional features
 * @{
 */

/**
 * @brief Enable debug mode compilation
 *
 * When enabled (1), the library includes additional debug information,
 * runtime checks, and diagnostic output. Disable (0) for production builds.
 */
#ifndef FSAL_CFG_DEBUG_ENABLED
#define FSAL_CFG_DEBUG_ENABLED 0
#endif

/**
 * @brief Enable parameter validation
 *
 * When enabled (1), all public API functions perform extensive parameter
 * validation. Disable (0) to improve performance if parameter correctness
 * is guaranteed by the application.
 */
#ifndef FSAL_CFG_PARAM_VALIDATION_ENABLED
#define FSAL_CFG_PARAM_VALIDATION_ENABLED 1
#endif

/**
 * @brief Enable thread safety features
 *
 * When enabled (1), the library includes synchronization mechanisms for
 * thread-safe operation. Disable (0) for single-threaded applications.
 */
#ifndef FSAL_CFG_THREAD_SAFE_ENABLED
#define FSAL_CFG_THREAD_SAFE_ENABLED 0
#endif

/**
 * @brief Enable file statistics collection
 *
 * When enabled (1), the library tracks file operation statistics like
 * bytes read/written, operation counts, etc. Useful for debugging.
 */
#ifndef FSAL_CFG_STATISTICS_ENABLED
#define FSAL_CFG_STATISTICS_ENABLED FSAL_CFG_DEBUG_ENABLED
#endif

/**
 * @brief Memory backend selection for internal FSAL allocations
 *
 * Selects which memory allocation system FSAL uses internally:
 * - 0: Standard C library (malloc/free from <stdlib.h>)
 * - 1: ag_mal Memory Abstraction Layer (ag_mal_alloc/ag_mal_free)
 *
 * When set to 1, FSAL will:
 * - Include ag_mal.h and depend on the ag_mal library
 * - Use ag_mal_alloc()/ag_mal_free() for all internal allocations
 * - Call ag_mal_init()/ag_mal_deinit() within fsal_init()/fsal_deinit()
 * - Benefit from memory leak detection, bounds checking and statistics
 *
 * @note When using ag_mal backend, ensure ag_mal is properly configured
 *       via ag_mal_cfg.h before compiling FSAL.
 */
#ifndef FSAL_CFG_MEMORY_BACKEND
#define FSAL_CFG_MEMORY_BACKEND 1
#endif

/** @} */ /* End of FSAL_Feature_Config group */

/*============================================================================*/
/* MEMORY CONFIGURATION                                                       */
/*============================================================================*/

/**
 * @defgroup FSAL_Memory_Config Memory Configuration
 * @brief Memory allocation and buffer size settings
 * @{
 */

/**
 * @brief Maximum number of simultaneously open files
 *
 * This setting determines how many files can be open at the same time.
 * Each open file consumes approximately 64 bytes of memory for its handle.
 *
 * @note Valid range: 1-255
 */
#ifndef FSAL_CFG_MAX_OPEN_FILES
#define FSAL_CFG_MAX_OPEN_FILES 16U
#endif

/**
 * @brief Maximum supported file path length
 *
 * This includes the full path, filename, and null terminator.
 * Must be sufficient for the target platform's maximum path length.
 *
 * @note Common values: Windows=260, Linux=4096, Embedded=64-256
 */
#ifndef FSAL_CFG_MAX_PATH_LENGTH
#define FSAL_CFG_MAX_PATH_LENGTH 260U
#endif

/**
 * @brief Internal buffer size for file operations
 *
 * Used for buffering during read/write operations to improve performance.
 * Larger buffers improve throughput but consume more memory.
 *
 * @note Should be a power of 2 for optimal performance
 */
#ifndef FSAL_CFG_BUFFER_SIZE
#define FSAL_CFG_BUFFER_SIZE 1024U
#endif

/** @} */ /* End of FSAL_Memory_Config group */

/*============================================================================*/
/* BACKEND-SPECIFIC CONFIGURATION                                             */
/*============================================================================*/

/**
 * @defgroup FSAL_Backend_Specific_Config Backend-Specific Configuration
 * @brief Configuration options specific to each backend
 * @{
 */

#if (FSAL_CFG_BACKEND == 1)
/**
 * @brief FatFS-specific configuration
 * These settings are only used when FSAL_CFG_BACKEND is set to 1 (FatFS).
 */

/**
 * @brief Maximum number of open files for FatFS backend
 *
 * FatFS has its own limit on simultaneously open files.
 * This should not exceed _FS_LOCK in your FatFS configuration.
 */
#ifndef FSAL_CFG_FATFS_MAX_OPEN_FILES
#define FSAL_CFG_FATFS_MAX_OPEN_FILES 4U
#endif

/**
 * @brief Enable FatFS long filename support
 *
 * Enable this if your FatFS configuration supports long filenames (LFN).
 */
#ifndef FSAL_CFG_FATFS_LFN_ENABLED
#define FSAL_CFG_FATFS_LFN_ENABLED 1
#endif

/**
 * @brief FatFS volume/drive prefix
 *
 * Default drive prefix for FatFS operations (e.g., "0:" for drive 0).
 * Set to empty string if not needed.
 */
#ifndef FSAL_CFG_FATFS_DRIVE_PREFIX
#define FSAL_CFG_FATFS_DRIVE_PREFIX "0:"
#endif

#elif (FSAL_CFG_BACKEND == 2)
/**
 * @brief Custom backend configuration
 * These settings are used when implementing a custom file system backend.
 */

/**
 * @brief Enable custom backend function pointer validation
 *
 * When enabled, the library validates that all required function pointers
 * are set before attempting to use them.
 */
#ifndef FSAL_CFG_CUSTOM_VALIDATION_ENABLED
#define FSAL_CFG_CUSTOM_VALIDATION_ENABLED 1
#endif

#endif /* Backend-specific configuration */

/** @} */ /* End of FSAL_Backend_Specific_Config group */

/*============================================================================*/
/* PERFORMANCE CONFIGURATION                                                  */
/*============================================================================*/

/**
 * @defgroup FSAL_Performance_Config Performance Configuration
 * @brief Performance tuning and optimization settings
 * @{
 */

/**
 * @brief File operation timeout in milliseconds
 *
 * Maximum time to wait for file I/O operations to complete.
 * Set to 0 to disable timeout (blocking I/O).
 */
#ifndef FSAL_CFG_OPERATION_TIMEOUT_MS
#define FSAL_CFG_OPERATION_TIMEOUT_MS 5000U
#endif

/**
 * @brief Enable read-ahead optimization
 *
 * When enabled (1), the library may perform read-ahead operations
 * to improve sequential read performance.
 */
#ifndef FSAL_CFG_READ_AHEAD_ENABLED
#define FSAL_CFG_READ_AHEAD_ENABLED 0
#endif

/**
 * @brief Enable write-behind optimization
 *
 * When enabled (1), the library may buffer write operations
 * to improve write performance.
 */
#ifndef FSAL_CFG_WRITE_BEHIND_ENABLED
#define FSAL_CFG_WRITE_BEHIND_ENABLED 0
#endif

/**
 * @brief Enable read buffering with read-ahead
 *
 * When enabled (1), FSAL maintains per-file buffers for read operations.
 * This significantly improves performance for repeated small reads on the
 * same file by reducing underlying I/O operations.
 *
 * Memory impact: Each file with buffering enabled allocates a buffer
 * of size FSAL_CFG_READ_BUFFER_SIZE bytes.
 *
 * @note Buffering must be explicitly enabled per file handle using
 * fsal_enable_buffering() after opening the file.
 */
#ifndef FSAL_CFG_READ_BUFFERING_ENABLED
#define FSAL_CFG_READ_BUFFERING_ENABLED 1
#endif

#if FSAL_CFG_READ_BUFFERING_ENABLED

/**
 * @brief Default read buffer size in bytes
 *
 * Size of the internal buffer used for read-ahead operations.
 * Larger buffers improve throughput for sequential access but consume
 * more memory. Should be aligned with underlying storage sector size.
 *
 * Recommended values:
 * - Embedded (SD card): 512-2048 bytes (sector-aligned)
 * - Desktop/SSD: 4096-8192 bytes (page-aligned)
 * - High-performance: 16384-32768 bytes
 *
 * @note Must be a power of 2 for optimal performance
 */
#ifndef FSAL_CFG_READ_BUFFER_SIZE
#define FSAL_CFG_READ_BUFFER_SIZE 4096U
#endif

/**
 * @brief Sequential read threshold for adaptive mode
 *
 * Number of consecutive sequential reads required before activating
 * aggressive read-ahead in FSAL_BUFFER_ADAPTIVE mode.
 *
 * Lower values activate read-ahead sooner but may waste bandwidth
 * on false positives. Higher values are more conservative.
 */
#ifndef FSAL_CFG_READAHEAD_THRESHOLD
#define FSAL_CFG_READAHEAD_THRESHOLD 3U
#endif

/**
 * @brief Read-ahead size multiplier for adaptive mode
 *
 * In adaptive mode, when sequential access is detected, FSAL reads
 * (multiplier × requested_size) bytes to fill the buffer.
 *
 * Example: If user requests 256 bytes and multiplier is 4,
 * FSAL will read 1024 bytes (up to buffer capacity).
 */
#ifndef FSAL_CFG_READAHEAD_MULTIPLIER
#define FSAL_CFG_READAHEAD_MULTIPLIER 4U
#endif

/**
 * @brief Maximum tolerated seek distance for sequential detection
 *
 * Maximum distance (in bytes) between consecutive reads that is still
 * considered sequential. Reads within this distance maintain the
 * sequential counter, larger jumps reset it.
 *
 * Set to 0 to require strictly consecutive reads.
 */
#ifndef FSAL_CFG_SEQUENTIAL_TOLERANCE
#define FSAL_CFG_SEQUENTIAL_TOLERANCE 0U
#endif

#endif /* FSAL_CFG_READ_BUFFERING_ENABLED */

/** @} */ /* End of FSAL_Performance_Config group */

/*============================================================================*/
/* VALIDATION AND SAFETY CHECKS                                              */
/*============================================================================*/

/**
 * @defgroup FSAL_Validation_Config Validation Configuration
 * @brief Compile-time validation of configuration parameters
 * @{
 */

/* Backend validation */
#if (FSAL_CFG_BACKEND < 0) || (FSAL_CFG_BACKEND > 2)
#error "FSAL_CFG_BACKEND must be 0, 1, or 2"
#endif

/* Memory configuration validation */
#if (FSAL_CFG_MAX_OPEN_FILES < 1U) || (FSAL_CFG_MAX_OPEN_FILES > 255U)
#error "FSAL_CFG_MAX_OPEN_FILES must be between 1 and 255"
#endif

#if (FSAL_CFG_MAX_PATH_LENGTH < 16U) || (FSAL_CFG_MAX_PATH_LENGTH > 8192U)
#error "FSAL_CFG_MAX_PATH_LENGTH must be between 16 and 8192 characters"
#endif

#if (FSAL_CFG_BUFFER_SIZE < 64U) || (FSAL_CFG_BUFFER_SIZE > 65536U)
#error "FSAL_CFG_BUFFER_SIZE must be between 64 and 65536 bytes"
#endif

/* Backend-specific validation */
#if (FSAL_CFG_BACKEND == 1)
#if (FSAL_CFG_FATFS_MAX_OPEN_FILES > FSAL_CFG_MAX_OPEN_FILES)
#error "FSAL_CFG_FATFS_MAX_OPEN_FILES cannot exceed FSAL_CFG_MAX_OPEN_FILES"
#endif
#endif

/* Read buffering validation */
#if FSAL_CFG_READ_BUFFERING_ENABLED
#if (FSAL_CFG_READ_BUFFER_SIZE < 64U) || (FSAL_CFG_READ_BUFFER_SIZE > 65536U)
#error "FSAL_CFG_READ_BUFFER_SIZE must be between 64 and 65536 bytes"
#endif

#if (FSAL_CFG_READAHEAD_THRESHOLD < 1U) || (FSAL_CFG_READAHEAD_THRESHOLD > 100U)
#error "FSAL_CFG_READAHEAD_THRESHOLD must be between 1 and 100"
#endif

#if (FSAL_CFG_READAHEAD_MULTIPLIER < 1U) || (FSAL_CFG_READAHEAD_MULTIPLIER > 16U)
#error "FSAL_CFG_READAHEAD_MULTIPLIER must be between 1 and 16"
#endif
#endif

/** @} */ /* End of FSAL_Validation_Config group */

/*============================================================================*/
/* DERIVED CONFIGURATION                                                      */
/*============================================================================*/

/**
 * @defgroup FSAL_Derived_Config Derived Configuration
 * @brief Automatically calculated configuration values
 * @{
 */

/**
 * @brief Configuration signature for compatibility checking
 *
 * This value changes when configuration parameters are modified.
 * Can be used to detect configuration mismatches.
 */
#define FSAL_CFG_SIGNATURE                                                                                          \
	\\\n((FSAL_CFG_BACKEND << 24) | \\\n(FSAL_CFG_MAX_OPEN_FILES << 16) | \\\n(FSAL_CFG_DEBUG_ENABLED << 8) | \\\n( \
			 FSAL_CFG_THREAD_SAFE_ENABLED << 4) | \\\n(FSAL_VERSION_MAJOR << 2) | \\\n FSAL_VERSION_MINOR)

/**
 * @brief Estimated memory usage in bytes
 *
 * Approximate memory footprint based on current configuration.
 */
#define FSAL_CFG_ESTIMATED_MEMORY_USAGE \
	\\\n((FSAL_CFG_MAX_OPEN_FILES *     \
		  64U) + \\\n FSAL_CFG_BUFFER_SIZE + \\\n(FSAL_CFG_STATISTICS_ENABLED ? (FSAL_CFG_MAX_OPEN_FILES * 32U) : 0U))

/** @} */ /* End of FSAL_Derived_Config group */

#ifdef __cplusplus
}
#endif

#endif /* AG_FSAL_CFG_H */

/**
 * @defgroup FSAL_Configuration FSAL Configuration System
 * @brief Complete configuration system for File System Abstraction Layer
 *
 * The FSAL configuration system provides fine-grained control over:
 *
 * @par Backend Selection:
 * - Standard C Library for desktop development
 * - FatFS for embedded systems
 * - Custom implementations via function pointers
 *
 * @par Feature Control:
 * - Debug and diagnostic capabilities
 * - Parameter validation and safety checks
 * - Thread safety mechanisms
 * - Performance optimizations
 *
 * @par Memory Management:
 * - Buffer sizes and limits
 * - Resource allocation constraints
 * - Platform-specific optimizations
 *
 * @par Usage Guidelines:
 * 1. Choose appropriate backend for your target platform
 * 2. Enable debug features during development
 * 3. Optimize memory usage for embedded systems
 * 4. Test configuration changes thoroughly
 * 5. Document custom configuration choices
 *
 * @warning Configuration changes affect API compatibility and require
 * recompilation of all dependent code.
 */