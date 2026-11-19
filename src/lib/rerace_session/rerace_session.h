/**
 * @file rerace_session.h
 * @brief ReRace Session Information Library - Public API
 *
 * This header provides track management functionality for ReRace files,
 * including track name extraction, validation, and formatting.
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
 */

#ifndef RERACE_SESSION_H
#define RERACE_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "rerace_track.h"

/**
 * @brief Result codes for ReRace session operations
 */
typedef enum {
	RERACE_SESSION_SUCCESS = 0,			 ///< Operation completed successfully
	RERACE_SESSION_ERROR_INVALID_PARAM,	 ///< Invalid parameter passed
	RERACE_SESSION_ERROR_NOT_FOUND,		 ///< Session information not found
	RERACE_SESSION_ERROR_MEMORY,		 ///< Memory allocation failed
	RERACE_SESSION_ERROR_CORRUPTED		 ///< Session data is corrupted
} rerace_session_result_t;

/**
 * @brief The size of the array containing a single lap's intermediate times.
 */
#define RERACE_SESSION_LAP_TIMES_ARRAY_SIZE (RERACE_TRACK_MAX_INTERMEDIATE_TIME_LINES + 1)

/**
 * @brief Array type representing the intermediate times for a single lap.
 * @details The first element of the array, index 0, represents the overall lap time, while the subsequent elements
 * correspond to the intermediate times defined by the track.
 */
typedef float rerace_session_lap_times[RERACE_SESSION_LAP_TIMES_ARRAY_SIZE];

/**
 * @brief Structure representing the data for a single lap in a ReRace session.
 */
typedef struct {
	rerace_session_lap_times lap_times; /**< Array of intermediate times for a single lap */
	uint16_t rpm_max;					/**< Maximum RPM recorded for the lap */
	uint16_t speed_max;					/**< Maximum speed recorded for the lap */
	uint32_t file_pointer;				/**< File pointer associated with the lap data */
} rerace_session_lap_data_t;

/**
 * @brief Node of the doubly linked list containing the laps of a ReRace session.
 */
typedef struct rerace_session_lap_node {
	rerace_session_lap_data_t lap;			  /**< Data of the lap */
	struct rerace_session_lap_node* next;	  /**< Next lap in the session, NULL for the last lap */
	struct rerace_session_lap_node* previous; /**< Previous lap in the session, NULL for the first lap */
} rerace_session_lap_node_t;

/**
 * @brief Structure representing a ReRace session.
 */
typedef struct {
	rerace_track_info_t track_info;		/**< Information about the track */
	uint8_t lap_count;					/**< Number of laps in the session, i.e. the number of nodes of the list */
	rerace_session_lap_data_t best_lap; /**< Data for the best lap in the session */
	rerace_session_lap_data_t
		best_overall_intermediate_lap; /**< Data for the best intermediate lap in the session, i.e. the sum of all the
										  best intermediate in the session */
	rerace_session_lap_node_t* laps_head; /**< First lap of the session, NULL when lap_count is 0 */
	rerace_session_lap_node_t* laps_tail; /**< Last lap of the session, NULL when lap_count is 0 (allows O(1) append) */
} rerace_session_t;

/**
 * @brief Initializes a ReRace session.
 * @details The session structure is owned by the caller (it can be a local or static variable), while the list of laps
 * is allocated dynamically through the ag_mal library. This function initializes ag_mal, so every successful call must
 * be matched by exactly one call to rerace_session_deinit().
 * @param[out] session Pointer to the session structure to initialize.
 * @param[in] track_info Pointer to the track information associated with the session, copied into the session.
 * @return RERACE_SESSION_SUCCESS on success, RERACE_SESSION_ERROR_INVALID_PARAM if a pointer is NULL,
 * RERACE_SESSION_ERROR_MEMORY if ag_mal cannot be initialized.
 * @note Do not copy a session by value: the copy would share the same list of laps.
 */
rerace_session_result_t rerace_session_init(rerace_session_t* session, const rerace_track_info_t* track_info);

/**
 * @brief Releases all the resources of a ReRace session.
 * @details Frees every lap of the list, resets the session and deinitializes ag_mal.
 * @param[in,out] session Pointer to a session initialized with rerace_session_init().
 * @return RERACE_SESSION_SUCCESS on success, RERACE_SESSION_ERROR_INVALID_PARAM if session is NULL,
 * RERACE_SESSION_ERROR_MEMORY if a memory operation fails.
 */
rerace_session_result_t rerace_session_deinit(rerace_session_t* session);

/**
 * @brief Parses the payload of a ReRace session.
 * @details Any lap previously stored in the session is released and replaced by the laps contained in the payload. On
 * error the session is left without laps.
 * @param[in] payload Pointer to the session payload data.
 * @param[in] payload_size Size of the payload data in bytes.
 * @param[in,out] session Pointer to a session initialized with rerace_session_init().
 * @return RERACE_SESSION_SUCCESS on success, an error code otherwise.
 */
rerace_session_result_t rerace_session_parse_payload(const void* payload, uint32_t payload_size,
													 rerace_session_t* session);

#ifdef __cplusplus
}
#endif

#endif /* RERACE_SESSION_H */
