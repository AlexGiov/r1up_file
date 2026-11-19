# R1UP File SDK

Static C library to read R1UP / ReRace session files from your own analysis programs.

## Package content

```text
r1up-sdk-<version>/        the SDK
  include/                 public headers (add this folder to the include path)
    r1up_file/r1up_file.h  main API
    r1up_file/r1up_file_variable_type_database.h
    r1up_file_cfg.h, ag_fsal_cfg.h
    ag_fixedpoint/         fixed-point types used by the API
  lib/
    r1up_file.lib          static library (GNU ar archive, MinGW)
    libr1up_file.a         same library, GNU name
main.c                     example program
build.bat                  builds and runs the example
RR_0001.rer                sample session file
R1UP_0001.r1up             same session in the R1UP extension
```

## Quick start

Requirements: Windows and a MinGW `gcc` in the `PATH`.

```bat
build.bat
```

It compiles `main.c` against the SDK into `r1up_test.exe` and runs it, first without arguments
(default file `RR_0001.rer`) and then with `RR_0001.rer` explicitly. To read another file:

```bat
r1up_test.exe <path_to_file>
```

Expected output:

```text
Build OK: r1up_test.exe
R1UP SDK consumer sample - version: <version>
Opening file: RR_0001.rer
Total samples: 3810
File closed successfully.
```

## Use the library in your project

```c
#include "r1up_file/r1up_file.h"
```

```bat
gcc -std=gnu99 -I"r1up-sdk-<version>\include" main.c "r1up-sdk-<version>\lib\r1up_file.lib" -o my_tool.exe
```

Use `libr1up_file.a` instead of `r1up_file.lib` if your build system expects the GNU name.
The headers must be used together with the library of the same package.

## The example (`main.c`)

1. **Open**: `r1up_file_open(filename, R1UP_FILE_MODE_READ_ONLY)` returns a handle (`NULL` on error).
2. **Load**: `r1up_file_read_body(handle)` reads the samples; `r1up_file_get_body_sample_count(handle)` returns their number.
3. **Whole session** (not split by laps): the caller allocates a buffer of `sample_count` values
   (`ag_fixedpoint_float_t`) and fills it with:
   - `r1up_file_get_gps_latitude_dd` / `r1up_file_get_gps_longitude_dd` for the GPS position;
   - `r1up_file_get_var_float(handle, R1UP_FILE_VAR_ID_<NAME>, buffer, sample_count)` for any other variable.
4. **Split by laps**: `r1up_file_get_lap_count` gives the number of laps. Segments are numbered
   `0` (out lap), `1..lap_count` (laps) and `lap_count + 1` (in lap).
   For each segment `r1up_file_get_lap_sample_range` gives the first and last sample, and the `*_by_lap` functions
   (`r1up_file_get_var_float_by_lap`, `r1up_file_get_gps_latitude_dd_by_lap`, ...) read only that segment.
5. **Close**: `r1up_file_close(handle)`.

All functions return `R1UP_FILE_SUCCESS` (0) or an error code. The example prints
`ERROR R1UP_FILE_VAR_ID_<NAME>: 2` when a variable is not stored in the file
(`R1UP_FILE_ERROR_NOT_FOUND`): the file simply does not contain it.

`main.c` reads every known variable one by one, so it can be used as a copy-and-paste reference:
the variable ids are the `R1UP_FILE_VAR_ID_*` constants listed in `r1up_file_variable_type_database.h`.
