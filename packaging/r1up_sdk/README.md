# R1UP SDK Packaging (Static Library)

This folder contains a step-by-step workflow to produce an SDK package:
- static libraries: `r1up_file.lib` and `libr1up_file.a`
- public headers: `r1up_file.h` and its required dependencies
- consumer smoke test to validate packaging (run on the real package in `dist/r1up-sdk`, shipped as example)

## Goal

Distribute an SDK that can be used by external analysis programs without exposing source files.

## Design

- static library, with dependencies (`ag_fsal`, `ag_mal`, `ag_fixedpoint`) embedded in the library build
- distribute library + public headers only

## Prerequisites

- Windows with PowerShell
- MinGW-w64 8.1.0 at `%DEVBIN%\mingw\mingw64\8.1.0\bin` (`DEVBIN` defaults to `C:\devbin`)

## Scripts

| Script | Purpose |
|--------|---------|
| `build_static_mingw.bat` | Compiles `src/mut/src/r1up_file.c` and the `ag_*` libraries into `build/r1up_sdk/lib/r1up_file.lib` and `libr1up_file.a` (same GNU `ar` archive, two names). Recreates `build/r1up_sdk` on every run. Exit codes: 1/2 toolchain missing, 10-13 compile error, 20 archive error. |
| `stage_sdk.ps1` | Deletes `dist/r1up-sdk`, then stages public headers and libraries into `dist/r1up-sdk/r1up-sdk-<version>`. Params: `-Version`, `-LibSourcePath`. |
| `example_template/` | Files copied as-is into `dist/r1up-sdk` and shipped with the SDK: `README.md` (package content and library usage), `build.bat`, `main.c`, `RR_0001.rer`, `R1UP_0001.r1up`. It only holds templates: nothing is built here. |
| `release_sdk.ps1` | Runs build, stage, template copy, smoke test and zip. Params: `-Version`, `-SkipSmokeTest`. |

## Generated package (`dist/`)

```text
dist/
  r1up-sdk/                  <- rebuilt from scratch on every release
    r1up-sdk-<version>/      <- the SDK (see "SDK layout")
    build.bat                <- copied from example_template
    README.md
    main.c
    RR_0001.rer
    R1UP_0001.r1up
  r1up-sdk-<version>.zip     <- zip of the whole dist/r1up-sdk
```

## One-command release

From `src/test/unit`, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\r1up_sdk\release_sdk.ps1 -Version "1.0.0"
```

This command executes:
1. static library build
2. SDK staging into `dist/r1up-sdk/r1up-sdk-<version>` (the whole `dist/r1up-sdk` folder is deleted first)
3. copy of `example_template/*` into `dist/r1up-sdk`; in this copy of `build.bat`, `r1up-sdk-1.0.0` is replaced with `r1up-sdk-<version>`
4. smoke test: `build.bat` is run inside `dist/r1up-sdk`, so it validates the real package (skip with `-SkipSmokeTest`);
   `r1up_test.exe` is then deleted
5. zip creation `dist/r1up-sdk-<version>.zip`

Warning: everything in `dist/r1up-sdk` and the `.zip` with the same version are overwritten/deleted.
Use a different `-Version` (e.g. `1.0.0-test`) for trial runs, and keep manual files elsewhere.

Expected smoke test output (the sample runs twice, without and with `RR_0001.rer`):

```text
Build OK: r1up_test.exe
R1UP SDK consumer sample - version: 1.0.0
Opening file: RR_0001.rer
Total samples: 3810
File closed successfully.
```

`build.bat` uses `gcc` from the `PATH`; if it is not found, `release_sdk.ps1` prepends the MinGW of `%DEVBIN%` for the test.
Users of the zip need a MinGW `gcc` in their `PATH` to run `build.bat`.

Note: the sample reads every known variable one by one and prints `ERROR R1UP_FILE_VAR_ID_<name>: 2`
(`R1UP_FILE_ERROR_NOT_FOUND`) for variables not stored in the file. `RR_0006.rer` and `RR_0027.rer`
do not contain them: running `r1up_test.exe <file>` on them prints these errors, with exit code 0.

## Step by step (manual)

Run from `src/test/unit`:

```bat
packaging\r1up_sdk\build_static_mingw.bat
powershell -ExecutionPolicy Bypass -File .\packaging\r1up_sdk\stage_sdk.ps1 -Version "1.0.0" -LibSourcePath ".\build\r1up_sdk\lib\r1up_file.lib"
copy packaging\r1up_sdk\example_template\* dist\r1up-sdk\
cd dist\r1up-sdk
build.bat
```

For a version other than `1.0.0`, edit the folder name in `build.bat`.

## SDK layout

```text
r1up-sdk-<version>/
  include/
    r1up_file/r1up_file.h
    r1up_file/r1up_file_variable_type_database.h
    r1up_file_cfg.h
    ag_fsal_cfg.h
    ag_fixedpoint/ag_fixedpoint.h
    ag_fixedpoint/ag_fixedpoint_cfg.h
  lib/
    r1up_file.lib
    libr1up_file.a
```

Consumers use `#include "r1up_file/r1up_file.h"` and add `<sdk>/include` to the include path.

## Notes

- Keep `r1up_file_cfg.h` exactly aligned with the library build (the build uses `src/cfg`).
- `-Version` only names the output folder/zip and the folder referenced by `build.bat`;
  `R1UP_FILE_VERSION_STRING` is defined in `r1up_file.h` and must be updated there.
- If you change public enums/types/macros, bump SDK version.
- Do not copy test-only headers into the SDK.
