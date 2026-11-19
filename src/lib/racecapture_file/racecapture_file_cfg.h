/**
 * @file racecapture_file_cfg.h
 * @brief RaceCapture File Writer Library - Configuration
 *
 * @author Alex Giov
 * @date January 23, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 */

#ifndef RACECAPTURE_FILE_CFG_H
#define RACECAPTURE_FILE_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* CONFIGURATION OPTIONS                                                      */
/*============================================================================*/

/**
 * @brief Maximum number of channels allowed in a RaceCapture log file
 * @details Adjust this based on your application requirements.
 *          Higher values consume more memory but allow more channels.
 *
 * @note R1UP to RaceCapture uses 26 channels (2 system + 24 data)
 * @note Typical RaceCapture files have 20-40 channels
 */
#define RACECAPTURE_FILE_MAX_CHANNELS 64U

/**
 * @brief Default base sample rate in Hz
 * @details This is the highest sample rate in the file.
 *          All other channel rates must be divisors of this base rate.
 *
 * @note For R1UP: base rate is 10 Hz (100 ms period)
 * @note This can be overridden per-file via API (future enhancement)
 */
#define RACECAPTURE_FILE_DEFAULT_BASE_RATE_HZ 10U

/**
 * @brief Buffer size for formatting channel names and units
 * @details Maximum length for channel name or units string.
 */
#define RACECAPTURE_FILE_MAX_NAME_LENGTH 32U

/**
 * @brief Buffer size for formatting numeric values
 * @details Internal buffer for sprintf formatting of double values.
 */
#define RACECAPTURE_FILE_VALUE_BUFFER_SIZE 64U

#ifdef __cplusplus
}
#endif

#endif /* RACECAPTURE_FILE_CFG_H */
