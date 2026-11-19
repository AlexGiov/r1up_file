# R1UP Variable Stream Format - Technical Specification

**Version:** 2.0  
**Date:** January 26, 2026  
**Author:** Alex Giov  
**Status:** Design Document

---

## Table of Contents

1. [Overview](#overview)
2. [Type System Refactoring](#type-system-refactoring)
3. [Self-Describing Format Architecture](#self-describing-format-architecture)
4. [Binary Format Specification](#binary-format-specification)
5. [Data Structures](#data-structures)
6. [Implementation Prototype](#implementation-prototype)
7. [Migration Strategy](#migration-strategy)
8. [Examples](#examples)

---

## 1. Overview

This document describes the Variable Stream Format (VSF) used in R1UP telemetry files. It covers:

- **Type System Unification** (completed): Simplified fixed-point type representation
- **Self-Describing Format** (proposed): Flexible variable ordering through lookup tables

### Goals

✅ **Flexibility**: Support arbitrary variable ordering without breaking compatibility  
✅ **Extensibility**: Add/remove variables without regenerating entire codebase  
✅ **Simplicity**: Single unified type system eliminates redundancy  
✅ **Performance**: Efficient binary encoding for embedded systems  

---

## 2. Type System Refactoring

### 2.1 Previous Design (Legacy)

Variables were defined with **redundant type information**:

```c
// Old VSF Entry (6 parameters)
VSF_ENTRY(RPM_ID, R1UP_FILE_UINT16, uint16_t, 1U, rpm, uq16_0)
          ^^^^^^  ^^^^^^^^^^^^^^^^  ^^^^^^^^       ^^^  ^^^^^^
          var_id  generic_type_id   C_base_type    fd   q_format
```

**Problems:**
- Three type-related fields (`R1UP_FILE_UINT16`, `uint16_t`, `uq16_0`)
- Manual coordination between Type ID and Q-format suffix
- Verbose macro expansions

### 2.2 New Design (Implemented)

**Unified type representation** with semantic Type IDs:

```c
// New VSF Entry (5 parameters)
VSF_ENTRY(RPM_ID, R1UP_FILE_UQ16_0, uq16_0, 1U, rpm)
          ^^^^^^  ^^^^^^^^^^^^^^^^^  ^^^^^^  ^^  ^^^
          var_id  semantic_type_id   q_type  fd  name
```

**Key Changes:**
- Single `type` parameter encodes Q-format notation (e.g., `uq16_0`)
- Type ID directly indicates format (e.g., `R1UP_FILE_UQ16_0`)
- Complete type constructed via macro: `ag_fixedpoint_##type##_t`

### 2.3 Type ID Table

| Type ID Symbol     | Value | C Type                   | Description                            |
| ------------------ | ----- | ------------------------ | -------------------------------------- |
| `R1UP_FILE_UQ8_0`  | 0     | `ag_fixedpoint_uq8_0_t`  | Unsigned 8-bit, Q8.0 (0-255)           |
| `R1UP_FILE_UQ16_0` | 1     | `ag_fixedpoint_uq16_0_t` | Unsigned 16-bit, Q16.0                 |
| `R1UP_FILE_UQ32_0` | 2     | `ag_fixedpoint_uq32_0_t` | Unsigned 32-bit, Q32.0                 |
| `R1UP_FILE_Q7_0`   | 3     | `ag_fixedpoint_q7_0_t`   | Signed 8-bit, Q7.0 (-128..127)         |
| `R1UP_FILE_Q15_0`  | 4     | `ag_fixedpoint_q15_0_t`  | Signed 16-bit, Q15.0                   |
| `R1UP_FILE_Q31_0`  | 5     | `ag_fixedpoint_q31_0_t`  | Signed 32-bit, Q31.0                   |
| `R1UP_FILE_UQ12_4` | 6     | `ag_fixedpoint_uq12_4_t` | Unsigned 16-bit, Q12.4 (0-4095.9375)   |
| `R1UP_FILE_UQ10_6` | 7     | `ag_fixedpoint_uq10_6_t` | Unsigned 16-bit, Q10.6 (0-1023.984375) |
| `R1UP_FILE_UQ6_10` | 8     | `ag_fixedpoint_uq6_10_t` | Unsigned 16-bit, Q6.10 (0-63.999)      |
| `R1UP_FILE_UQ4_12` | 9     | `ag_fixedpoint_uq4_12_t` | Unsigned 16-bit, Q4.12 (0-15.9998)     |
| `R1UP_FILE_Q4_11`  | 10    | `ag_fixedpoint_q4_11_t`  | Signed 16-bit, Q4.11 (-16..15.9995)    |
| `R1UP_FILE_Q3_12`  | 11    | `ag_fixedpoint_q3_12_t`  | Signed 16-bit, Q3.12 (-8..7.9998)      |
| `R1UP_FILE_Q3_28`  | 12    | `ag_fixedpoint_q3_28_t`  | Signed 32-bit, Q3.28 (-8..7.999999)    |

**Mapping Rules:**
- Type ID name uses uppercase Q-notation (e.g., `R1UP_FILE_UQ12_4`)
- C typedef uses lowercase with `ag_fixedpoint_` prefix and `_t` suffix
- Conversion functions: `ag_fixedpoint_<format>_to_float()` (no `_t`)

### 2.4 Macro System

Complete type is constructed via token pasting:

```c
#define VSF_ENTRY(var_id, type_id, type, freq_div, name) \
    /* Example: type = uq12_4 */ \
    ag_fixedpoint_##type##_t variable; // Expands to: ag_fixedpoint_uq12_4_t
    ag_fixedpoint_##type##_to_float(value); // Expands to: ag_fixedpoint_uq12_4_to_float()
```

**Benefits:**
- Automatic type construction
- No manual coordination needed
- Compiler-enforced type safety

---

## 3. Self-Describing Format Architecture

### 3.1 Problem Statement

Current R1UP files have a **fixed variable order** defined at compile-time in `r1up_file_cfg.h`:

```c
VSF_ENTRY(RPM_ID,       R1UP_FILE_UQ16_0, uq16_0, 1U,  rpm)      // Position 0
VSF_ENTRY(EXAUSTEMP_ID, R1UP_FILE_UQ12_4, uq12_4, 1U,  exaust)   // Position 1
VSF_ENTRY(FLUIDTEMP_ID, R1UP_FILE_UQ10_6, uq10_6, 10U, coolant)  // Position 2
// ... always in this order
```

**Limitations:**
- Reordering variables requires recompiling reader
- Adding/removing variables breaks old files
- Optional variables waste space (must write zeros)

### 3.2 Proposed Solution: Lookup Table

**Concept:** Each file contains a **Variable Stream Format (VSF) Lookup Table** mapping:

```
Variable ID (semantic) → Position in data stream (physical)
```

**Example:**
```
File A writes: [RPM, TEMP, SPEED]  → Positions [0, 1, 2]
File B writes: [SPEED, RPM]        → Positions [0, 1]
File C writes: [TEMP, RPM, SPEED]  → Positions [0, 1, 2]
```

All files are valid! Reader reconstructs the mapping from the lookup table.

### 3.3 Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    R1UP FILE STRUCTURE                  │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  HEADER SECTION                                         │
│  ├─ R1UP_FILE_HEADER (total header size)               │
│  ├─ R1UP_FILE_VER                                       │
│  ├─ R1UP_FILE_HW_VER                                    │
│  ├─ ...                                                 │
│  ├─ R1UP_FILE_VARS_STREAM_FORMAT ◄── LOOKUP TABLE HERE │
│  │  ┌─────────────────────────────────────────────┐    │
│  │  │ uint16_t num_variables = N                  │    │
│  │  ├─────────────────────────────────────────────┤    │
│  │  │ For each variable (i = 0..N-1):             │    │
│  │  │   uint16_t variable_id      (semantic ID)   │    │
│  │  │   uint8_t  type_id          (Q-format type) │    │
│  │  │   uint16_t frequency_div    (sampling rate) │    │
│  │  │   uint8_t  position         (stream index)  │    │
│  │  └─────────────────────────────────────────────┘    │
│  └─ R1UP_FILE_DATA_FORMAT                               │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  BODY SECTION                                           │
│  └─ R1UP_FILE_BODY                                      │
│     ┌───────────────────────────────────────────────┐  │
│     │ Sample 0:                                     │  │
│     │   var[position=0] value (type from lookup)   │  │
│     │   var[position=1] value (type from lookup)   │  │
│     │   var[position=2] value (type from lookup)   │  │
│     │   ...                                         │  │
│     ├───────────────────────────────────────────────┤  │
│     │ Sample 1:                                     │  │
│     │   var[position=0] value                       │  │
│     │   var[position=1] value (or skip if fd > 1)  │  │
│     │   ...                                         │  │
│     └───────────────────────────────────────────────┘  │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  FOOTER SECTION                                         │
│  └─ R1UP_FILE_FOOTER                                    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 4. Binary Format Specification

### 4.1 VARS_STREAM_FORMAT Block

**Block ID:** `"R1UP_VSF______"`  
**Payload Structure:**

```c
struct r1up_vsf_block_payload {
    uint16_t num_variables;           // Total number of variables in stream
    
    // Variable descriptors (repeated num_variables times)
    // IMPORTANT: Array index IS the position (0, 1, 2, ...)
    //            The variable_id field tells WHICH variable is at that position
    struct {
        uint16_t variable_id;         // Semantic ID (e.g., RPM_ID = 5)
        uint8_t  type_id;             // Data type (e.g., R1UP_FILE_UQ12_4 = 6)
        uint16_t frequency_divider;   // Sampling rate divisor (1 = every sample)
    } variables[num_variables];       // Variable-length array
};
```

**Size calculation:**
```c
payload_size = sizeof(uint16_t)                           // num_variables
             + num_variables * (2 + 1 + 2)                // 5 bytes per variable
             = 2 + (num_variables * 5)
```

**Example (3 variables):**
```
**Example (3 variables in order: RPM, EXAUSTEMP, GPS_VEL):**

| Offset | Size | Field                | Value (hex) | Value (dec) | Meaning             |
| ------ | ---- | -------------------- | ----------- | ----------- | ------------------- |
| 0x00   | 2    | num_variables        | 0x0003      | 3           |                     |
| 0x02   | 2    | variable_id[0]       | 0x0005      | 5           | Position 0 = RPM    |
| 0x04   | 1    | type_id[0]           | 0x06        | 6           | UQ12_4              |
| 0x05   | 2    | frequency_divider[0] | 0x0001      | 1           | Every sample        |
| 0x07   | 2    | variable_id[1]       | 0x000D      | 13          | Position 1 = EXAUST |
| 0x09   | 1    | type_id[1]           | 0x06        | 6           | UQ12_4              |
| 0x0A   | 2    | frequency_divider[1] | 0x0001      | 1           | Every sample        |
| 0x0C   | 2    | variable_id[2]       | 0x0014      | 20          | Position 2 = GPS    |
| 0x0E   | 1    | type_id[2]           | 0x01        | 1           | UQ16_0              |
| 0x0F   | 2    | frequency_divider[2] | 0x000A      | 10          | Every 10th sample   |

**Note:** Array index = position. `variables[0]` is first in file, `variables[1]` is second, etc.  
The `variable_id` field tells you WHICH semantic variable is at that position
```

### 4.2 Byte Order

**Little-endian** for all multi-byte fields (uint16_t, uint32_t).

### 4.3 Alignment

No padding - fields are **packed** to minimize file size.

---

## 5. Data Structures

### 5.1 In-Memory VSF Entry

```c
/**
 * @brief Variable Stream Format entry (runtime representation)
 * @note The VSF array is indexed by PHYSICAL POSITION in file, not by variable ID!
 *       vsf[0] = first variable in file, vsf[1] = second variable, etc.
 *       The 'id' field tells you WHICH semantic variable is at that position.
 */
typedef struct {
    uint16_t id;                 ///< Semantic variable ID (RPM_ID, TEMP_ID, etc.)
    uint8_t  type;               ///< Data type ID (R1UP_FILE_UQ16_0, etc.)
    uint16_t sz;                 ///< Size in bytes (calculated from type)
    uint16_t frequency_divider;  ///< Sampling rate divisor (1 = every sample)
} r1up_file_vsf_t;
```

### 5.2 File Handle Extensions

```c
/**
 * @brief Variable registry for ID→position lookup
 */
typedef struct {
    int32_t* id_to_position;     ///< Maps variable ID → position in VSF array (-1 if absent)
    uint16_t max_id;             ///< Highest variable ID in file
    uint16_t var_count;          ///< Total number of variables in file
} r1up_var_registry_t;

typedef struct {
    // ... existing fields ...
    
    r1up_file_vsf_t*    vsf;                 ///< VSF array (indexed by POSITION, not ID!)
    uint32_t            varible_number;      ///< Total number of variables
    void**              vsd_read_buffers;    ///< Data buffers (indexed by position)
    
    // New fields for self-describing format
    r1up_var_registry_t registry;            ///< Lookup table: ID → position
} r1up_file_t;
```

**Critical Concept: VSF Indexing**

⚠️ **VSF array is indexed by PHYSICAL POSITION in file, NOT by variable ID!**

**Example with 3 variables in file:**
```
// File physical layout: [TEMP] [SPEED] [RPM]

// VSF array (indexed by position 0, 1, 2):
vsf[0].id = TEMP_ID     // Position 0 contains TEMP (ID might be 13)
vsf[1].id = SPEED_ID    // Position 1 contains SPEED (ID might be 20)
vsf[2].id = RPM_ID      // Position 2 contains RPM (ID might be 5)

// Lookup table (indexed by variable ID):
id_to_position[RPM_ID=5]    = 2    // RPM found at position 2
id_to_position[TEMP_ID=13]  = 0    // TEMP found at position 0
id_to_position[SPEED_ID=20] = 1    // SPEED found at position 1
id_to_position[other_ids]   = -1   // Not present in file

// Data access:
// User wants RPM value:
//   1. Lookup: position = id_to_position[RPM_ID] = 2
//   2. Access: value = vsd_read_buffers[position][sample_idx]
```

---

## 6. Implementation Prototype

### 6.1 Writing VSF Block

```c
/**
 * @brief Write Variable Stream Format block
 * @details Writes VSF array in order. Position is implicit (array index).
 */
static r1up_file_result_t r1up_file_write_vsf_block(r1up_file_t* file) {
    // Calculate payload size (5 bytes per variable: id + type + freq_div)
    uint16_t num_vars = file->varible_number;
    size_t payload_size = sizeof(uint16_t) + (num_vars * 5);
    
    // Write block header
    r1up_file_write_block_id("R1UP_VSF______");
    r1up_file_write_uint32(payload_size);
    
    // Write number of variables
    r1up_file_write_uint16(num_vars);
    
    // Write each variable descriptor in file order
    // Note: vsf[pos] is already indexed by position (0, 1, 2, ...)
    for (uint16_t pos = 0; pos < num_vars; pos++) {
        r1up_file_write_uint16(file->vsf[pos].id);                // Which variable
        r1up_file_write_uint8(file->vsf[pos].type);               // Data type
        r1up_file_write_uint16(file->vsf[pos].frequency_divider); // Sampling rate
        // Position is implicit: array index 'pos'
    }
    
    return R1UP_FILE_SUCCESS;
}
```

### 6.2 Reading VSF Block

```c
/**
 * @brief Read Variable Stream Format block and build lookup table
 * @details Reads VSF array (indexed by position) and builds registry for ID→position lookup
 */
static r1up_file_result_t r1up_file_read_vsf_block(r1up_file_t* file) {
    // Read number of variables
    uint16_t num_vars;
    r1up_file_read_uint16(&num_vars);
    
    file->varible_number = num_vars;
    
    // Allocate VSF array (indexed by position)
    file->vsf = malloc(num_vars * sizeof(r1up_file_vsf_t));
    if (file->vsf == NULL) {
        return R1UP_FILE_ERROR_MALLOC;
    }
    
    // Read variable descriptors into VSF array
    uint16_t max_id = 0;
    for (uint16_t pos = 0; pos < num_vars; pos++) {
        r1up_file_read_uint16(&file->vsf[pos].id);
        r1up_file_read_uint8(&file->vsf[pos].type);
        r1up_file_read_uint16(&file->vsf[pos].frequency_divider);
        
        // Calculate size from type
        file->vsf[pos].sz = r1up_file_get_type_size(file->vsf[pos].type);
        
        // Track max ID for registry allocation
        if (file->vsf[pos].id > max_id) {
            max_id = file->vsf[pos].id;
        }
    }
    
    // Build registry: ID → position lookup table
    file->registry.max_id = max_id;
    file->registry.var_count = num_vars;
    file->registry.id_to_position = malloc((max_id + 1) * sizeof(int32_t));
    if (file->registry.id_to_position == NULL) {
        return R1UP_FILE_ERROR_MALLOC;
    }
    
    // Initialize all positions to -1 (not present)
    for (uint16_t id = 0; id <= max_id; id++) {
        file->registry.id_to_position[id] = -1;
    }
    
    // Populate mapping: for each position, map its ID to that position
    for (uint16_t pos = 0; pos < num_vars; pos++) {
        file->registry.id_to_position[file->vsf[pos].id] = pos;
    }
    
    return R1UP_FILE_SUCCESS;
}
```

### 6.3 Reading Body with Flexible Ordering

```c
/**
 * @brief Read one sample from body
 * @details Reads variables in their physical file order (position 0, 1, 2, ...)
 */
static r1up_file_result_t r1up_file_read_sample(r1up_file_t* file, 
                                                 uint64_t sample_idx) {
    // Iterate through positions (physical order in file)
    for (uint16_t pos = 0; pos < file->varible_number; pos++) {
        r1up_file_vsf_t* vsf_entry = &file->vsf[pos];
        
        // Check if this variable should be sampled at current sample index
        if (sample_idx % vsf_entry->frequency_divider == 0) {
            // Read value from file
            uint8_t buffer[8];  // Max size for any type
            size_t bytes_read;
            fsal_read(buffer, 1, vsf_entry->sz, file->fsal_handle_ptr, &bytes_read);
            
            // Store in buffer indexed by position
            memcpy(&file->vsd_read_buffers[pos][sample_idx * vsf_entry->sz],
                   buffer, vsf_entry->sz);
        }
        // else: sample-and-hold (use previous value)
    }
    
    return R1UP_FILE_SUCCESS;
}

/**
 * @brief Get sample value by variable ID (user-facing API)
 * @details Uses registry to map ID→position, then accesses data buffer
 */
static r1up_file_result_t r1up_file_get_sample_by_id(r1up_file_t* file,
                                                      uint16_t var_id,
                                                      uint64_t sample_idx,
                                                      void* out_value) {
    // Lookup position from ID
    if (var_id > file->registry.max_id) {
        return R1UP_FILE_ERROR_INVALID_ID;
    }
    
    int32_t pos = file->registry.id_to_position[var_id];
    if (pos < 0) {
        return R1UP_FILE_ERROR_VARIABLE_NOT_PRESENT;  // ID not in file
    }
    
    // Access data at that position
    r1up_file_vsf_t* vsf_entry = &file->vsf[pos];
    memcpy(out_value, 
           &file->vsd_read_buffers[pos][sample_idx * vsf_entry->sz],
           vsf_entry->sz);
    
    return R1UP_FILE_SUCCESS;
}
```

### 6.4 Helper Function: Type Size Lookup

```c
/**
 * @brief Get byte size for a given type ID
 */
static uint16_t r1up_file_get_type_size(uint8_t type_id) {
    switch (type_id) {
        case R1UP_FILE_UQ8_0:
        case R1UP_FILE_Q7_0:
            return 1;
        
        case R1UP_FILE_UQ16_0:
        case R1UP_FILE_Q15_0:
        case R1UP_FILE_UQ12_4:
        case R1UP_FILE_UQ10_6:
        case R1UP_FILE_UQ6_10:
        case R1UP_FILE_UQ4_12:
        case R1UP_FILE_Q4_11:
        case R1UP_FILE_Q3_12:
            return 2;
        
        case R1UP_FILE_UQ32_0:
        case R1UP_FILE_Q31_0:
        case R1UP_FILE_Q3_28:
            return 4;
        
        default:
            return 0;  // Invalid type
    }
}
```

---

## 7. Migration Strategy

### 7.1 Version Flag

Add version identifier to distinguish old vs. new format:

```c
// In R1UP_FILE_VER block
#define R1UP_FILE_FORMAT_VERSION_1  0x0100  // Fixed order (legacy)
#define R1UP_FILE_FORMAT_VERSION_2  0x0200  // Self-describing (new)
```

### 7.2 Compatibility Mode

```c
if (file->format_version == R1UP_FILE_FORMAT_VERSION_1) {
    // Legacy reader: assume fixed VSF order from r1up_file_cfg.h
    r1up_file_read_vsf_legacy(file);
} else if (file->format_version == R1UP_FILE_FORMAT_VERSION_2) {
    // New reader: parse VSF lookup table from file
    r1up_file_read_vsf_block(file);
} else {
    return R1UP_FILE_ERROR_UNSUPPORTED_VERSION;
}
```

### 7.3 Writer Configuration

Allow user to choose format version:

```c
r1up_file_handle_t handle = r1up_file_open("output.r1up", 
                                           R1UP_FILE_MODE_WRITE_ONLY);

// Enable self-describing format
r1up_file_set_format_version(handle, R1UP_FILE_FORMAT_VERSION_2);

// Optionally specify variable order
uint16_t custom_order[] = {SPEED_ID, RPM_ID, TEMP_ID};
r1up_file_set_variable_order(handle, custom_order, 3);
```

---

## 8. Examples

### 8.1 Example File A: Standard Order (RPM, TEMP, SPEED)

**VSF Array (indexed by position):**
```
| Position | Variable ID | Type ID          | Freq Div |
| -------- | ----------- | ---------------- | -------- |
| 0        | RPM_ID=5    | R1UP_FILE_UQ16_0 | 1        |
| 1        | TEMP_ID=13  | R1UP_FILE_UQ12_4 | 1        |
| 2        | SPEED_ID=20 | R1UP_FILE_UQ16_0 | 1        |
```

**Lookup Table (built at runtime):**
```
id_to_position[RPM_ID=5]    = 0
id_to_position[TEMP_ID=13]  = 1
id_to_position[SPEED_ID=20] = 2
```

**Body (Sample 0):**
```
[RPM_value][TEMP_value][SPEED_value]
```

### 8.2 Example File B: Custom Order (SPEED, TEMP, RPM)

**VSF Array:**
```
| Position | Variable ID | Type ID          | Freq Div |
| -------- | ----------- | ---------------- | -------- |
| 0        | SPEED_ID=20 | R1UP_FILE_UQ16_0 | 1        |
| 1        | TEMP_ID=13  | R1UP_FILE_UQ12_4 | 10       |
| 2        | RPM_ID=5    | R1UP_FILE_UQ16_0 | 1        |
```

**Lookup Table:**
```
id_to_position[RPM_ID=5]    = 2    ← RPM is at position 2!
id_to_position[TEMP_ID=13]  = 1
id_to_position[SPEED_ID=20] = 0    ← SPEED is at position 0!
```

**Body (Sample 0):**
```
[SPEED_value][TEMP_value][RPM_value]
```

**Body (Sample 1):**
```
[SPEED_value][skip - freq_div=10, use sample 0][RPM_value]
```

### 8.3 Example File C: Subset of Variables (only RPM, TEMP)

**VSF Array:**
```
| Position | Variable ID | Type ID          | Freq Div |
| -------- | ----------- | ---------------- | -------- |
| 0        | RPM_ID=5    | R1UP_FILE_UQ16_0 | 1        |
| 1        | TEMP_ID=13  | R1UP_FILE_UQ12_4 | 1        |
```

**Lookup Table:**
```
id_to_position[RPM_ID=5]    = 0
id_to_position[TEMP_ID=13]  = 1
id_to_position[SPEED_ID=20] = -1   ← SPEED not in file!
```

**Body (Sample 0):**
```
[RPM_value][TEMP_value]
```

**User code:**
```c
// Trying to get SPEED will fail gracefully:
if (r1up_file_get_sample_by_id(file, SPEED_ID, 0, &value) != R1UP_FILE_SUCCESS) {
    // SPEED not present in this file
}
```

---

## 9. Benefits Summary

| Feature                | Old Format            | New Format                         |
| ---------------------- | --------------------- | ---------------------------------- |
| **Variable Order**     | Fixed at compile-time | Flexible, defined per-file         |
| **Add Variable**       | Recompile reader      | Just add to VSF table              |
| **Remove Variable**    | Breaks old files      | Old files still readable           |
| **Optional Variables** | Must write zeros      | Simply omit from VSF               |
| **Retrocompatibility** | N/A                   | Version flag distinguishes formats |
| **File Size**          | Fixed overhead        | Minimal (+6 bytes per variable)    |

---

## 10. Open Questions & Future Work

### 10.1 Questions for Review

1. **Variable ID Allocation**: Should we reserve ID ranges (0-255 = standard, 256+ = custom)?
2. **Type Validation**: Should reader reject unknown Type IDs or ignore them?
3. **Position Gaps**: Allow non-contiguous positions (e.g., 0, 2, 5) or require dense (0, 1, 2)?
4. **Compression**: Consider RLE or delta encoding for frequent values?

### 10.2 Future Enhancements

- **Metadata per variable**: Units, scaling factors, human-readable names
- **Dynamic types**: Support for string, array, or struct variables
- **Checksums**: Per-sample or per-block CRC for data integrity
- **Streaming mode optimization**: Index for fast random access

---

## 11. References

- `r1up_file_cfg.h` - Variable Stream Format table definition
- `r1up_file.c` - Core file I/O implementation  
- `ag_fixedpoint.h` - Fixed-point arithmetic library
- `RER_SPARSE_VARIABLES.md` - Legacy format sparse variable problem
- `DATA_TYPE.md` - Data type encoding documentation

---

**End of Document**
