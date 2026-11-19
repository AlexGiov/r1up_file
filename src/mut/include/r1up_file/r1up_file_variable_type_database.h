/**
 * @file r1up_file_variable_type_database.h
 * @brief Race1Up Variable and Type ID Authoritative Database
 *
 * @warning !!! READ-ONLY FILE - DO NOT MODIFY !!!
 * @warning This file is part of the r1up_file library distribution.
 * @warning Modifying this file will break library binary compatibility.
 * @warning Use this file ONLY as reference for available variable and type IDs.
 * @warning Contact library maintainer before making any changes.
 *
 * This file contains the complete, authoritative definitions of:
 * - All known Race1Up variable identifiers (VARIABLE_ID_TABLE)
 * - All supported fixed-point data type identifiers (TYPE_ID_TABLE)
 *
 * These tables are the "sources of truth" for READ mode operations.
 * They enable the library to interpret R1UP files dynamically at runtime,
 * supporting arbitrary combinations of variables and types as determined
 * by the VSF (Variable Stream Format) block stored in each file.
 *
 * @note CRITICAL: There is NO fixed mapping between variable ID and type ID.
 *       The same variable (e.g., RPM) can have different types in different files.
 *       The actual VAR_ID ↔ TYPE_ID pairing is determined at runtime from the
 *       VSF block read from the file.
 *
 * @note These tables are used ONLY for READ mode. For WRITE mode configuration,
 *       see R1UP_FILE_VARS_STREAM_FORMAT_TABLE in r1up_file_cfg.h.
 *
 * @author Alex Giov
 * @date January 28, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 */

#ifndef R1UP_FILE_VARIABLE_TYPE_DATABASE_H
#define R1UP_FILE_VARIABLE_TYPE_DATABASE_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* VARIABLE ID DATABASE - Complete Race1Up Signal Catalog                    */
/*============================================================================*/

/**
 * @defgroup R1UP_Variable_Database Variable ID Database
 * @brief Complete catalog of all known Race1Up variable identifiers
 *
 * This table maps numeric IDs to semantic signal names and descriptions.
 * It represents ALL variables that could potentially appear in any R1UP file.
 *
 * Format: ENTRY(Symbol, HexValue, ShortName, "Human Description")
 *   - Symbol: C enum constant (e.g., R1UP_FILE_VAR_ID_RPM)
 *   - HexValue: Unique 32-bit ID (e.g., 0x01000000U)
 *   - ShortName: C token for variable/field naming (e.g., rpm) - NO QUOTES
 *   - Description: Human-readable string with quotes
 *
 * @par Variable ID Encoding Scheme (Recommended, Not Mandatory)
 * The Variable IDs follow a category-based 32-bit encoding pattern:
 *
 * Bit Layout: 0xCCSSSSSSU
 *   - Bits 31-24 (CC): Category code
 *   - Bits 23-0 (SSSSSS): Sequential ID within category (16 million IDs per category)
 *
 * Category Codes:
 *   - 0x01 = Engine & Powertrain (0x01000000 - 0x01FFFFFF)
 *   - 0x02 = GPS & Navigation (0x02000000 - 0x02FFFFFF)
 *   - 0x03 = IMU - Inertial Measurement Unit (0x03000000 - 0x03FFFFFF)
 *     . 0x0301xxxx = Accelerometer (X/Y/Z acceleration)
 *     . 0x0302xxxx = Orientation (Roll/Pitch/Yaw)
 *     . 0x0303xxxx = Gyroscope (X/Y/Z angular velocity)
 *   - 0x04 = Electrical & Power (0x04000000 - 0x04FFFFFF)
 *   - 0xFF = User-defined/Custom variables (0xFF000000 - 0xFFFFFFFF)
 *
 * Examples:
 *   - 0x01000000U → RPM (Engine category, first ID)
 *   - 0x02000000U → GPS_SPEED (GPS category, first ID)
 *   - 0x03010000U → X_ACC (IMU Accelerometer subcategory, first ID)
 *   - 0xFF000001U → Custom user variable #1
 *
 * Benefits:
 *   - Self-documenting: category visible in hex value
 *   - Easy filtering: `if ((var_id & 0xFF000000U) == 0x01000000U)` → all engine variables
 *   - Scalable: 16 million IDs per category
 *   - Organized: related variables grouped together
 *
 * @note This encoding is a CONVENTION to generate unique IDs, not a requirement.
 * @note You may use any unique 32-bit value as Variable ID if preferred.
 * @note Adding new variables: append to end of appropriate category, never remove/reorder existing
 * @{
 */
// clang-format off
#define R1UP_FILE_VARIABLE_ID_TABLE(ENTRY)																				\
	/* Engine & Powertrain Variables (Category 0x01) */																	\
	ENTRY(R1UP_FILE_VAR_ID_RPM,				0x01000000U,	rpm,		"Engine RPM")									\
	ENTRY(R1UP_FILE_VAR_ID_EXAUST_TEMP,		0x01000001U,	exhaust,	"Exhaust Gas Temperature")						\
	ENTRY(R1UP_FILE_VAR_ID_COOLANT_TEMP,	0x01000002U,	coolant,	"Engine Coolant Temperature")					\
	ENTRY(R1UP_FILE_VAR_ID_THROTTLE,		0x01000003U,	throttle,	"Throttle Position Percentage")					\
	ENTRY(R1UP_FILE_VAR_ID_GEAR,			0x01000004U,	gear,		"Current Gear Position")						\
	ENTRY(R1UP_FILE_VAR_ID_RPM_HALL,		0x01000005U,	rpm_hall,	"RPM from Hall Effect Sensor")					\
	ENTRY(R1UP_FILE_VAR_ID_PWSS_HALL,		0x01000006U,	pwss_hall,	"Pulse Width Hall Sensor Signal")				\
	/* GPS & Navigation Variables (Category 0x02) */																	\
	ENTRY(R1UP_FILE_VAR_ID_GPS_SPEED,		0x02000000U,	gps_speed,	"GPS Velocity (km/h or mph)")					\
	ENTRY(R1UP_FILE_VAR_ID_LAT_DEG,			0x02000001U,	lat_deg,	"GPS Latitude - Degrees Component")				\
	ENTRY(R1UP_FILE_VAR_ID_LAT_MIN,			0x02000002U,	lat_min,	"GPS Latitude - Minutes Component")				\
	ENTRY(R1UP_FILE_VAR_ID_LAT_DECI,		0x02000003U,	lat_deci,	"GPS Latitude - Decimal Minutes Component")		\
	ENTRY(R1UP_FILE_VAR_ID_LONG_DEG,		0x02000004U,	long_deg,	"GPS Longitude - Degrees Component")			\
	ENTRY(R1UP_FILE_VAR_ID_LONG_MIN,		0x02000005U,	long_min,	"GPS Longitude - Minutes Component")			\
	ENTRY(R1UP_FILE_VAR_ID_LONG_DECI,		0x02000006U,	long_deci,	"GPS Longitude - Decimal Minutes Component")	\
	ENTRY(R1UP_FILE_VAR_ID_ALTITUDE,		0x02000007U,	alt,		"Mean Sea Level Altitude (meters)")				\
	ENTRY(R1UP_FILE_VAR_ID_GPS_SAT_COUNT,	0x02000008U,	gps_sat,	"Number of GPS Satellites Used")				\
	/* IMU - Accelerometer (Category 0x03, Subcategory 0x01) */															\
	ENTRY(R1UP_FILE_VAR_ID_X_ACC,			0x03010000U,	x_acc,		"X-Axis Acceleration (g-force)")				\
	ENTRY(R1UP_FILE_VAR_ID_Y_ACC,			0x03010001U,	y_acc,		"Y-Axis Acceleration (g-force)")				\
	ENTRY(R1UP_FILE_VAR_ID_Z_ACC,			0x03010002U,	z_acc,		"Z-Axis Acceleration (g-force)")				\
	/* IMU - Orientation (Category 0x03, Subcategory 0x02) */															\
	ENTRY(R1UP_FILE_VAR_ID_ROLL,			0x03020000U,	roll,		"Roll Angle (degrees)")							\
	ENTRY(R1UP_FILE_VAR_ID_PITCH,			0x03020001U,	pitch,		"Pitch Angle (degrees)")						\
	ENTRY(R1UP_FILE_VAR_ID_YAW,				0x03020002U,	yaw,		"Yaw Angle (degrees)")							\
	/* IMU - Gyroscope (Category 0x03, Subcategory 0x03) */																\
	ENTRY(R1UP_FILE_VAR_ID_X_GYRO,			0x03030000U,	x_gyro,		"X-Axis Angular Velocity (rad/s)")				\
	ENTRY(R1UP_FILE_VAR_ID_Y_GYRO,			0x03030001U,	y_gyro,		"Y-Axis Angular Velocity (rad/s)")				\
	ENTRY(R1UP_FILE_VAR_ID_Z_GYRO,			0x03030002U,	z_gyro,		"Z-Axis Angular Velocity (rad/s)")				\
	/* Electrical & Power Variables (Category 0x04) */																	\
	ENTRY(R1UP_FILE_VAR_ID_BATT_VOLT,		0x04000000U,	batt_volt,	"Battery Voltage (V)")							\
	ENTRY(R1UP_FILE_VAR_ID_EXT_PS_VOLT,		0x04000001U,	power_volt,	"External Power Supply Voltage (V)")			\
	ENTRY(R1UP_FILE_VAR_ID_INT_TEMP,		0x04000002U,	int_temp,	"Internal Electronics Temperature (°C)")
// clang-format on
/** @} */ /* End of R1UP_Variable_Database group */

/*============================================================================*/
/* TYPE ID DATABASE - Fixed-Point Data Type Catalog                          */
/*============================================================================*/

/**
 * @defgroup R1UP_Type_Database Type ID Database
 * @brief Complete catalog of all supported fixed-point data types
 *
 * This table maps numeric type IDs to fixed-point format descriptions.
 * Each entry defines a specific binary representation for numeric data.
 *
 * Format: ENTRY(Symbol, HexValue, type_token, "Type Description")
 *
 * Naming Convention: TYPE_[U]Qn_m where:
 *   - U = Unsigned (omit for signed)
 *   - n = Integer bits
 *   - m = Fractional bits
 *
 * @par Type ID Encoding Scheme (Recommended, Not Mandatory)
 * The Type IDs follow a self-documenting 32-bit encoding pattern with THREE major categories:
 *
 * @par Fixed-Point Types (0x00xxxxxx and 0x80xxxxxx)
 * Bit Layout: 0xSSFFIIFFU
 *   - Bits 31-24 (SS): Sign flag (0x80 = unsigned, 0x00 = signed)
 *   - Bits 23-16 (FF): Size in bytes (0x01 = 8-bit, 0x02 = 16-bit, 0x04 = 32-bit)
 *   - Bits 15-8  (II): Integer bits (decimal value in hex)
 *   - Bits 7-0   (FF): Fractional bits (decimal value in hex)
 *
 * Examples:
 *   - UQ12_4  → 0x80020C04U (unsigned, 2 bytes, 12 integer bits, 4 fractional bits)
 *   - Q3_28   → 0x0004031CU (signed, 4 bytes, 3 integer bits, 28 fractional bits)
 *   - UQ16_0  → 0x80021000U (unsigned, 2 bytes, 16 integer bits, 0 fractional bits)
 *
 * @par Complex/Special Types (0xFFxxxxxx) - Future Extension
 * Reserved prefix 0xFF for non-numeric data types:
 *
 * Bit Layout: 0xFFCCSSSSU
 *   - Bits 31-24: Category marker (0xFF = complex/special type)
 *   - Bits 23-16 (CC): Type category:
 *     . 0x01 = String types (ASCII, UTF-8, UTF-16, etc.)
 *     . 0x02 = Structured records (compound data, tuples)
 *     . 0x03 = Array types (homogeneous element arrays)
 *     . 0x04 = Binary blobs (raw bytes, checksums, signatures)
 *     . 0x05 = Timestamp/DateTime (Unix epoch, ISO8601, GPS time)
 *     . 0x06 = Enumeration types (named constants)
 *     . 0xFE = Reserved for future use
 *     . 0xFF = User-defined custom types
 *   - Bits 15-0 (SSSS): Subtype/variant identifier (category-specific)
 *
 * Suggested Subtype Encoding Examples:
 *   - 0xFF010001U → ASCII string, null-terminated, max 256 chars
 *   - 0xFF010100U → UTF-8 string, length-prefixed
 *   - 0xFF020001U → GPS coordinate struct (lat/lon/alt)
 *   - 0xFF050001U → Unix timestamp (32-bit seconds since 1970)
 *   - 0xFF050002U → GPS time (week number + milliseconds)
 *
 * @note Complex types are NOT YET IMPLEMENTED in current library version.
 * @note This scheme reserves namespace for future extensibility.
 * @note Detection: `if ((type_id & 0xFF000000U) == 0xFF000000U)` identifies complex types.
 *
 * @par General Notes
 * @note This encoding is a CONVENTION to generate unique IDs, not a requirement.
 * @note The library does NOT parse these bit fields at runtime - they are only documentation.
 * @note Type properties (size, signedness) are still determined by switch/case in code.
 * @note You may use any unique 32-bit value as Type ID if preferred.
 *
 * @note Size and conversion functions are derived at runtime via switch/case
 * @note All types correspond to ag_fixedpoint library types
 * @note type_token is used to build ag_fixedpoint_##type_token##_t type,
 * ag_fixedpoint_##type_token##_to_float function for floating point conversion, etc. as_##type_token
 * for variable name in Tagged Union.
 * @{
 */
// clang-format off
#define R1UP_FILE_TYPE_ID_TABLE(ENTRY)																					\
	/* 8-bit Types */																									\
	ENTRY(R1UP_FILE_TYPE_ID_UQ8_0,	0x80010800U,	uq8_0,	"Unsigned 8.0 - 8-bit unsigned integer (uint8_t)")			\
	ENTRY(R1UP_FILE_TYPE_ID_Q7_0,	0x00010700U,	q7_0,	"Signed 7.0 + sign bit - 8-bit signed integer (int8_t)")	\
	/* 16-bit Types */																									\
	ENTRY(R1UP_FILE_TYPE_ID_UQ16_0,	0x80021000U,	uq16_0,	"Unsigned 16.0 - 16-bit unsigned integer (uint16_t)")		\
	ENTRY(R1UP_FILE_TYPE_ID_UQ12_4,	0x80020C04U,	uq12_4,	"Unsigned 12.4 - 12-bit integer, 4-bit fractional (16-bit total)")	\
	ENTRY(R1UP_FILE_TYPE_ID_UQ10_6,	0x80020A06U,	uq10_6,	"Unsigned 10.6 - 10-bit integer, 6-bit fractional (16-bit total)")	\
	ENTRY(R1UP_FILE_TYPE_ID_UQ6_10,	0x8002060AU,	uq6_10,	"Unsigned 6.10 - 6-bit integer, 10-bit fractional (16-bit total)")	\
	ENTRY(R1UP_FILE_TYPE_ID_Q15_0,	0x00020F00U,	q15_0,	"Signed 15.0 + sign bit - 16-bit signed integer (int16_t)")	\
	ENTRY(R1UP_FILE_TYPE_ID_Q4_11,	0x0002040BU,	q4_11,	"Signed 4.11 + sign bit - 4-bit integer, 11-bit fractional (16-bit total)")	\
	ENTRY(R1UP_FILE_TYPE_ID_Q3_12,	0x0002030CU,	q3_12,	"Signed 3.12 + sign bit - 3-bit integer, 12-bit fractional (16-bit total)")	\
	/* 32-bit Types */																									\
	ENTRY(R1UP_FILE_TYPE_ID_UQ32_0,	0x80042000U,	uq32_0,	"Unsigned 32.0 - 32-bit unsigned integer (uint32_t)")		\
	ENTRY(R1UP_FILE_TYPE_ID_Q31_0,	0x00041F00U,	q31_0,	"Signed 31.0 + sign bit - 32-bit signed integer (int32_t)")	\
	ENTRY(R1UP_FILE_TYPE_ID_Q3_28,	0x0004031CU,	q3_28,	"Signed 3.28 + sign bit - 3-bit integer, 28-bit fractional (32-bit total)")
// clang-format on
/** @} */ /* End of R1UP_Type_Database group */

/*============================================================================*/
/* GENERATED ENUMERATIONS                                                     */
/*============================================================================*/

/**
 * @brief Enumeration of all Race1Up variable IDs
 * @details Generated from VARIABLE_ID_TABLE. Use these constants in API calls.
 */
typedef enum {
#define VARIABLE_ID_AS_ENUM(symbol, value, ...) symbol = value,
	R1UP_FILE_VARIABLE_ID_TABLE(VARIABLE_ID_AS_ENUM)
#undef VARIABLE_ID_AS_ENUM
		R1UP_FILE_VARIABLE_ID_COUNT /**< Total number of defined variables (not a valid ID) */
} r1up_variable_id_t;

/**
 * @brief Enumeration of all supported fixed-point type IDs
 * @details Generated from TYPE_ID_TABLE. Used internally for type identification.
 */
typedef enum {
#define TYPE_ID_AS_ENUM(name, value, ...) name = value,
	R1UP_FILE_TYPE_ID_TABLE(TYPE_ID_AS_ENUM)
#undef TYPE_ID_AS_ENUM
		R1UP_FILE_TYPE_ID_COUNT /**< Total number of defined types (not a valid ID) */
} r1up_type_id_t;

/**
 * @brief Macro to get the C type corresponding to a fixed-point type ID
 * @details Expands to the appropriate ag_fixedpoint_##type##_t type.
 * @note Used internally for type resolution based on TYPE_ID_TABLE.
 */
#define R1UP_FILE_TYPE(name) ag_fixedpoint_##name##_t

/**
 * @brief Macro to get the floating-point conversion function for a fixed-point type ID
 * @details Expands to ag_fixedpoint_##type##_to_float function.
 * @note Used internally for converting fixed-point to float based on TYPE_ID_TABLE.
 */
#define R1UP_FILE_TYPE_TO_FLOAT_FUNC(name) ag_fixedpoint_##name##_to_float

/**
 * @brief Macro to get variable name in Tagged Union based on fixed-point type ID
 * @details Expands to as_##type for use in tagged union variable access.
 */
#define R1UP_FILE_TYPE_AS_UNION_MEMBER(name) as_##name

#ifdef __cplusplus
}
#endif

#endif /* R1UP_FILE_VARIABLE_TYPE_DATABASE_H */

/**
 * @page variable_type_database Variable and Type Database Documentation
 *
 * @section db_overview Overview
 *
 * This database contains the complete knowledge of Race1Up variable identifiers
 * and data type formats. It serves as the foundation for dynamic file reading.
 *
 * @section db_usage Usage in Code
 *
 * @subsection db_read_mode READ Mode (Dynamic)
 *
 * When reading an R1UP file:
 * 1. File's VSF block specifies: [(var_id=0x0011, type_id=0x0012), ...]
 * 2. Library looks up var_id in VARIABLE_ID_TABLE → "RPM"
 * 3. Library looks up type_id in TYPE_ID_TABLE → "UQ12.4"
 * 4. Library reads data as UQ12.4 and converts to float
 *
 * @subsection db_no_fixed_mapping No Fixed VAR↔TYPE Mapping
 *
 * CRITICAL: The same variable can have different types in different files!
 *
 * Example:
 * - File A: RPM (0x0011) stored as R1UP_FILE_TYPE_ID_UQ16_0 (0x0001)
 * - File B: RPM (0x0011) stored as R1UP_FILE_TYPE_ID_Q31_0 (0x0050) for higher precision
 *
 * Both are valid and readable thanks to dynamic VSF interpretation.
 *
 * @section db_maintenance Maintenance
 *
 * @subsection db_adding_var Adding New Variables
 * 1. Append new ENTRY() to VARIABLE_ID_TABLE
 * 2. Choose unique hex ID (check for conflicts)
 * 3. Provide clear description
 * 4. Never remove or renumber existing entries
 *
 * @subsection db_adding_type Adding New Types
 * 1. Ensure ag_fixedpoint library supports the type
 * 2. Append new ENTRY() to TYPE_ID_TABLE
 * 3. Choose unique hex ID
 * 4. Update conversion switch statement in r1up_file.c
 */
