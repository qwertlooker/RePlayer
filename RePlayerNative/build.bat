@echo off
chcp 65001 >nul 2>&1
echo ============================
echo   RePlayer Build Script
echo ============================
echo.

where dotnet >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: .NET 8 SDK not found. Please install from https://dotnet.microsoft.com/download/dotnet/8.0
    pause
    exit /b 1
)

echo Building RePlayer for Windows...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish

if %errorlevel% neq 0 (
    echo.
    echo Build FAILED!
    pause
    exit /b 1
)

echo.
echo ============================
echo   Build successful!
echo   Output: publish\RePlayer.exe
echo ============================
pause
