#!/bin/bash
echo "============================"
echo "  RePlayer Build Script"
echo "============================"
echo ""

if ! command -v dotnet &>/dev/null; then
    echo "ERROR: .NET 8 SDK not found. Please install from https://dotnet.microsoft.com/download/dotnet/8.0"
    exit 1
fi

if [ -z "$1" ]; then
    echo "Building for all platforms..."
    echo ""
    echo "Building RePlayer for Windows x64..."
    dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/win-x64

    echo ""
    echo "Building RePlayer for Windows ARM64..."
    dotnet publish -c Release -r win-arm64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/win-arm64

    echo ""
    echo "Building RePlayer for Linux x64..."
    dotnet publish -c Release -r linux-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/linux-x64

    echo ""
    echo "Building RePlayer for Linux ARM64..."
    dotnet publish -c Release -r linux-arm64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/linux-arm64

    echo ""
    echo "Building RePlayer for macOS ARM64..."
    dotnet publish -c Release -r osx-arm64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/osx-arm64

    echo ""
    echo "Building RePlayer for macOS x64..."
    dotnet publish -c Release -r osx-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/osx-x64
else
    echo "Building RePlayer for $1..."
    dotnet publish -c Release -r "$1" --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish/"$1"
fi

if [ $? -ne 0 ]; then
    echo ""
    echo "Build FAILED!"
    exit 1
fi

echo ""
echo "============================"
echo "  Build successful!"
echo "============================"
