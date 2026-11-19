@echo off
setlocal

rem Build static R1UP library with MinGW (desktop backend config)

if "%DEVBIN%"=="" (
    set DEVBIN=C:\devbin
)

set GCC=%DEVBIN%\mingw\mingw64\8.1.0\bin\gcc.exe
set AR=%DEVBIN%\mingw\mingw64\8.1.0\bin\ar.exe

if not exist "%GCC%" (
    echo GCC not found: %GCC%
    exit /b 1
)

if not exist "%AR%" (
    echo AR not found: %AR%
    exit /b 2
)

set ROOT=%~dp0..\..
for %%I in ("%ROOT%") do set ROOT=%%~fI

set BUILD_DIR=%ROOT%\build\r1up_sdk
set OBJ_DIR=%BUILD_DIR%\obj
set LIB_DIR=%BUILD_DIR%\lib

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
mkdir "%OBJ_DIR%"
mkdir "%LIB_DIR%"

set CFLAGS=-std=gnu99 -Wall -Wextra -O2 -Dgeneric -D_WIN32 -DUSE_LTYPES_PROJECT_WORKAROUNDS
set INCLUDES=-I"%ROOT%\src\mut\include" -I"%ROOT%\src\cfg" -I"%ROOT%\src\lib\ag_fsal\include" -I"%ROOT%\src\lib\ag_mal\include" -I"%ROOT%\src\lib\ag_fixedpoint\include"

echo Compiling r1up_file.c
"%GCC%" %CFLAGS% %INCLUDES% -c "%ROOT%\src\mut\src\r1up_file.c" -o "%OBJ_DIR%\r1up_file.o" || exit /b 10

echo Compiling ag_fsal.c
"%GCC%" %CFLAGS% %INCLUDES% -c "%ROOT%\src\lib\ag_fsal\src\ag_fsal.c" -o "%OBJ_DIR%\ag_fsal.o" || exit /b 11

echo Compiling ag_mal.c
"%GCC%" %CFLAGS% %INCLUDES% -c "%ROOT%\src\lib\ag_mal\src\ag_mal.c" -o "%OBJ_DIR%\ag_mal.o" || exit /b 12

echo Compiling ag_fixedpoint.c
"%GCC%" %CFLAGS% %INCLUDES% -c "%ROOT%\src\lib\ag_fixedpoint\src\ag_fixedpoint.c" -o "%OBJ_DIR%\ag_fixedpoint.o" || exit /b 13

echo Creating static library
"%AR%" rcs "%LIB_DIR%\r1up_file.lib" "%OBJ_DIR%\r1up_file.o" "%OBJ_DIR%\ag_fsal.o" "%OBJ_DIR%\ag_mal.o" "%OBJ_DIR%\ag_fixedpoint.o" || exit /b 20
copy /Y "%LIB_DIR%\r1up_file.lib" "%LIB_DIR%\libr1up_file.a" >NUL

echo Build completed:
echo   %LIB_DIR%\r1up_file.lib
echo   %LIB_DIR%\libr1up_file.a

exit /b 0
