/**
 * @file rerace_track.c
 * @brief ReRace Track Information Library - Public API
 *
 * This file provides track management functionality for ReRace files,
 * including track name extraction, validation, and formatting.
 *
 * @author Alex Giov
 * @date November 14, 2026
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026, Race1Up Project.
 * All rights reserved.
 *
 * @par License:
 * This software is proprietary and confidential.
 */

#include "rerace_track.h"

#include <string.h>

rerace_track_result_t rerace_track_parse_payload(const void* payload, uint32_t payload_size,
												 rerace_track_info_t* track_info) {
	// The payload uses the serialized legacy layout, not the in-memory layout of rerace_track_info_t (which is larger
	// due to padding and extra fields), so the minimum size is the sum of the fields read below:
	// id, name, date, time, 2 corners (lon+lat), is_open, line count, and the finish line plus all the intermediate
	// timing lines (2 points, lon+lat).
	const size_t min_payload_size = sizeof(track_info->track_id) + sizeof(track_info->track_name) +
									sizeof(track_info->acq_utc_date) + sizeof(track_info->acq_utc_time) +
									(2U * 2U * sizeof(ag_geo_float_t)) + sizeof(uint8_t) +
									sizeof(track_info->intermediate_line_count) +
									((RERACE_TRACK_MAX_INTERMEDIATE_TIME_LINES + 1U) * 2U * 2U * sizeof(ag_geo_float_t));
	if (payload == NULL || track_info == NULL || ((size_t)payload_size < min_payload_size)) {
		return RERACE_TRACK_ERROR_INVALID_PARAM;
	}

	// Copy one member at time from payload to track_info, due the structure is slightly different in data types
	void* payload_ptr = (void*)payload;
	// track id
	memcpy(&track_info->track_id, payload_ptr, sizeof(track_info->track_id));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->track_id);

	// track name
	memcpy(&track_info->track_name, payload_ptr, sizeof(track_info->track_name));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->track_name);

	// acquisition date
	memcpy(&track_info->acq_utc_date, payload_ptr, sizeof(track_info->acq_utc_date));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->acq_utc_date);

	// acquisition time
	memcpy(&track_info->acq_utc_time, payload_ptr, sizeof(track_info->acq_utc_time));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->acq_utc_time);

	// track square corner A
	memcpy(&track_info->track_zone.corner_a.longitude_deg, payload_ptr,
		   sizeof(track_info->track_zone.corner_a.longitude_deg));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->track_zone.corner_a.longitude_deg);

	memcpy(&track_info->track_zone.corner_a.latitude_deg, payload_ptr,
		   sizeof(track_info->track_zone.corner_a.latitude_deg));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->track_zone.corner_a.latitude_deg);

	// track square corner B (the opposite corner, named corner_c in ag_geo_rect_t)
	memcpy(&track_info->track_zone.corner_c.longitude_deg, payload_ptr,
		   sizeof(track_info->track_zone.corner_c.longitude_deg));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->track_zone.corner_c.longitude_deg);

	memcpy(&track_info->track_zone.corner_c.latitude_deg, payload_ptr,
		   sizeof(track_info->track_zone.corner_c.latitude_deg));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->track_zone.corner_c.latitude_deg);

	// Fields not stored in the payload
	track_info->track_zone.corner_a.altitude_m = 0;
	track_info->track_zone.corner_a.is_valid = true;
	track_info->track_zone.corner_c.altitude_m = 0;
	track_info->track_zone.corner_c.is_valid = true;

	// track open or closed circuit
	uint8_t is_open;
	memcpy(&is_open, payload_ptr, sizeof(is_open));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(is_open);
	track_info->is_open = (bool)is_open;

	// intermediate line count
	memcpy(&track_info->intermediate_line_count, payload_ptr, sizeof(track_info->intermediate_line_count));
	payload_ptr = (uint8_t*)payload_ptr + sizeof(track_info->intermediate_line_count);

	if (track_info->intermediate_line_count > RERACE_TRACK_MAX_INTERMEDIATE_TIME_LINES)
		return RERACE_TRACK_ERROR_CORRUPTED;

	// The payload always contains the finish line (index 0) followed by all the intermediate lines. Entries with an
	// index greater than track_info->intermediate_line_count are filled with zeroed entries.
	for (int i = 0; i < RERACE_TRACK_MAX_INTERMEDIATE_TIME_LINES + 1; i++) {
		ag_geo_line_t* line = &track_info->timing_lines[i];

		memcpy(&line->point_a.longitude_deg, payload_ptr, sizeof(line->point_a.longitude_deg));
		payload_ptr = (uint8_t*)payload_ptr + sizeof(line->point_a.longitude_deg);

		memcpy(&line->point_a.latitude_deg, payload_ptr, sizeof(line->point_a.latitude_deg));
		payload_ptr = (uint8_t*)payload_ptr + sizeof(line->point_a.latitude_deg);

		memcpy(&line->point_b.longitude_deg, payload_ptr, sizeof(line->point_b.longitude_deg));
		payload_ptr = (uint8_t*)payload_ptr + sizeof(line->point_b.longitude_deg);

		memcpy(&line->point_b.latitude_deg, payload_ptr, sizeof(line->point_b.latitude_deg));
		payload_ptr = (uint8_t*)payload_ptr + sizeof(line->point_b.latitude_deg);

		line->point_a.altitude_m = 0;
		line->point_b.altitude_m = 0;
		line->point_a.is_valid = (i <= track_info->intermediate_line_count);
		line->point_b.is_valid = (i <= track_info->intermediate_line_count);
	}

	// Ensure all the strings are null-terminated
	track_info->track_name[sizeof(track_info->track_name) - 1] = '\0';
	track_info->acq_utc_date[sizeof(track_info->acq_utc_date) - 1] = '\0';
	track_info->acq_utc_time[sizeof(track_info->acq_utc_time) - 1] = '\0';

	return RERACE_TRACK_SUCCESS;
}