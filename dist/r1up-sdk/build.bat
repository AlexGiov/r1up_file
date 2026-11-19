@echo off
setlocal

set OUT=r1up_test.exe

gcc.exe -std=gnu99 -Wall -I".\r1up-sdk-1.0.0\include" main.c ".\r1up-sdk-1.0.0\lib\r1up_file.lib" -o "%OUT%"
if errorlevel 1 (
    echo Build failed.
    exit /b 3
)

echo Build OK: %OUT%
"%OUT%"
"%OUT%" RR_0001.rer
exit /b %ERRORLEVEL%
