@echo off

REM Path to your cmake executable
set "CMake=C:\Program Files\CMake\bin\cmake.exe"

cd /d %~dp0\build

CMake --build . --config Debug --target INSTALL

pause
