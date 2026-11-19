# R1UP MATLAB Source Bundle

This folder contains a helper script to copy the same source files used by
`packaging/r1up_sdk/build_static_mingw.bat` into a MATLAB-oriented working
directory with an SDK-like layout:

- `include/` (headers in the same namespaced layout used by the sources: `r1up_file/`, `ag_fsal/`, `ag_mal/`, `ag_fixedpoint/`;
  the `*_cfg.h` files from `src/cfg`)
- `src/` (the `.c` files compiled for the static library)

## Script

- `copy_sources_for_matlab.ps1`

## Default destination

`dist/r1up_file_matlab_src` (under `src/test/unit`)

## Usage

Run from `src/test/unit`:

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\r1up_file_matlab_bundle\copy_sources_for_matlab.ps1
```

Clean destination before copy (its contents are deleted, the folder itself is kept):

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\r1up_file_matlab_bundle\copy_sources_for_matlab.ps1 -CleanDestination
```

Override destination:

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\r1up_file_matlab_bundle\copy_sources_for_matlab.ps1 -DestinationRoot "<target_folder>" -CleanDestination
```

## Notes

- The script creates `include/` + `src/` structure explicitly for MATLAB build usage.
- The include layout is designed so you can compile with `-Iinclude` without editing source `#include` lines.
- The copy of `include/r1up_file_cfg.h` is patched with `R1UP_FILE_BUILD_ON_MATLAB 1`; the original in `src/cfg` is untouched.
  With this flag `r1up_file.c` includes `mex.h` and exposes `mexFunction`, so it compiles only with the MATLAB `mex` toolchain.
- Use `-DestinationRoot` to copy somewhere else (e.g. a network share).
- `BUNDLE_MANIFEST.txt` and `MATLAB_BUILD_HINT.txt` are generated in destination.
- Verified: with the flag set to 0 all four sources compile with plain `gcc -std=gnu99 -Iinclude`. The `mex` build itself
  has not been run (MATLAB is not available on the dev machine).
