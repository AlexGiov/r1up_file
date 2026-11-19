#include <stdio.h>
#include <stdlib.h>

#include "r1up_file/r1up_file.h"

int main(int argc, char* argv[]) {
	const char* filename = (argc > 1) ? argv[1] : "RR_0001.rer";
	r1up_file_handle_t handle;
	r1up_file_result_t res;
	r1up_file_size_t sample_count;

	printf("R1UP SDK consumer sample - version: %s\n", R1UP_FILE_VERSION_STRING);
	printf("Opening file: %s\n", filename);

	handle = r1up_file_open(filename, R1UP_FILE_MODE_READ_ONLY);
	if (handle == NULL) {
		printf("ERROR: cannot open file: %s\n", filename);
		return 2;
	}

	res = r1up_file_read_body(handle);
	if (res != R1UP_FILE_SUCCESS) {
		printf("ERROR: r1up_file_read_body failed: %d\n", (int)res);
		(void)r1up_file_close(handle);
		return 3;
	}

	/*
	 * Get the full data of the session, not splited by laps.
	 */
	sample_count = r1up_file_get_body_sample_count(handle);
	if (sample_count == 0U) {
		printf("WARNING: file has zero samples in body.\n");
		(void)r1up_file_close(handle);
		return 0;
	}
	printf("Total samples: %lu\n", (unsigned long)sample_count);

	// ── Get ALL RPM samples via r1up_file_get_var_float (reference) ─────────────
	ag_fixedpoint_float_t* all_data = (ag_fixedpoint_float_t*)malloc(sample_count * sizeof(ag_fixedpoint_float_t));

	/* Get latitude and longitude (this is the only variable that has a dedicated function to read them) */
	res = r1up_file_get_gps_latitude_dd(handle, all_data, sample_count);
	if (res != R1UP_FILE_SUCCESS) {
		printf("ERROR R1UP_FILE_VAR_ID_LATITUDE_DD: %d\n", (int)res);
	}
	res = r1up_file_get_gps_longitude_dd(handle, all_data, sample_count);
	if (res != R1UP_FILE_SUCCESS) {
		printf("ERROR R1UP_FILE_VAR_ID_LONGITUDE_DD: %d\n", (int)res);
	}
	/* Get rpm */
	res = r1up_file_get_var_float(handle, R1UP_FILE_VAR_ID_RPM, all_data, sample_count);
	if (res != R1UP_FILE_SUCCESS) {
		printf("ERROR: r1up_file_get_var_float failed: %d\n", (int)res);
		free(all_data);
		(void)r1up_file_close(handle);
		return 4;
	}
	/* ... do the same for other variables (see below for variable id) */

	// free allocated buffer after use
	free(all_data);

	/*
	 * Get data splitted by laps.
	 */

	uint32_t lap_count = 0U;
	r1up_file_get_lap_count(handle, &lap_count);

	// ── For each segment, verify r1up_file_get_var_float_by_lap matches the slice ─
	// Total segments: outlap (0) + 3 laps (1,2,3) + inlap (4) = 5
	const uint32_t total_segments = lap_count + 2U;	 // outlap + laps + inlap

	for (uint32_t seg = 0U; seg <= total_segments - 1U; seg++) {
		r1up_file_size_t start_sample = 0U, end_sample = 0U;
		res = r1up_file_get_lap_sample_range(handle, seg, &start_sample, &end_sample);

		r1up_file_size_t expected_count = (r1up_file_size_t)(end_sample - start_sample + 1U);
		r1up_file_size_t actual_count = 0U;

		// Allocate buffer for this lap
		ag_fixedpoint_float_t* values = (ag_fixedpoint_float_t*)malloc(expected_count * sizeof(ag_fixedpoint_float_t));

		/* LATITUDE AND LONGITUDE
		 * Note this is are the only variables that have a dedicate function to read them.
		 */
		res = r1up_file_get_gps_latitude_dd_by_lap(handle, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_LATITUDE_DD: %d\n", (int)res);
		}
		res = r1up_file_get_gps_longitude_dd_by_lap(handle, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_LONGITUDE_DD: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_RPM */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_RPM, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_RPM: %d\n", (int)res);
		}
		/* R1UP_FILE_VAR_ID_EXAUST_TEMP */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_EXAUST_TEMP, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_EXAUST_TEMP: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_COOLANT_TEMP */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_COOLANT_TEMP, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_COOLANT_TEMP: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_THROTTLE */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_THROTTLE, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_THROTTLE: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_GEAR */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_GEAR, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_GEAR: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_RPM_HALL */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_RPM_HALL, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_RPM_HALL: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_PWSS_HALL */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_PWSS_HALL, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_PWSS_HALL: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_GPS_SPEED */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_GPS_SPEED, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_GPS_SPEED: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_ALTITUDE */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_ALTITUDE, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_ALTITUDE: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_GPS_SAT_COUNT */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_GPS_SAT_COUNT, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_GPS_SAT_COUNT: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_X_ACC */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_X_ACC, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_X_ACC: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_Y_ACC */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_Y_ACC, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_Y_ACC: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_Z_ACC */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_Z_ACC, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_Z_ACC: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_ROLL */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_ROLL, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_ROLL: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_PITCH */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_PITCH, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_PITCH: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_YAW */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_YAW, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_YAW: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_X_GYRO */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_X_GYRO, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_X_GYRO: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_Y_GYRO */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_Y_GYRO, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_Y_GYRO: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_Z_GYRO */
		res =
			r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_Z_GYRO, seg, values, expected_count, &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_Z_GYRO: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_BATT_VOLT */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_BATT_VOLT, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_BATT_VOLT: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_EXT_PS_VOLT */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_EXT_PS_VOLT, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_EXT_PS_VOLT: %d\n", (int)res);
		}

		/* R1UP_FILE_VAR_ID_INT_TEMP */
		res = r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_INT_TEMP, seg, values, expected_count,
											 &actual_count);
		if (res != R1UP_FILE_SUCCESS) {
			printf("ERROR R1UP_FILE_VAR_ID_INT_TEMP: %d\n", (int)res);
		}
		free(values);
	}

	res = r1up_file_close(handle);
	if (res != R1UP_FILE_SUCCESS) {
		printf("WARNING: r1up_file_close failed: %d\n", (int)res);
	} else {
		printf("File closed successfully.\n");
	}

	return 0;
}
