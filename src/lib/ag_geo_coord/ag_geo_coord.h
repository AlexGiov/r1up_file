/**
 * @file ag_geo_coord.h
 * @brief Geographic Coordinate System Library - WGS84 Standard
 *
 * This library provides data structures and functions for handling geographic
 * coordinates based on the WGS84 datum (World Geodetic System 1984), the
 * standard coordinate system used by GPS satellites.
 *
 * @author Alex Giov
 * @date February 9, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 *
 * @par License:
 * This software is proprietary and confidential.
 *
 * @par Coordinate System:
 * - Datum: WGS84 (EPSG:4326)
 * - Units: Decimal degrees
 * - Latitude range: [-90.0, +90.0] (negative = South, positive = North)
 * - Longitude range: [-180.0, +180.0] (negative = West, positive = East)
 * - Altitude: meters above WGS84 ellipsoid (optional)
 *
 * @par References:
 * - EPSG:4326 - WGS 84 coordinate reference system
 * - ISO 6709:2008 - Standard representation of geographic point location
 */

#ifndef AG_GEO_COORD_H
#define AG_GEO_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* INCLUDES                                                                   */
/*============================================================================*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "./../ag_fixedpoint/ag_fixedpoint.h"

/*============================================================================*/
/* CONSTANTS AND MACROS                                                       */
/*============================================================================*/

/**
 * @brief Minimum valid latitude in decimal degrees (South Pole)
 */
#define AG_GEO_COORD_LAT_MIN (-90.0f)

/**
 * @brief Maximum valid latitude in decimal degrees (North Pole)
 */
#define AG_GEO_COORD_LAT_MAX (90.0f)

/**
 * @brief Minimum valid longitude in decimal degrees (International Date Line West)
 */
#define AG_GEO_COORD_LON_MIN (-180.0f)

/**
 * @brief Maximum valid longitude in decimal degrees (International Date Line East)
 */
#define AG_GEO_COORD_LON_MAX (180.0f)

/**
 * @brief Invalid coordinate marker value
 */
#define AG_GEO_COORD_INVALID (999.0f)

/**
 * @brief Earth's mean radius in meters (for distance calculations)
 */
#define AG_GEO_COORD_EARTH_RADIUS_M (6371000.0f)

/*============================================================================*/
/* TYPE DEFINITIONS                                                           */
/*============================================================================*/

/**
 * @brief Result codes for geographic coordinate operations
 */
typedef enum {
	AG_GEO_COORD_SUCCESS = 0,		   ///< Operation completed successfully
	AG_GEO_COORD_ERROR_INVALID_PARAM,  ///< Invalid parameter (NULL pointer, etc.)
	AG_GEO_COORD_ERROR_OUT_OF_RANGE,   ///< Coordinate value out of valid range
	AG_GEO_COORD_ERROR_INVALID_COORD   ///< Coordinate is marked as invalid
} ag_geo_coord_result_t;

typedef ag_fixedpoint_float_t ag_geo_float_t;

/**
 * @brief Geographic point in WGS84 decimal degrees
 * @details Represents a single point on Earth's surface using WGS84 datum.
 * This is the standard format used by GPS receivers.
 *
 * @par Coordinate Conventions:
 * - latitude_deg: Positive = North, Negative = South, Range: [-90, +90]
 * - longitude_deg: Positive = East, Negative = West, Range: [-180, +180]
 * - altitude_m: Height above WGS84 ellipsoid (optional, use 0.0 if unknown)
 * - is_valid: Flag indicating if coordinates are valid
 *
 * @note Use AG_GEO_COORD_INVALID to mark invalid coordinates
 */
typedef struct {
	ag_geo_float_t latitude_deg;   ///< Latitude in decimal degrees (WGS84)
	ag_geo_float_t longitude_deg;  ///< Longitude in decimal degrees (WGS84)
	ag_geo_float_t altitude_m;	   ///< Altitude in meters above WGS84 ellipsoid (optional)
	bool is_valid;				   ///< Flag indicating if this coordinate is valid
} ag_geo_point_t;

/**
 * @brief Geographic line segment defined by two points
 * @details Commonly used for track boundaries, finish lines, or sector markers.
 * The line is defined by two endpoints in WGS84 coordinates.
 *
 * @par Common Uses:
 * - Race track finish line
 * - Track sector boundaries
 * - Geofence boundaries
 * - Virtual timing gates
 */
typedef struct {
	ag_geo_point_t point_a;	 ///< First endpoint of the line
	ag_geo_point_t point_b;	 ///< Second endpoint of the line
} ag_geo_line_t;

/**
 * @brief Geographic rectangular area defined by two opposite corners
 * @details Represents a bounding box in WGS84 coordinates, typically used
 * for defining track zones or areas of interest.
 *
 * @note The corners should be opposite vertices of the rectangle
 * (e.g., southwest and northeast corners)
 */
typedef struct {
	ag_geo_point_t corner_a;  ///< First corner (e.g., southwest)
	ag_geo_point_t corner_c;  ///< Opposite corner (e.g., northeast)
} ag_geo_rect_t;

/*============================================================================*/
/* PUBLIC FUNCTION PROTOTYPES                                                 */
/*============================================================================*/

/**
 * @brief Initialize a geographic point with default/invalid values
 * @param[out] point Pointer to point structure to initialize
 * @return AG_GEO_COORD_SUCCESS on success, error code otherwise
 */
ag_geo_coord_result_t ag_geo_point_init(ag_geo_point_t* point);

/**
 * @brief Set geographic point coordinates
 * @param[out] point Pointer to point structure
 * @param[in] latitude_deg Latitude in decimal degrees [-90, +90]
 * @param[in] longitude_deg Longitude in decimal degrees [-180, +180]
 * @param[in] altitude_m Altitude in meters (use 0.0 if unknown)
 * @return AG_GEO_COORD_SUCCESS on success, error code otherwise
 */
ag_geo_coord_result_t ag_geo_point_set(ag_geo_point_t* point, ag_geo_float_t latitude_deg, ag_geo_float_t longitude_deg,
									   ag_geo_float_t altitude_m);

/**
 * @brief Validate geographic point coordinates
 * @param[in] point Pointer to point to validate
 * @return true if coordinates are within valid WGS84 ranges, false otherwise
 */
bool ag_geo_point_is_valid(const ag_geo_point_t* point);

/**
 * @brief Calculate distance between two geographic points (Haversine formula)
 * @param[in] point_a First point
 * @param[in] point_b Second point
 * @param[out] distance_m Distance in meters
 * @return AG_GEO_COORD_SUCCESS on success, error code otherwise
 *
 * @note Uses Haversine formula for great-circle distance on a sphere
 */
ag_geo_coord_result_t ag_geo_point_distance(const ag_geo_point_t* point_a, const ag_geo_point_t* point_b,
											ag_geo_float_t* distance_m);

/**
 * @brief Initialize a geographic line with default/invalid values
 * @param[out] line Pointer to line structure to initialize
 * @return AG_GEO_COORD_SUCCESS on success, error code otherwise
 */
ag_geo_coord_result_t ag_geo_line_init(ag_geo_line_t* line);

/**
 * @brief Validate geographic line
 * @param[in] line Pointer to line to validate
 * @return true if both endpoints are valid, false otherwise
 */
bool ag_geo_line_is_valid(const ag_geo_line_t* line);

/**
 * @brief Initialize a geographic rectangle with default/invalid values
 * @param[out] rect Pointer to rectangle structure to initialize
 * @return AG_GEO_COORD_SUCCESS on success, error code otherwise
 */
ag_geo_coord_result_t ag_geo_rect_init(ag_geo_rect_t* rect);

/**
 * @brief Validate geographic rectangle
 * @param[in] rect Pointer to rectangle to validate
 * @return true if both corners are valid, false otherwise
 */
bool ag_geo_rect_is_valid(const ag_geo_rect_t* rect);

/**
 * @brief Convert result code to human-readable string
 * @param[in] result Result code to convert
 * @return Pointer to constant string describing the result
 */
const char* ag_geo_coord_get_error_string(ag_geo_coord_result_t result);

/*============================================================================*/
/* INLINE HELPER FUNCTIONS                                                    */
/*============================================================================*/

/**
 * @brief Check if latitude is within valid range
 * @param[in] latitude_deg Latitude in decimal degrees
 * @return true if valid, false otherwise
 */
static inline bool ag_geo_coord_is_latitude_valid(ag_geo_float_t latitude_deg) {
	return (latitude_deg >= AG_GEO_COORD_LAT_MIN && latitude_deg <= AG_GEO_COORD_LAT_MAX);
}

/**
 * @brief Check if longitude is within valid range
 * @param[in] longitude_deg Longitude in decimal degrees
 * @return true if valid, false otherwise
 */
static inline bool ag_geo_coord_is_longitude_valid(ag_geo_float_t longitude_deg) {
	return (longitude_deg >= AG_GEO_COORD_LON_MIN && longitude_deg <= AG_GEO_COORD_LON_MAX);
}

#ifdef __cplusplus
}
#endif

#endif /* AG_GEO_COORD_H */
