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

if "%1"=="" (
    echo Building for all platforms...
    echo.
    echo Building RePlayer for Windows x64...
    dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/win-x64

    echo.
    echo Building RePlayer for Windows ARM64...
    dotnet publish -c Release -r win-arm64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/win-arm64

    echo.
    echo Building RePlayer for Linux x64...
    dotnet publish -c Release -r linux-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/linux-x64

    echo.
    echo Building RePlayer for Linux ARM64...
    dotnet publish -c Release -r linux-arm64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/linux-arm64

    echo.
    echo Building RePlayer for macOS x64/ARM64 (Universal)...
    dotnet publish -c Release -r osx-arm64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/osx-arm64
) else (
    echo Building RePlayer for %1...
    dotnet publish -c Release -r %1 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/%1
)

if %errorlevel% neq 0 (
    echo.
    echo Build FAILED!
    pause
    exit /b 1
)

echo.
echo ============================
echo   Build successful!
echo ============================
pause
