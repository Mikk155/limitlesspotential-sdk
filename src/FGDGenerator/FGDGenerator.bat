@echo off

if exist bin rmdir /s /q bin
if exist obj rmdir /s /q obj

setlocal
dotnet add package Newtonsoft.Json
dotnet run
endlocal
