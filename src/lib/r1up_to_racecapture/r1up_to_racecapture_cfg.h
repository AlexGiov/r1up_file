/**
 * @file r1up_to_racecapture_cfg.h
 * @brief R1UP to RaceCapture Converter - Channel Mapping Configuration
 * @details Defines the mapping between R1UP telemetry variables and RaceCapture CSV channels
 *
 * @author Alex Giov
 * @date January 23, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 *
 * @par Channel Mapping Strategy:
 * - **System Channels (2):** Interval and Utc are calculated by the adapter
 * - **Data Channels (24):** Mapped from R1UP fixed-point variables
 * - **GPS Special:** Latitude/Longitude require conversion from 6 R1UP vars to 2 RC channels
 * - **Total:** 26 RaceCapture channels from 28 R1UP variables
 *
 * @warning THE ORDER OF ENTRIES IN THIS TABLE IS CRITICAL
 *          It defines the exact column order in the RaceCapture CSV file.
 *          Do not change the order unless you understand the implications for Track Attack compatibility.
 */

#ifndef R1UP_TO_RACECAPTURE_CFG_H
#define R1UP_TO_RACECAPTURE_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

#include "r1up_file.h"

/*============================================================================*/
/* CONFIGURATION                                                              */
/*============================================================================*/

/**
 * @brief Base sample rate for R1UP telemetry system in Hz
 * @details This is defined in R1UP_FILE_DFLT_SMPL_FRQ (typically 10 Hz = 100ms period)
 */
#define R1UP_TO_RC_BASE_RATE_HZ R1UP_FILE_DFLT_SMPL_FRQ

/**
 * @brief Base period in milliseconds
 */
#define R1UP_TO_RC_BASE_PERIOD_MS (1000U / R1UP_TO_RC_BASE_RATE_HZ)

/**
 * @brief Max file name length for RaceCapture output (including null terminator)
 */
#define R1UP_TO_RC_FILENAME_MAX_LEN 64U

/**
 * @brief Max file path length for RaceCapture output (including directory and null terminator)
 */
#define R1UP_TO_RC_FILEPATH_MAX_LEN 256U

/**
 * @brief Log file extention for RaceCapture output
 */
#define R1UP_TO_RC_OUTPUT_FILE_EXTENSION ".log"

/*============================================================================*/
/* CHANNEL MAPPING TABLE (X-MACRO)                                           */
/*============================================================================*/

/**
 * @brief R1UP to RaceCapture channel mapping table
 * @details This X-macro table defines all 26 RaceCapture channels and their mapping to R1UP data.
 *
 * @par X-Macro Parameters:
 * @param CHANNEL_TYPE - Type of channel:
 *                       - SYSTEM_INTERVAL: Calculated interval (ms uptime)
 *                       - SYSTEM_UTC: Calculated UTC timestamp
 *                       - DATA_SIMPLE: Direct R1UP variable conversion
 *                       - DATA_SCALED: R1UP variable with unit scaling
 *                       - DATA_GPS_LAT: GPS latitude (from 3 R1UP vars)
 *                       - DATA_GPS_LON: GPS longitude (from 3 R1UP vars)
 *
 * @param R1UP_VAR_ID - R1UP variable ID from R1UP_FILE_VARS_STREAM_FORMAT_TABLE
 *                      (or 0 for system channels)
 *
 * @param RC_NAME - RaceCapture channel name (string)
 * @param RC_UNITS - RaceCapture units label (string, or "" for unitless)
 * @param RC_MIN - Minimum value (metadata)
 * @param RC_MAX - Maximum value (metadata)
 * @param RC_HZ - Sample rate in Hz (must be divisor of base rate)
 * @param RC_PRECISION - Decimal places for formatting
 * @param UNIT_SCALE - Multiplicative scaling factor (1.0 for no scaling)
 *
 * @note GPS channels use special conversion functions from r1up_file.c
 * @note Gyro channels are scaled from rad/s to deg/s (× 57.2958)
 * @note Order matches RaceCapture Track Attack expected format
 */
// #define R1UP_TO_RC_MAPPING_TABLE(ENTRY)                                                                     \
// 	/* CHAN_TYPE,        R1UP_VAR_ID,     RC_NAME,     RC_UNITS, RC_MIN, RC_MAX, RC_HZ, RC_PREC, SCALE   */ \
//                                                                                                             \
// 	/* ========== SYSTEM CHANNELS (2) - Calculated by adapter ========== */                                 \
// 	ENTRY(SYSTEM_INTERVAL, 0, "Interval", "ms", 0, 0, 1, 0, 1.0)                                            \
// 	ENTRY(SYSTEM_UTC, 0, "Utc", "ms", 0, 0, 1, 0, 1.0)                                                      \
//                                                                                                             \
// 	/* ========== DATA CHANNELS (24) - From R1UP telemetry ========== */                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_RPM, "rpm", "RPM", 0, 12000, 10, 0, 1.0)                                          \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_EXAUST_TEMP, "exaust", "C", 0, 1000, 10, 1, 1.0)                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_COOLANT_TEMP, "coolant", "C", -40, 150, 1, 1, 1.0)                                   \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_X_ACC, "xacc", "G", -5, 5, 10, 2, 1.0)                                            \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Y_ACC, "yacc", "G", -5, 5, 10, 2, 1.0)                                            \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Z_ACC, "zacc", "G", -5, 5, 10, 2, 1.0)                                            \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_ROLL, "rool", "Deg", -180, 180, 10, 1, 1.0)                                     \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_PITCH, "pitch", "Deg", -180, 180, 10, 1, 1.0)                                   \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_YAW, "yaw", "Deg", -180, 180, 10, 1, 1.0)                                       \
// 	ENTRY(DATA_SCALED, R1UP_FILE_VAR_ID_X_GYRO, "x_gyro", "Deg/s", -500, 500, 10, 1, 57.2958)                         \
// 	ENTRY(DATA_SCALED, R1UP_FILE_VAR_ID_Y_GYRO, "y_gyro", "Deg/s", -500, 500, 10, 1, 57.2958)                         \
// 	ENTRY(DATA_SCALED, R1UP_FILE_VAR_ID_Z_GYRO, "z_gyro", "Deg/s", -500, 500, 10, 1, 57.2958)                         \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GPS_SPEED, "gps_speed", "km/h", 0, 350, 10, 1, 1.0)                                 \
// 	ENTRY(DATA_GPS_LAT, R1UP_FILE_VAR_ID_LAT_DEG, "latitude", "Deg", -90, 90, 10, 6, 1.0)                                 \
// 	ENTRY(DATA_GPS_LON, R1UP_FILE_VAR_ID_LONG_DEG, "longitude", "Deg", -180, 180, 10, 6, 1.0)                             \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GEAR, "gear", "", -1, 8, 10, 0, 1.0)                                              \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_THROTTLE, "throttle", "%", 0, 100, 10, 1, 1.0)                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_ALTITUDE, "altitude", "m", -500, 5000, 10, 1, 1.0)                                 \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GPS_SAT_COUNT, "gps_sat", "", 0, 20, 10, 0, 1.0)                                   \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_INT_TEMP, "intTemp", "C", -40, 125, 1, 1, 1.0)                                     \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_BATT_VOLT, "battVolt", "V", 0, 20, 1, 2, 1.0)                                      \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_EXT_PS_VOLT, "extps", "V", 0, 20, 1, 2, 1.0)                                      \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_RPM_HALL, "rpm_hall", "RPM", 0, 12000, 10, 0, 1.0)                                \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_PWSS_HALL, "pwss_hall", "Hz", 0, 500, 10, 1, 1.0)

// #define R1UP_TO_RC_MAPPING_TABLE(ENTRY)                                                                     \
// 	/* CHAN_TYPE,        R1UP_VAR_ID,     RC_NAME,     RC_UNITS, RC_MIN, RC_MAX, RC_HZ, RC_PREC, SCALE   */ \
//                                                                                                             \
// 	/* ========== SYSTEM CHANNELS (2) - Calculated by adapter ========== */                                 \
// 	ENTRY(SYSTEM_INTERVAL, 0, "Interval", "ms", 0, 0, 10, 0, 1.0)                                           \
// 	ENTRY(SYSTEM_UTC, 0, "Utc", "ms", 0, 0, 10, 0, 1.0)                                                     \
//                                                                                                             \
// 	/* ========== DATA CHANNELS (24) - From R1UP telemetry ========== */                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_RPM, "rpm", "RPM", 0, 12000, 10, 0, 1.0)                                          \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_EXAUST_TEMP, "exaust", "C", 0, 1000, 10, 1, 1.0)                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_COOLANT_TEMP, "coolant", "C", -40, 150, 10, 1, 1.0)                                  \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_X_ACC, "xacc", "G", -5, 5, 10, 2, 1.0)                                            \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Y_ACC, "yacc", "G", -5, 5, 10, 2, 1.0)                                            \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Z_ACC, "zacc", "G", -5, 5, 10, 2, 1.0)                                            \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_ROLL, "rool", "Deg", -180, 180, 10, 1, 1.0)                                     \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_PITCH, "pitch", "Deg", -180, 180, 10, 1, 1.0)                                   \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_YAW, "yaw", "Deg", -180, 180, 10, 1, 1.0)                                       \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GPS_SPEED, "gps_speed", "km/h", 0, 350, 10, 1, 1.0)                                 \
// 	ENTRY(DATA_GPS_LAT, R1UP_FILE_VAR_ID_LAT_DEG, "latitude", "Deg", -90, 90, 10, 6, 1.0)                                 \
// 	ENTRY(DATA_GPS_LON, R1UP_FILE_VAR_ID_LONG_DEG, "longitude", "Deg", -180, 180, 10, 6, 1.0)                             \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GEAR, "gear", "", -1, 8, 10, 0, 1.0)                                              \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_THROTTLE, "throttle", "%", 0, 100, 10, 1, 1.0)                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_ALTITUDE, "altitude", "m", -500, 5000, 10, 1, 1.0)                                 \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GPS_SAT_COUNT, "gps_sat", "", 0, 20, 10, 0, 1.0)                                   \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_INT_TEMP, "intTemp", "C", -40, 125, 10, 1, 1.0)                                    \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_BATT_VOLT, "battVolt", "V", 0, 20, 10, 2, 1.0)                                     \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_EXT_PS_VOLT, "extps", "V", 0, 20, 10, 2, 1.0)                                     \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_X_GYRO, "x_gyro", "Deg/s", -500, 500, 10, 1, 1.0)                             \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Y_GYRO, "y_gyro", "Deg/s", -500, 500, 10, 1, 1.0)                             \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Z_GYRO, "z_gyro", "Deg/s", -500, 500, 10, 1, 1.0)                             \
// 	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_RPM_HALL, "rpm_hall", "RPM", 0, 12000, 10, 0, 1.0)                                \
//	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_PWSS_HALL, "pwss_hall", "Hz", 0, 500, 10, 0, 1.0)

#define R1UP_TO_RC_MAPPING_TABLE(ENTRY)                                                                     \
	/* CHAN_TYPE,        R1UP_VAR_ID,     RC_NAME,     RC_UNITS, RC_MIN, RC_MAX, RC_HZ, RC_PREC, SCALE   */ \
                                                                                                            \
	/* ========== SYSTEM CHANNELS (2) - Calculated by adapter ========== */                                 \
	ENTRY(SYSTEM_INTERVAL, 0, "Interval", "ms", 0, 0, 10, 0, 1.0)                                           \
	ENTRY(SYSTEM_UTC, 0, "Utc", "ms", 0, 0, 10, 0, 1.0)                                                     \
                                                                                                            \
	/* ========== DATA CHANNELS (24) - From R1UP telemetry ========== */                                    \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_RPM, "rpm", "RPM", 0, 12000, 10, 0, 1.0)                            \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_EXAUST_TEMP, "exaust", "C", 0, 1000, 10, 1, 1.0)                    \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_COOLANT_TEMP, "coolant", "C", -40, 150, 10, 1, 1.0)                 \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_X_ACC, "xacc", "G", -5, 5, 10, 2, 1.0)                              \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Y_ACC, "yacc", "G", -5, 5, 10, 2, 1.0)                              \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Z_ACC, "zacc", "G", -5, 5, 10, 2, 1.0)                              \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_ROLL, "rool", "Deg", -180, 180, 10, 1, 1.0)                         \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_PITCH, "pitch", "Deg", -180, 180, 10, 1, 1.0)                       \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_YAW, "yaw", "Deg", -180, 180, 10, 1, 1.0)                           \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GPS_SPEED, "gps_speed", "km/h", 0, 350, 10, 1, 1.0)                 \
	ENTRY(DATA_GPS_LAT, R1UP_FILE_VAR_ID_LAT_DEG, "latitude", "Deg", -90, 90, 10, 6, 1.0)                   \
	ENTRY(DATA_GPS_LON, R1UP_FILE_VAR_ID_LONG_DEG, "longitude", "Deg", -180, 180, 10, 6, 1.0)               \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GEAR, "gear", "", -1, 8, 10, 0, 1.0)                                \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_THROTTLE, "throttle", "%", 0, 100, 10, 1, 1.0)                      \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_ALTITUDE, "altitude", "m", -500, 5000, 10, 1, 1.0)                  \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_GPS_SAT_COUNT, "gps_sat", "", 0, 20, 10, 0, 1.0)                    \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_INT_TEMP, "intTemp", "C", -40, 125, 10, 1, 1.0)                     \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_BATT_VOLT, "battVolt", "V", 0, 20, 10, 2, 1.0)                      \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_EXT_PS_VOLT, "extps", "V", 0, 20, 10, 2, 1.0)                       \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_X_GYRO, "x_gyro", "Deg/s", -500, 500, 10, 1, 1.0)                   \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Y_GYRO, "y_gyro", "Deg/s", -500, 500, 10, 1, 1.0)                   \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_Z_GYRO, "z_gyro", "Deg/s", -500, 500, 10, 1, 1.0)                   \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_RPM_HALL, "rpm_hall", "RPM", 0, 12000, 10, 0, 1.0)                  \
	ENTRY(DATA_SIMPLE, R1UP_FILE_VAR_ID_PWSS_HALL, "pwss_hall", "Hz", 0, 500, 10, 0, 1.0)

/**
 * @brief Channel type enumeration
 */
typedef enum {
	SYSTEM_INTERVAL,  ///< System uptime in milliseconds (calculated)
	SYSTEM_UTC,		  ///< UTC timestamp in milliseconds (calculated with time.h)
	DATA_SIMPLE,	  ///< Direct R1UP variable (fixed-point to float)
	DATA_SCALED,	  ///< R1UP variable with unit scaling (e.g. rad/s to deg/s)
	DATA_GPS_LAT,	  ///< GPS latitude (requires 3 R1UP vars: LAT_DEG, LAT_MIN, LAT_DECI)
	DATA_GPS_LON	  ///< GPS longitude (requires 3 R1UP vars: LONG_DEG, LONG_MIN, LONG_DECI)
} r1up_to_rc_channel_type_t;

/*============================================================================*/
/* COMPILE-TIME VALIDATION                                                   */
/*============================================================================*/

/**
 * @brief Count total channels in mapping table
 */
#define R1UP_TO_RC_COUNT_CHANNELS(type, var_id, name, units, min, max, hz, prec, scale) 1 +
#define R1UP_TO_RC_TOTAL_CHANNELS (R1UP_TO_RC_MAPPING_TABLE(R1UP_TO_RC_COUNT_CHANNELS) 0)

/* Static assertion: must have exactly 26 channels (Track Attack standard for R1UP) */
_Static_assert(R1UP_TO_RC_TOTAL_CHANNELS == 26, "R1UP to RaceCapture mapping must have exactly 26 channels");

#ifdef __cplusplus
}
#endif

#endif /* R1UP_TO_RACECAPTURE_CFG_H */
