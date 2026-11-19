/**
 * @file rerace_session.c
 * @brief ReRace Session Information Library - Implementation
 *
 * This file provides session management functionality for ReRace files,
 * including session initialization, payload parsing, and lap data management.
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

#include "rerace_session.h"

#include <string.h>

#include "ag_mal.h"

/**
 * @brief Frees all the laps of the session and resets the list.
 * @param[in,out] session Pointer to the session, must not be NULL.
 * @return true if all the laps have been freed correctly, false otherwise.
 */
static bool rerace_session_free_laps(rerace_session_t* session) {
	bool success = true;
	rerace_session_lap_node_t* current = session->laps_head;

	while (current != NULL) {
		rerace_session_lap_node_t* next = current->next;
		if (AG_FREE(current) != AG_MAL_OK) {
			success = false;
		}
		current = next;
	}

	session->laps_head = NULL;
	session->laps_tail = NULL;
	session->lap_count = 0U;
	return success;
}

/**
 * @brief Allocates a new zeroed lap and appends it at the end of the list of the session.
 * @param[in,out] session Pointer to the session, must not be NULL.
 * @param[out] out_node Pointer that receives the new node, must not be NULL.
 * @return RERACE_SESSION_SUCCESS on success, RERACE_SESSION_ERROR_MEMORY if the allocation fails,
 * RERACE_SESSION_ERROR_CORRUPTED if the session already contains the maximum number of laps.
 * @note On error the session is left unchanged.
 */
static rerace_session_result_t rerace_session_add_lap(rerace_session_t* session, rerace_session_lap_node_t** out_node) {
	if (session->lap_count == UINT8_MAX) {
		return RERACE_SESSION_ERROR_CORRUPTED;
	}

	rerace_session_lap_node_t* node = (rerace_session_lap_node_t*)AG_CALLOC(1U, sizeof(rerace_session_lap_node_t));
	if (node == NULL) {
		return RERACE_SESSION_ERROR_MEMORY;
	}

	node->previous = session->laps_tail;
	node->next = NULL;
	if (session->laps_tail != NULL) {
		session->laps_tail->next = node;
	} else {
		session->laps_head = node;
	}
	session->laps_tail = node;
	session->lap_count++;

	*out_node = node;
	return RERACE_SESSION_SUCCESS;
}

/**
 * @brief Copies size bytes from the payload cursor to dst and advances the cursor, checking the payload bounds.
 * @param[in,out] cursor Current read position in the payload.
 * @param[in] end Pointer to the first byte after the payload.
 * @param[out] dst Destination of the copy.
 * @param[in] size Number of bytes to read.
 * @return true if size bytes were available and have been copied, false (nothing copied) otherwise.
 */
static bool rerace_session_read(const uint8_t** cursor, const uint8_t* end, void* dst, size_t size) {
	if ((size_t)(end - *cursor) < size) {
		return false;
	}
	memcpy(dst, *cursor, size);
	*cursor += size;
	return true;
}

rerace_session_result_t rerace_session_init(rerace_session_t* session, const rerace_track_info_t* track_info) {
	if (session == NULL || track_info == NULL) {
		return RERACE_SESSION_ERROR_INVALID_PARAM;
	}

	if (ag_mal_init() != AG_MAL_OK) {
		return RERACE_SESSION_ERROR_MEMORY;
	}

	// Zeroing the session also resets lap_count, best_lap, best_overall_intermediate_lap and the list pointers
	memset(session, 0, sizeof(*session));
	session->track_info = *track_info;
	return RERACE_SESSION_SUCCESS;
}

rerace_session_result_t rerace_session_deinit(rerace_session_t* session) {
	if (session == NULL) {
		return RERACE_SESSION_ERROR_INVALID_PARAM;
	}

	bool success = rerace_session_free_laps(session);
	memset(session, 0, sizeof(*session));

	if (ag_mal_deinit() != AG_MAL_OK) {
		success = false;
	}
	return success ? RERACE_SESSION_SUCCESS : RERACE_SESSION_ERROR_MEMORY;
}

rerace_session_result_t rerace_session_parse_payload(const void* payload, uint32_t payload_size,
													 rerace_session_t* session) {
	if (payload == NULL || session == NULL || payload_size == 0U) {
		return RERACE_SESSION_ERROR_INVALID_PARAM;
	}

	// Replace any lap already stored in the session
	if (!rerace_session_free_laps(session)) {
		return RERACE_SESSION_ERROR_MEMORY;
	}

	// Read lap number into a local variable: session->lap_count is updated by rerace_session_add_lap()
	const uint8_t* payload_pointer = (const uint8_t*)payload;
	const uint8_t* const payload_end = payload_pointer + payload_size;
	uint8_t payload_lap_count = 0U;
	if (!rerace_session_read(&payload_pointer, payload_end, &payload_lap_count, sizeof(payload_lap_count))) {
		return RERACE_SESSION_ERROR_CORRUPTED;
	}

	for (uint8_t lap_index = 0; lap_index < payload_lap_count; ++lap_index) {
		// allocate a new lap in session
		rerace_session_lap_node_t* lap_node = NULL;
		rerace_session_result_t result = rerace_session_add_lap(session, &lap_node);
		if (result != RERACE_SESSION_SUCCESS) {
			// No partial state: release the laps added so far
			(void)rerace_session_free_laps(session);
			return result;
		}

		// Read lap data for each lap
		for (uint8_t intermediate_index = 0; intermediate_index < RERACE_SESSION_LAP_TIMES_ARRAY_SIZE;
			 ++intermediate_index) {
			// copy intermediate lap times from payload
			if (!rerace_session_read(&payload_pointer, payload_end, &lap_node->lap.lap_times[intermediate_index],
									 sizeof(lap_node->lap.lap_times[intermediate_index]))) {
				(void)rerace_session_free_laps(session);
				return RERACE_SESSION_ERROR_CORRUPTED;
			}
		}

		// copy rpm_max and max speed from payload
		if (!rerace_session_read(&payload_pointer, payload_end, &lap_node->lap.rpm_max,
								 sizeof(lap_node->lap.rpm_max)) ||
			!rerace_session_read(&payload_pointer, payload_end, &lap_node->lap.speed_max,
								 sizeof(lap_node->lap.speed_max))) {
			(void)rerace_session_free_laps(session);
			return RERACE_SESSION_ERROR_CORRUPTED;
		}
	}

	// copy best lap data from payload
	for (uint8_t intermediate_index = 0; intermediate_index < RERACE_SESSION_LAP_TIMES_ARRAY_SIZE;
		 ++intermediate_index) {
		if (!rerace_session_read(&payload_pointer, payload_end, &session->best_lap.lap_times[intermediate_index],
								 sizeof(session->best_lap.lap_times[intermediate_index]))) {
			(void)rerace_session_free_laps(session);
			return RERACE_SESSION_ERROR_CORRUPTED;
		}
	}

	// copy best overall intermediate lap data from payload
	for (uint8_t intermediate_index = 0; intermediate_index < RERACE_SESSION_LAP_TIMES_ARRAY_SIZE;
		 ++intermediate_index) {
		if (!rerace_session_read(&payload_pointer, payload_end,
								 &session->best_overall_intermediate_lap.lap_times[intermediate_index],
								 sizeof(session->best_overall_intermediate_lap.lap_times[intermediate_index]))) {
			(void)rerace_session_free_laps(session);
			return RERACE_SESSION_ERROR_CORRUPTED;
		}
	}

	// copy file pointer from payload: the payload holds lap_count - 1 file pointers, the first lap has none (it stays 0)
	rerace_session_lap_node_t* file_pointer_node = (session->laps_head != NULL) ? session->laps_head->next : NULL;
	while (file_pointer_node != NULL) {
		if (!rerace_session_read(&payload_pointer, payload_end, &file_pointer_node->lap.file_pointer,
								 sizeof(file_pointer_node->lap.file_pointer))) {
			(void)rerace_session_free_laps(session);
			return RERACE_SESSION_ERROR_CORRUPTED;
		}
		file_pointer_node = file_pointer_node->next;
	}

	return RERACE_SESSION_SUCCESS;
}