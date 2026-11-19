::*********************************************************************
::
:: batch file to execute coverity on component level
::
::*********************************************************************

@echo off

%DEVBIN%\python\v3.9.13\python.exe %~dp0build\UnitTestIt\UnitTestIt.py --config %~dp0test.xml %1 %2 %3 %4 %5 %6 %7 %8 %9
