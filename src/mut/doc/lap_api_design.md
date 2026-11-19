# Lap-by-Lap Data Access API — Design Document

## Goal

Provide an interface to:
1. **WRITE mode**: record lap boundary markers during live data acquisition (finish line crossings)
2. **READ mode**: retrieve body data from an R1UP file on a lap-by-lap basis

---

## Part 1 — WRITE Mode: Recording Lap Markers

### Context

During a track session the application logs sensor data continuously via `r1up_file_set_var()` + `r1up_file_update_vars()`. The lap timing algorithm detects finish line crossings asynchronously. At each crossing, a **lap marker** must be saved so that the data can later be sliced per lap.

### Data Synchronization — Deferred Flag Pattern

`r1up_file_set_var()` is a **memory-only** operation: it copies a variable's value into `vsd_write_buffer[pos]` with `memcpy`. No file I/O occurs. `r1up_file_update_vars()` is the **commit** of sample $n$: it writes all variables to the file (respecting each variable's `fd`), calls `fsal_flush()`, then increments `current_sample_number++`.

The application loop is:

```
for each sample n:
    set_var(handle, VAR_A, &data_a[n])   // memory only
    set_var(handle, VAR_B, &data_b[n])   // memory only
    ...
    update_vars(handle)                   // flush to disk, n++
```

**Design decision**: `r1up_file_mark_lap()` does NOT perform any I/O or allocations. It **only raises a boolean flag** (`lap_mark_pending`). Then `r1up_file_update_vars()`, after writing the sample data and before incrementing `current_sample_number`, checks and processes the flag:

```c
// Inside r1up_file_update_vars(), after fsal_write + fsal_flush:
if (file->lap_mark_pending) {
    file->lap_mark_pending = false;           // consume flag
    r1up_lap_marker_t marker;
    marker.sample_index = file->current_sample_number;  // = n (last written sample)
    marker.file_offset  = fsal_tell(file->fsal_handle_ptr);  // first byte AFTER sample n
    // realloc + append marker to lap_table
    ...
}
file->current_sample_number++;
```

**Why this approach**:
- **Perfect synchronization**: the marker is recorded exactly when `current_sample_number` is coherent and all variables of sample $n$ have been written to disk.
- **Atomicity**: marker creation happens inside the same logical "commit" as the data write.
- **Correct file offset**: `fsal_tell()` after writing sample $n$ gives the byte position of the boundary between sample $n$ and $n+1$, which is the semantically correct lap boundary.
- **Simplicity for the caller**: `mark_lap()` can be called at any point during the sampling cycle — before, between, or after `set_var()` calls — the effect is always deferred to the next `update_vars()`.

**Semantic**: the marker indicates "the finish line was crossed between sample $n$ (current) and sample $n-1$ (previous)". The marker's `sample_index` = $n$ = first sample of the NEW lap.

### ISR Safety — Atomicity Requirement

**`r1up_file_mark_lap()` will be called from an ISR** (Interrupt Service Routine) on the target platform. The flag read/write **must be atomic**. For now the flag is a plain `bool`, with a `// TODO:` comment marking the atomicity requirement. The actual mechanism will be chosen based on the target platform:

| Platform                | Atomic mechanism                                                          |
| ----------------------- | ------------------------------------------------------------------------- |
| FreeRTOS                | `portENTER_CRITICAL()` / `portEXIT_CRITICAL()`, or `taskENTER_CRITICAL()` |
| ARM Cortex-M bare metal | `volatile` + single-byte write (naturally atomic on ARM)                  |
| POSIX / unit tests      | `volatile bool` or `<stdatomic.h>` `atomic_flag`                          |

```c
// In r1up_file_t:
bool lap_mark_pending;   // TODO: make atomic for ISR safety (platform-dependent)
```

### Lap Marker Structure

```c
/**
 * @brief Single lap boundary marker, recorded at each finish line crossing.
 */
typedef struct {
    uint64_t sample_index;   // body sample number at the finish line crossing
    uint64_t file_offset;    // byte offset in the file at that sample (for fast seek on read)
} r1up_lap_marker_t;
```

### Dynamic Lap Table (runtime, inside `r1up_file_t`)

Since the number of laps is unknown at session start, markers are allocated one at a time
(laps last tens of seconds, so allocation frequency is negligible):

```c
/**
 * @brief Dynamic array of lap markers, grows one element at a time.
 */
typedef struct {
    r1up_lap_marker_t* markers;   // dynamically allocated array (realloc on each new lap)
    uint32_t count;               // number of markers recorded (= finish line crossings)
    uint32_t capacity;            // current allocated slots (grow strategy: +1 per lap, or small batch)
} r1up_lap_table_t;
```

New fields to add to `r1up_file_t`:

```c
bool              lap_mark_pending;  // flag raised by mark_lap(), consumed by update_vars()
                                     // TODO: make atomic for ISR safety (platform-dependent)
r1up_lap_table_t  lap_table;          // lap markers (WRITE mode only at runtime)
```

### Available State in `r1up_file_t`

| Field                   | Already present? | Notes                                             |
| ----------------------- | :--------------: | ------------------------------------------------- |
| `current_sample_number` |       YES        | Incremented by `r1up_file_update_vars()`          |
| current file offset     |      **NO**      | Must be obtained via `fsal_tell()` at marker time |

### WRITE API

```c
/**
 * @brief Request a lap boundary marker at the current sample position.
 *
 * Called by the application's lap timing algorithm when the finish line is crossed.
 * This function ONLY raises an internal flag (lap_mark_pending). The actual marker
 * recording (sample_index, file_offset, realloc) is performed by r1up_file_update_vars()
 * on the next call, ensuring perfect data synchronization.
 *
 * This function is ISR-safe: it performs no I/O, no allocation, no locking.
 * The flag write must be atomic on the target platform (see ISR Safety section).
 *
 * @param handle  Valid file handle opened in WRITE mode
 * @return R1UP_FILE_SUCCESS, or error if handle is NULL / wrong mode / body already closed
 */
r1up_file_result_t r1up_file_mark_lap(r1up_file_handle_t handle);
```

> **Note**: if `mark_lap()` is called multiple times before the next `update_vars()`, only
> **one** marker is recorded (the flag is idempotent). This is correct because multiple
> crossings within a single sample period are physically impossible at racing speeds.

### Persistence (footer serialization)

At `r1up_file_close()` the lap table is serialized into a **dedicated footer block**:

- **Block ID**: `R1UP_FILE_LAP_ACCESS_TABLE_BID` (16-char padded: `"LAP_ACCESS_TABLE"`)
- **Binary format** (little-endian, packed):

```
Offset  Size     Field
------  ------   ---------------------
0       4        marker_count (uint32_t)
4       16       marker[0] { sample_index: uint64, file_offset: uint64 }
20      16       marker[1]
...
4+16*N  —        end
```

Total payload size: `4 + 16 * marker_count` bytes.

### Lap ↔ Marker Mapping

```
Markers:       M0          M1          M2              (count = 3)
Samples:  |---outlap---|---lap 1---|---lap 2---|---inlap---|
Lap #:         0            1           2           3
Range:    [0, M0)      [M0, M1)    [M1, M2)    [M2, total)
```

- **Outlap (lap 0)**: from sample 0 to first marker (exclusive)
- **Timed laps**: between consecutive markers
- **Inlap (last lap)**: from last marker to total sample count
- **Total laps** = `marker_count + 1`

The sample at the finish line belongs to the **start** of the next lap (`[start, end)` convention).

---

## Part 2 — READ Mode: Retrieving Data by Lap

### Deserialization

When reading a file, the `R1UP_FILE_LAP_ACCESS_TABLE_BID` block is parsed (during `r1up_file_read_body()` or lazily on first lap access) to reconstruct the `r1up_lap_table_t` in memory.

### READ API

#### Low-level: sample range per lap

```c
r1up_file_result_t r1up_file_get_lap_count(
    r1up_file_handle_t handle,
    uint32_t* lap_count             // returns marker_count + 1
);

r1up_file_result_t r1up_file_get_lap_sample_range(
    r1up_file_handle_t handle,
    uint32_t lap_number,
    uint64_t* start_sample,         // inclusive
    uint64_t* end_sample            // exclusive
);
// samples_in_lap = end_sample - start_sample
```

#### High-level: float data per lap and variable

```c
r1up_file_result_t r1up_file_get_lap_sample_count(
    r1up_file_handle_t handle,
    uint32_t lap_number,
    uint64_t* sample_count
);

r1up_file_result_t r1up_file_get_var_float_by_lap(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    uint32_t lap_number,
    ag_fixedpoint_float_t* buffer,  // caller-allocated
    uint64_t buffer_size            // must be >= lap sample count
);
```

### Typical READ usage flow

```c
r1up_file_handle_t handle = r1up_file_open(filename, R1UP_FILE_MODE_READ_ONLY);
r1up_file_result_t result = r1up_file_read_body(handle);

uint32_t lap_count = 0;
r1up_file_get_lap_count(handle, &lap_count);

for (uint32_t lap = 0; lap < lap_count; lap++) {
    uint64_t samples = 0;
    r1up_file_get_lap_sample_count(handle, lap, &samples);

    ag_fixedpoint_float_t* rpm = malloc(samples * sizeof(ag_fixedpoint_float_t));
    r1up_file_get_var_float_by_lap(handle, R1UP_FILE_VAR_ID_RPM, lap, rpm, samples);

    // ... process lap data ...
    free(rpm);
}

r1up_file_close(handle);
```

### Typical WRITE usage flow

```c
r1up_file_handle_t handle = r1up_file_open(filename, R1UP_FILE_MODE_WRITE_ONLY);
r1up_file_close_header(handle);

for (uint32_t i = 0; i < total_samples; i++) {
    // Application or ISR may call mark_lap() at any time:
    if (finish_line_crossed(i)) {
        r1up_file_mark_lap(handle);   // <-- raises flag only (ISR-safe, O(1), no I/O)
    }

    // Set all variables for sample i...
    r1up_file_set_var(handle, VAR_A, &data_a[i]);
    r1up_file_set_var(handle, VAR_B, &data_b[i]);
    // ...

    // Commit sample i to file. If lap_mark_pending is set, the marker is
    // recorded here with correct sample_index and file_offset, then flag is cleared.
    r1up_file_update_vars(handle);
}

r1up_file_close(handle);  // serializes lap table into footer block
```

---

## Error Codes

| Code                            | Condition                                                                            |
| ------------------------------- | ------------------------------------------------------------------------------------ |
| `R1UP_FILE_SUCCESS`             | Operation completed successfully                                                     |
| `R1UP_FILE_ERROR_INVALID_PARAM` | NULL pointer or `buffer_size` too small                                              |
| `R1UP_FILE_ERROR_NOT_FOUND`     | `lap_number >= lap_count` or variable not in file, or no lap table in file           |
| `R1UP_FILE_ERROR_INVALID_STATE` | Wrong mode (e.g. `mark_lap` in READ mode, `get_var_float_by_lap` before `read_body`) |
| `R1UP_FILE_ERROR_MEMORY`        | `realloc` failed when appending a marker                                             |

---

## Implementation Checklist

1. [ ] Add `r1up_lap_marker_t`, `r1up_lap_table_t` typedefs to `r1up_file.c` (private types)
2. [ ] Add `r1up_lap_table_t lap_table` field to `r1up_file_t`
3. [ ] Initialize `lap_table` in `r1up_file_init_structure()` (zero/NULL)
4. [ ] Register new block ID `R1UP_FILE_LAP_ACCESS_TABLE_BID` in blocks table
5. [ ] Implement `r1up_file_mark_lap()` — raise `lap_mark_pending` flag (ISR-safe, no I/O)
5b. [ ] Add deferred marker processing in `r1up_file_update_vars()` — check flag, record `current_sample_number` + `fsal_tell()`, realloc + append
6. [ ] Serialize lap table in `r1up_file_close_footer()` into the dedicated block
7. [ ] Deserialize lap table on read (in `r1up_file_read_body()` or lazy)
8. [ ] Implement `r1up_file_get_lap_count()`
9. [ ] Implement `r1up_file_get_lap_sample_range()`
10. [ ] Implement `r1up_file_get_lap_sample_count()` (wrapper)
11. [ ] Implement `r1up_file_get_var_float_by_lap()`
12. [ ] Free `lap_table.markers` in `r1up_file_deinit_all()`
13. [ ] Write unit tests
