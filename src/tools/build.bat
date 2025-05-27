@echo off

REM Build to the assets repository

dotnet build -c Release -o ./../../assets/tools

echo Clearing up...

for /d /r src %%d in (bin,obj) do (

    if exist "%%d" (

        attrib -r -s -h "%%d" /s /d

        rmdir /s /q "%%d"

    )

)

pause
