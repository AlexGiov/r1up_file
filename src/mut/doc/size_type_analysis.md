# R1UP File Library — Size Type Architecture

**Date:** 2026-02-27  
**Author:** Alex Giov + AI brainstorming session  
**Status:** Architecture defined — `r1up_file_size_t` as public API type, migration to v2 deferred

---

## 1. The Problem: `size_t` Is Platform-Dependent

The R1UP file library is a **cross-platform binary format library**. It runs on:

| Platform                 | Role                               | `size_t` width |
| ------------------------ | ---------------------------------- | -------------- |
| PIC32 (Microchip MIPS32) | Embedded writer (current)          | 32 bit         |
| STM32 (ARM Cortex-M)     | Embedded writer (future)           | 32 bit         |
| Linux x86_64             | Server-side reader (web platform)  | 64 bit         |
| Windows x64 + MATLAB     | Workstation reader (data analysis) | 64 bit         |

A binary file format **must** guarantee identical byte layout on every platform.
Using `size_t` for file offsets or section lengths would produce 4-byte fields
on embedded and 8-byte fields on workstations — the same file would be
unreadable across architectures.

The standard library solved the analogous problem for memory by inventing
`size_t`: a single name whose underlying width adapts to the platform.
We need the same pattern, but for the **file domain**.

---

## 2. The Solution: `r1up_file_size_t`

```c
/**
 * @brief Domain-specific size type for the R1UP file format.
 *
 * Analogous to size_t for memory, r1up_file_size_t is THE canonical type
 * for every quantity that refers to a position, offset, count, or length
 * within an R1UP file.
 *
 * All public API functions that accept or return file-related sizes
 * use this type exclusively. This means:
 *
 *  - Changing uint32_t → uint64_t here upgrades the ENTIRE library
 *    (internal structs, public API, serialization) in ONE line.
 *  - User code written against r1up_file_size_t recompiles without edits.
 *  - The API contract never changes; only the capacity grows.
 *
 * Current: uint32_t  (format v1, max body ~4 GB)
 * Future:  uint64_t  (format v2, endurance sessions > 4 GB)
 */
typedef uint32_t r1up_file_size_t;
```

This typedef **must live in `r1up_file.h`** (the public header), not in
the `.c` file, because it appears in public function signatures.

### 2.1 Why in the public header?

This is the **key architectural insight**. If the typedef is private (in `.c`),
public API functions are forced to use concrete types like `uint32_t` or
`uint64_t`. When the format evolves, every function signature changes, and
every caller must be updated.

By placing `r1up_file_size_t` in `r1up_file.h`:

1. **API stability** — The function signature `r1up_file_get_var_float_by_lap(..., r1up_file_size_t buffer_size)` never changes. Users write their code once.
2. **Single-line migration** — Changing `uint32_t` → `uint64_t` in the typedef propagates everywhere automatically: internal structs, public API parameters, return types.
3. **Self-documenting** — A parameter typed `r1up_file_size_t` communicates "this is a file-domain quantity" better than a raw `uint32_t` that could mean anything.
4. **Compiler enforcement** — On platforms with strict type checking or `-Wconversion`, the dedicated type catches accidental mixing with unrelated integers.

This is exactly the pattern behind `size_t` itself: you never see `malloc(unsigned long n)` — you see `malloc(size_t n)`, and `size_t` adapts.

### 2.2 ABI considerations

Changing the typedef **does break binary compatibility** (the function's
actual parameter width changes). However, since the library is **statically
linked** and **recompiled** on each target platform, ABI stability is
irrelevant. Source-level compatibility is what matters, and `r1up_file_size_t`
guarantees it.

---

## 3. Two Distinct Worlds

### 3.1 Serialized (on-disk) fields

These are part of the binary file layout. Their width **defines** the format
version and must be deterministic across all platforms:

| Field                                    | Current type                    | Role                            |
| ---------------------------------------- | ------------------------------- | ------------------------------- |
| `hdr_ln`, `bdy_ln`, `ftr_ln`             | `r1up_file_size_t` (`uint32_t`) | Section lengths                 |
| `body_offset_start`, `body_offset_end`   | `r1up_file_size_t`              | Byte positions in file          |
| `file_size`                              | `r1up_file_size_t`              | Total file size                 |
| Block `sz`                               | `uint32_t`                      | Payload size                    |
| `var_stream_data_size`                   | `r1up_file_size_t`              | Body variable data section size |
| Lap marker `sample_index`, `file_offset` | `uint64_t` ⚠️                    | Should be `r1up_file_size_t`    |

### 3.2 In-memory / API quantities

These are derived from on-disk fields and exposed to users:

| Field / parameter                                     | Current type  | Should be           |
| ----------------------------------------------------- | ------------- | ------------------- |
| `total_sample_number`                                 | `uint64_t` ⚠️  | `r1up_file_size_t`  |
| `current_sample_number`                               | `uint64_t` ⚠️  | `r1up_file_size_t`  |
| `r1up_file_get_body_sample_count()` return            | `uint64_t` ⚠️  | `r1up_file_size_t`  |
| `r1up_file_get_var_float()` `buffer_size`             | `uint32_t`    | `r1up_file_size_t`  |
| `r1up_file_get_lap_sample_range()` indices            | `uint64_t*` ⚠️ | `r1up_file_size_t*` |
| `r1up_file_get_lap_offset_range()` offsets            | `uint64_t*` ⚠️ | `r1up_file_size_t*` |
| New: `r1up_file_get_var_float_by_lap()` `buffer_size` | —             | `r1up_file_size_t`  |

The ⚠️ items are **incoherent today**: they use `uint64_t` while the format
can only physically express values up to `UINT32_MAX`.

---

## 4. The Coherence Invariant

The number of samples is derived from body offsets:

```
total_sample_number = (body_offset_end - body_offset_start) / sample_row_size
```

Since `sample_row_size ≥ 1`, the result is always ≤ the range of the offset type.

> **INVARIANT:** `typeof(total_sample_number)` MUST equal `typeof(body_offsets)` = `r1up_file_size_t`
>
> Using a **wider** type than the container (body offsets) is incoherent:
> it promises a capacity that the format cannot physically express.
> Using a **narrower** type is a latent truncation bug.

This invariant holds regardless of whether `r1up_file_size_t` is `uint32_t`
or `uint64_t`. It's a design truth, not a value constraint.

When `r1up_file_size_t` becomes `uint64_t`, the invariant is automatically
satisfied everywhere — no manual hunt for mismatched types.

---

## 5. Practical Limits of uint32_t

With the current 32-bit body size (max 4,294,967,295 bytes ≈ 4 GB):

| Scenario                    | Sample rate | Vars × bytes/var | Row size | Max duration    |
| --------------------------- | ----------- | ---------------- | -------- | --------------- |
| Minimal (1 var, 1 byte)     | 10 Hz       | 1 × 1 = 1 B      | 1 B      | ~13.6 years     |
| Typical kart (50 vars, 2 B) | 10 Hz       | 50 × 2 = 100 B   | 100 B    | **~11.9 hours** |
| Rich GT car (100 vars, 4 B) | 10 Hz       | 100 × 4 = 400 B  | 400 B    | **~2.98 hours** |
| High-freq (50 vars, 2 B)    | 100 Hz      | 50 × 2 = 100 B   | 100 B    | ~1.19 hours     |

**Risk:** Endurance races (24h Le Mans, Daytona 24h, Nürburgring 24h) with a
typical variable set will exceed the 4 GB body limit after ~12 hours.
This is a **real constraint** that motivates the future v2 migration.

---

## 6. Migration Strategy

### Phase 0 — Now (format v1, before implementing `get_var_float_by_lap`)

| Action                                        | File                          | Notes                                          |
| --------------------------------------------- | ----------------------------- | ---------------------------------------------- |
| Move `typedef uint32_t r1up_file_size_t;`     | `r1up_file.c` → `r1up_file.h` | Becomes public API type                        |
| Use `r1up_file_size_t` in new API functions   | `r1up_file.h`                 | `buffer_size`, sample counts                   |
| Use `r1up_file_size_t` in lap marker struct   | `r1up_file.c`                 | `sample_index`, `file_offset`                  |
| Align existing internal fields                | `r1up_file.c`                 | `total_sample_number`, `current_sample_number` |
| Update existing public API return/param types | `r1up_file.h`                 | `get_body_sample_count`, `get_lap_*`           |

**No behavioral change.** The underlying type remains `uint32_t`. Only the
symbolic name changes. All existing tests must still pass.

### Phase 1 — Format v2 (future, when endurance support needed)

| Action                                            | Notes                     |
| ------------------------------------------------- | ------------------------- |
| Change `typedef uint32_t` → `typedef uint64_t`    | **One line.**             |
| Update serialization (section lengths to 8 bytes) | Format-level change       |
| Bump format version number in header              | Backward compat indicator |
| Update `r1up_file_compute_sample_number`          | Handle wider arithmetic   |

Because of Phase 0, **no public API signatures change**. User code calling
`r1up_file_get_var_float_by_lap()` recompiles unchanged.

---

## 7. Summary — The Design Principle

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│   r1up_file_size_t  is to the R1UP file format                  │
│                     what                                         │
│   size_t            is to the C memory model.                   │
│                                                                  │
│   It abstracts the width behind a stable name.                  │
│   The API contract is written in terms of the name.             │
│   The width is an implementation detail that can evolve.        │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

**One typedef. One line to change. Zero API breakage.**

---

## 8. Action Items

- [ ] **Phase 0:** Move `r1up_file_size_t` typedef to `r1up_file.h` (public header)
- [ ] **Phase 0:** Use `r1up_file_size_t` in all new API functions (`get_var_float_by_lap`, `get_lap_sample_count`)
- [ ] **Phase 0:** Align lap marker fields to `r1up_file_size_t`
- [ ] **Phase 0:** Align `total_sample_number`, `current_sample_number` to `r1up_file_size_t`
- [ ] **Phase 0:** Update existing public API (`get_body_sample_count`, `get_lap_sample_range`, `get_lap_offset_range`)
- [ ] **Deferred (v2):** Change `r1up_file_size_t` from `uint32_t` to `uint64_t`
- [ ] **Deferred (v2):** Update serialization format for 64-bit section lengths
- [ ] **Deferred (v2):** Bump format version number
- [x] **Done:** Document the coherence invariant and migration architecture (this document)
