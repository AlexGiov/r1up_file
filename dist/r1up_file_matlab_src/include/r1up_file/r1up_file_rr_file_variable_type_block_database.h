/**
 * @file r1up_file_rr_file_variable_type_block_database.h
 * @brief ReRace Legacy File Format Database - Block and Variable Mappings
 *
 * @warning !!! READ-ONLY DATABASE - DO NOT MODIFY !!!
 * @warning This file contains the authoritative definitions for reading legacy ReRace .rer files.
 * @warning Any modification will break compatibility with existing ReRace files.
 * @warning This is a frozen specification for backward compatibility only.
 *
 * This file provides complete mapping tables to convert legacy ReRace file format (.rer)
 * to the modern Race1Up format (.r1up), enabling seamless reading of old data files.
 *
 * @par Legacy ReRace File Format Overview (.rer files)
 * The ReRace format was the predecessor to Race1Up and used a different block structure:
 * - Fixed 16-character block IDs (same as Race1Up)
 * - Different block organization (simpler header/body/footer)
 * - Numeric IDs for header metadata blocks (0xFF, 0xFE, 0xFD, etc.)
 * - Different variable stream format encoding
 * - Limited to specific variable types (no extensibility)
 *
 * @par Conversion Strategy
 * This file enables reading ReRace files WITHOUT duplicating the entire library codebase:
 * 1. **Block Mapping**: ReRace blocks are mapped to equivalent Race1Up blocks
 * 2. **Variable Mapping**: ReRace variable IDs are mapped to Race1Up variable IDs
 * 3. **Type Mapping**: ReRace inner types are mapped to Race1Up fixed-point types
 * 4. **Unified Processing**: After mapping, all files use the same reading functions
 *
 * The library automatically detects file type and applies appropriate mappings,
 * presenting a unified interface regardless of the underlying format.
 *
 * @par Database Tables
 * - **R1UP_FILE_RR_FILE_BLOCKS_TABLE**: Maps ReRace block structure to Race1Up blocks
 * - **R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE**: Maps ReRace variables/types to Race1Up equivalents
 * - **R1UP_FILE_RR_FILE_INNER_DATA_TYPE_ID_TABLE**: Defines ReRace inner data type IDs
 *
 * @author Alex Giov
 * @date February 4, 2026
 * @version 1.0.0 (Frozen - Legacy Support Only)
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 */

#ifndef R1UP_FILE_RR_FILE_VARIABLE_TYPE_BLOCK_DATABASE_H
#define R1UP_FILE_RR_FILE_VARIABLE_TYPE_BLOCK_DATABASE_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* RERACE FILE BLOCK STRUCTURE DEFINITIONS                                   */
/*============================================================================*/

/**
 * @defgroup ReRace_Block_Constants ReRace Block Structure Constants
 * @brief Core constants defining ReRace file block layout
 * @{
 */
/**
 * @brief Block ID length for ReRace files (16 characters, same as Race1Up)
 */
#define R1UP_FILE_RR_FILE_BLCK_ID_LENGTH 16U

/**
 * @brief Main section block identifiers for ReRace file format
 * @details These are the three primary structural blocks in every ReRace file:
 * - Header: Contains all metadata (version, user info, variable format, etc.)
 * - Body: Contains the actual telemetry data samples
 * - Footer: Optional section for track/session information
 */
#define R1UP_FILE_RR_HEADER_ID_STRING "RR_FILE_HEADER__"
#define R1UP_FILE_RR_BODY_ID_STRING "RR_FILE_BODY____"
#define R1UP_FILE_RR_FOOTER_ID_STRING "RR_FILE_FOOTER__"

/** @} */ /* End of ReRace_Block_Constants group */

/*============================================================================*/
/* RERACE BLOCK MAPPING TABLE                                                */
/*============================================================================*/

/**
 * @defgroup ReRace_Block_Mapping_Table ReRace Block Mapping Table
 * @brief Complete mapping of ReRace file blocks to Race1Up equivalents
 *
 * @par Purpose
 * This table defines how each ReRace block is interpreted and mapped to the Race1Up format.
 * It enables the library to read legacy .rer files by translating their structure on-the-fly.
 *
 * @par Table Structure
 * Each entry contains 8 parameters that fully describe a ReRace block:
 *
 * | Parameter          | Description                                                    | Example              |
 * |--------------------|----------------------------------------------------------------|----------------------|
 * | R1UP_BLOCK_ID_MAP  | Corresponding Race1Up block ID symbol                          | R1UP_FILE_VER_BID    |
 * | ID_in_file         | How the block ID appears in the .rer file                      | 0xFFu or "STRING"    |
 * | ID_type            | C type of the ID (uint8_t for numeric, char for string)        | uint8_t              |
 * | ID_Length          | Size of the ID in bytes                                        | 1U or 16U            |
 * | Payload_length_type| C type used for payload length field                           | uint8_t / uint16_t   |
 * | Payload_length     | Size of the payload length field in bytes                      | 2U                   |
 * | is_length_in_file  | Whether length field is actually stored in file (true/false)   | true                 |
 * | PAYLOAD            | Default payload value (string literal or hex constant)         | "Version string"     |
 *
 * @par ReRace Block ID Types
 * ReRace uses TWO types of block identifiers:
 * 1. **String IDs** (16 chars): For main sections (header/body/footer) - same as Race1Up
 * 2. **Numeric IDs** (1 byte): For metadata blocks (0xFF=version, 0xFE=hardware, etc.)
 *
 * @par Example Entry Breakdown
 * @code
 * BLCK_MIX_ENTRY(
 *     R1UP_FILE_VER_BID,              // Maps to Race1Up version block
 *     0xFFu,                          // In .rer file, this block has ID byte 0xFF
 *     uint8_t,                        // ID is a single unsigned byte
 *     1U,                             // ID occupies 1 byte in file
 *     uint8_t,                        // Payload length field is 1 byte
 *     0x3Eu,                          // Length field size is 0x3E (62 bytes)
 *     true,                           // Length IS stored in file
 *     "File ver: v0.20140115..."      // Default/example payload content
 * )
 * @endcode
 *
 * @par Critical Notes
 * - **Order Matters**: Blocks are read in the order they appear in the file
 * - **Frozen Definition**: This table is READ-ONLY for backward compatibility
 * - **Automatic Detection**: Library auto-detects ReRace vs Race1Up files
 * - **Unified Processing**: After mapping, both formats use same read functions
 *
 * @warning Never modify this table - it would break compatibility with existing ReRace files
 * @warning ReRace format is legacy - all new files should use Race1Up format
 *
 * @{
 */
// clang-format off
#define R1UP_FILE_RR_FILE_BLOCKS_TABLE(BLCK_MIX_ENTRY) 																																											\
	/* 				[ID MAP]						[ID]							[ID]		[ID]	[LENGTH]		[LENGTH]	[LENGTH]	[PAYLOAD]																	*/	\
	/* 				R1UP BLOCK ID MAP				ID in file						ID type		ID 		Payload length	Payload		is length  																				*/	\
	/* 																							Length	type			length		in file?																				*/	\
	BLCK_MIX_ENTRY(	R1UP_FILE_HEADER_BID,			R1UP_FILE_RR_HEADER_ID_STRING,	char,		16U,	uint16_t,		2U,			false,		0x00E0U																		)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_VER_BID,				0xFFu,							uint8_t,	1U,		uint8_t,		0x3Eu,		true,		"File ver: v0.20140115 add Altitude and sat num\0"							)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_HW_VER_BID,			0xFEu,							uint8_t,	1U,		uint8_t,		0x60u,		true,		"HARDWARE VERSION: REV_C pre-produzione\0"									)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_FW_VER_BID,			0xFDu,							uint8_t,	1U,		uint8_t,		0x60u,		true,		"FIRMWARE VERSION: v.20120521 beta. Pic32 USB STARTER KIT + MilleFori\0"	)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_USER_BID,				0xFCu,							uint8_t,	1U,		uint8_t,		0x10u,		true,		"Alex_Giov\0"																)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_VEHICLE_BID,			0xFBu,							uint8_t,	1U,		uint8_t,		0x20u,		true,		"Punto 55s 330.000Km!!!\0"													)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_STRING_BID,			0xFAu,							uint8_t,	1U,		uint8_t,		0x30u,		true,		"ReRace Test File: generato con Matlab!\0"									)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_ACQ_DATE_BID,			0xF9u,							uint8_t,	1U,		uint8_t,		0x20u,		true,		"Date in some format!!!\0"													)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_ACQ_LEN_BID,			0xF8u,							uint8_t,	1U,		uint8_t,		0x20u,		true,		"Date length in second!!!\0"												)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_VARS_STRM_FRMT_BID,	0x00u,							uint8_t,	1U,		uint16_t,		0x02U,		true,		""																			)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_BODY_BID,				R1UP_FILE_RR_BODY_ID_STRING,	char,		16U,	uint16_t,		2U,			false,		0x0000U																		)	\
	BLCK_MIX_ENTRY(	R1UP_FILE_FOOTER_BID,			R1UP_FILE_RR_FOOTER_ID_STRING,	char,		16U,	uint16_t,		0U,			false,		0xFFFFU																		)	\
 	BLCK_MIX_ENTRY(	R1UP_FILE_TRACK_FTR_BID,		0xFFU,							uint8_t,	1U,		uint8_t,		0xF0U,		true,		NULL																		)	\
 	BLCK_MIX_ENTRY(	R1UP_FILE_SESSION_FTR_BID,		0x00U,							uint8_t,	1U,		uint16_t,		0xFFU,		false,		NULL																		)
// clang-format on

/** @} */ /* End of ReRace_Block_Mapping_Table group */

/**
 * @defgroup ReRace_Variable_Type_Mapping_Table ReRace Variable and Type Mapping Table
 * @brief Authoritative mapping of ReRace variables and data types to Race1Up equivalents
 *
 * @par Purpose
 * This table enables the library to read legacy ReRace telemetry data by translating
 * both variable identifiers AND data types from the old format to the new format.
 * This dual mapping allows complete backward compatibility without code duplication.
 *
 * @par Table Structure
 * Each entry maps ONE ReRace variable to its Race1Up equivalent with 6 parameters:
 *
 * | Parameter              | Description                                           | Example                    |
 * |------------------------|-------------------------------------------------------|----------------------------|
 * | ReRace_VariableID      | Symbol name for ReRace variable                       | RR_RPM_ID                  |
 * | ReRace_VariableID_Value| Numeric ID used in .rer files                         | 1                          |
 * | Data_TypeID            | ReRace inner type (from type table below)             | RR_FILE_UINT16             |
 * | Freq_Data_divisor      | Sample frequency divisor (1=every sample, 10=every 10)| 1U                         |
 * | Race1UP_VariableID_MAP | Corresponding Race1Up variable ID (categorized)      | R1UP_FILE_VAR_ID_RPM       |
 * | Race1UP_TypeID_MAP     | Corresponding Race1Up fixed-point type ID            | R1UP_FILE_TYPE_ID_UQ16_0   |
 *
 * @par ReRace vs Race1Up Differences
 * - **Variable IDs**: ReRace used simple sequential numbers (1, 2, 3...), Race1Up uses categorized 32-bit IDs
 * (0x01xxxxxx, 0x02xxxxxx, etc.)
 * - **Type IDs**: ReRace used simple inner types (UINT8, UINT16, etc.), Race1Up uses self-documenting fixed-point types
 * (UQ16_0, Q3_28, etc.)
 * - **Encoding**: ReRace types are basic C types, Race1Up types encode sign, size, integer bits, and fractional bits
 *
 * @par Example Entry Breakdown
 * @code
 * VSF_ENTRY(
 *     RR_RPM_ID,                    // ReRace symbol: engine RPM
 *     1,                            // In .rer files, RPM has ID=1
 *     RR_FILE_UINT16,               // Stored as 16-bit unsigned integer
 *     1U,                           // Sampled at full frequency (every sample)
 *     R1UP_FILE_VAR_ID_RPM,         // Maps to Race1Up RPM (0x01000000U - Engine category)
 *     R1UP_FILE_TYPE_ID_UQ16_0      // Maps to Race1Up UQ16.0 type (0x80021000U)
 * )
 * @endcode
 *
 * @par Frequency Divisor Behavior
 * The frequency divisor implements "sample-and-hold" logic:
 * - **fd=1**: Variable sampled every sample (0, 1, 2, 3, 4, ...)
 * - **fd=10**: Variable sampled every 10th sample (0, 10, 20, 30, ...), held constant between samples
 * - Used to reduce storage for slow-changing variables (temperature, voltage, etc.)
 *
 * @par Critical Mappings
 * - **Engine/Powertrain** (Category 0x01): RPM, exhaust temp, coolant temp, throttle, gear
 * - **GPS/Navigation** (Category 0x02): Speed, latitude, longitude, altitude, satellite count
 * - **IMU Sensors** (Category 0x03): Accelerometers (X/Y/Z), orientation (roll/pitch/yaw), gyroscopes (X/Y/Z)
 * - **Electrical** (Category 0x04): Battery voltage, external power supply, internal temperature
 *
 * @par Usage in Library
 * When reading a ReRace file:
 * 1. Library detects .rer format
 * 2. Reads variable stream format block (contains ReRace IDs and types)
 * 3. Uses this table to translate to Race1Up IDs and types
 * 4. All subsequent operations use standard Race1Up functions
 * 5. User sees only Race1Up variable IDs (transparent conversion)
 *
 * @warning This table is FROZEN for backward compatibility - never modify existing entries
 * @warning Order and values must match the ReRace specification exactly
 * @warning All new telemetry should use Race1Up format, not ReRace
 *
 * @see r1up_file_variable_type_database.h for Race1Up variable and type definitions
 * @{
 */
// clang-format off
#define R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE(VSF_ENTRY)																						\
	/*			ReRace				ReRace		Data TypeID			Freq.Data 	Race1UP							Race1UP TypeID				*/	\
	/*			VariableID			VariableID						divisor		VariableID						MAP							*/	\
	/*								Value										MAP															*/	\
	VSF_ENTRY(	RR_RPM_ID, 			1,			RR_FILE_UINT16,		1U,			R1UP_FILE_VAR_ID_RPM, 			R1UP_FILE_TYPE_ID_UQ16_0	)	\
	VSF_ENTRY(	RR_EXAUSTEMP_ID,	2,			RR_FILE_UINT16,		1U,			R1UP_FILE_VAR_ID_EXAUST_TEMP,	R1UP_FILE_TYPE_ID_UQ12_4	)	\
	VSF_ENTRY(	RR_FLUIDTEMP_ID, 	3,			RR_FILE_UINT16,		10U,		R1UP_FILE_VAR_ID_COOLANT_TEMP,	R1UP_FILE_TYPE_ID_UQ10_6	)	\
	VSF_ENTRY(	RR_X_ACC_ID,		4,			RR_FILE_UINT16,		1U, 		R1UP_FILE_VAR_ID_X_ACC,			R1UP_FILE_TYPE_ID_Q4_11		)	\
	VSF_ENTRY(	RR_Y_ACC_ID,		5,			RR_FILE_UINT16,		1U, 		R1UP_FILE_VAR_ID_Y_ACC,			R1UP_FILE_TYPE_ID_Q4_11		)	\
	VSF_ENTRY(	RR_Z_ACC_ID,		6,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_Z_ACC,			R1UP_FILE_TYPE_ID_Q4_11		)	\
	VSF_ENTRY(	RR_X_ROOL_ID,		7,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_ROLL,			R1UP_FILE_TYPE_ID_Q3_12		)	\
	VSF_ENTRY(	RR_Y_PITCH_ID,		8,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_PITCH,			R1UP_FILE_TYPE_ID_Q3_12		)	\
	VSF_ENTRY(	RR_Z_YAW_ID,		9,			RR_FILE_UINT16,		1U,			R1UP_FILE_VAR_ID_YAW,			R1UP_FILE_TYPE_ID_Q3_12		)	\
	VSF_ENTRY(	RR_GPS_VEL_ID,		10,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_GPS_SPEED,		R1UP_FILE_TYPE_ID_UQ16_0	)	\
	VSF_ENTRY(	RR_LAT_DEG_ID,		11,			RR_FILE_UINT8,		1U,	 		R1UP_FILE_VAR_ID_LAT_DEG,		R1UP_FILE_TYPE_ID_UQ8_0		)	\
	VSF_ENTRY(	RR_LAT_MIN_ID,		12,			RR_FILE_INT8,		1U,	 		R1UP_FILE_VAR_ID_LAT_MIN,		R1UP_FILE_TYPE_ID_Q7_0		)	\
	VSF_ENTRY(	RR_LAT_DECI_ID,		13,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_LAT_DECI,		R1UP_FILE_TYPE_ID_UQ16_0	)	\
	VSF_ENTRY(	RR_LONG_DEG_ID, 	14,			RR_FILE_UINT8,		1U,	 		R1UP_FILE_VAR_ID_LONG_DEG,		R1UP_FILE_TYPE_ID_UQ8_0		)	\
	VSF_ENTRY(	RR_LONG_MIN_ID, 	15,			RR_FILE_INT8,		1U,	 		R1UP_FILE_VAR_ID_LONG_MIN,		R1UP_FILE_TYPE_ID_Q7_0		)	\
	VSF_ENTRY(	RR_LONG_DECI_ID, 	16,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_LONG_DECI,		R1UP_FILE_TYPE_ID_UQ16_0	)	\
	VSF_ENTRY(	RR_GEAR_ID, 		17,			RR_FILE_UINT8,		1U,	 		R1UP_FILE_VAR_ID_GEAR,			R1UP_FILE_TYPE_ID_UQ8_0		)	\
	VSF_ENTRY(	RR_THROTTLE_ID, 	18,			RR_FILE_UINT8,		1U,	 		R1UP_FILE_VAR_ID_THROTTLE,		R1UP_FILE_TYPE_ID_UQ8_0		)	\
	VSF_ENTRY(	RR_MSL_ALT_ID, 		19,			RR_FILE_UINT16,	/*ALEXGIOV TODO: Verificare che non sia int16 sul firmware*/	1U,	 		R1UP_FILE_VAR_ID_ALTITUDE,		R1UP_FILE_TYPE_ID_Q15_0		)	\
	VSF_ENTRY(	RR_GPS_USED_SAT_ID,	20,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_GPS_SAT_COUNT,	R1UP_FILE_TYPE_ID_UQ16_0	)	\
	VSF_ENTRY(	RR_INTRNL_TMP,		21,			RR_FILE_UINT16,		10U,		R1UP_FILE_VAR_ID_INT_TEMP,		R1UP_FILE_TYPE_ID_UQ12_4	)	\
	VSF_ENTRY(	RR_BAT_VOLT_ID,		22,			RR_FILE_UINT16,		10U,		R1UP_FILE_VAR_ID_BATT_VOLT,		R1UP_FILE_TYPE_ID_UQ6_10	)	\
	VSF_ENTRY(	RR_EXT_PS_VOLT_ID,	23,			RR_FILE_UINT16,		10U,		R1UP_FILE_VAR_ID_EXT_PS_VOLT,	R1UP_FILE_TYPE_ID_UQ12_4	)	\
	VSF_ENTRY(	RR_X_GYRO_RDS_ID,	24,			RR_FILE_UINT32,		1U,			R1UP_FILE_VAR_ID_X_GYRO,		R1UP_FILE_TYPE_ID_Q3_28		)	\
	VSF_ENTRY(	RR_Y_GYRO_RDS_ID,	25,			RR_FILE_UINT32,		1U,			R1UP_FILE_VAR_ID_Y_GYRO,		R1UP_FILE_TYPE_ID_Q3_28		)	\
	VSF_ENTRY(	RR_Z_GYRO_RDS_ID,	26,			RR_FILE_UINT32,		1U,			R1UP_FILE_VAR_ID_Z_GYRO,		R1UP_FILE_TYPE_ID_Q3_28		)	\
	VSF_ENTRY(	RR_RPM_HALL_ID, 	27,			RR_FILE_UINT16,		1U,	 		R1UP_FILE_VAR_ID_RPM_HALL,		R1UP_FILE_TYPE_ID_UQ16_0	)	\
	VSF_ENTRY(	RR_PWSS_HALL_ID, 	28,			RR_FILE_UINT32,		1U,	 		R1UP_FILE_VAR_ID_PWSS_HALL,		R1UP_FILE_TYPE_ID_UQ32_0	)
// clang-format on

/** @} */ /* End of ReRace_Variable_Type_Mapping_Table group */

/*============================================================================*/
/* RERACE INNER DATA TYPE DEFINITIONS                                        */
/*============================================================================*/

/**
 * @defgroup ReRace_Inner_Data_Types ReRace Inner Data Type Definitions
 * @brief Fundamental data types used in ReRace file variable storage
 *
 * @par Purpose
 * ReRace files use simple C-style type identifiers (UINT8, UINT16, UINT32, INT8)
 * to specify how variables are stored in the binary data stream. This table defines
 * the complete set of supported types with their numeric identifiers.
 *
 * @par Type ID Values
 * | Type Symbol      | Type ID | C Type   | Size  | Range                    | Description                  |
 * |------------------|---------|----------|-------|--------------------------|------------------------------|
 * | RR_FILE_UINT8    | 1U      | uint8_t  | 1 byte| 0 to 255                 | Unsigned 8-bit integer       |
 * | RR_FILE_UINT16   | 2U      | uint16_t | 2 bytes| 0 to 65,535             | Unsigned 16-bit integer      |
 * | RR_FILE_UINT32   | 3U      | uint32_t | 4 bytes| 0 to 4,294,967,295      | Unsigned 32-bit integer      |
 * | RR_FILE_INT8     | 5U      | int8_t   | 1 byte| -128 to +127             | Signed 8-bit integer         |
 *
 * @par Usage
 * These type IDs appear in the ReRace variable stream format block and are mapped
 * to Race1Up fixed-point types during file reading. The mapping table above shows
 * how each ReRace type translates to a specific Race1Up type.
 *
 * @par Limitations
 * - No signed 16-bit or 32-bit types (not needed in ReRace applications)
 * - No floating-point support (all values stored as integers)
 * - No fixed-point fractional encoding (handled by Race1Up mapping)
 * - Limited type set compared to Race1Up's extensive fixed-point types
 *
 * @note These values are part of the frozen ReRace specification and must not be changed
 * @{
 */
#define R1UP_FILE_RR_FILE_INNER_DATA_TYPE_ID_TABLE(IDT_TYPE_ENTRY) \
	IDT_TYPE_ENTRY(RR_FILE_UINT8, 1U)                              \
	IDT_TYPE_ENTRY(RR_FILE_UINT16, 2U)                             \
	IDT_TYPE_ENTRY(RR_FILE_UINT32, 3U)                             \
	IDT_TYPE_ENTRY(RR_FILE_INT8, 5U)

/** @} */ /* End of ReRace_Inner_Data_Types group */

/*============================================================================*/
/* GENERATED ENUMERATIONS                                                     */
/*============================================================================*/

/**
 * @brief Enumeration of all ReRace variable IDs
 * @details Generated from R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE for type-safe ReRace variable handling.
 * These values correspond to the numeric IDs stored in legacy .rer files.
 *
 * @note This enumeration is automatically generated via X-macro expansion
 * @note Values match the ReRace specification exactly (frozen for compatibility)
 */
typedef enum {
#define VARIABLE_ID_AS_ENUM(name, value, ...) name = value,
	R1UP_FILE_RR_FILE_VARS_AND_TYPES_TABLE(VARIABLE_ID_AS_ENUM)
#undef VARIABLE_ID_AS_ENUM
		R1UP_FILE_RR_VARS_STREAM_FORMAT_ID_NUM
} r1up_file_rr_vars_stream_format_ids_t;

#ifdef __cplusplus
}
#endif

#endif /* R1UP_FILE_RR_FILE_VARIABLE_TYPE_BLOCK_DATABASE_H */