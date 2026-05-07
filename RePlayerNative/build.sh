#!/bin/bash
echo "============================"
echo "  RePlayer Build Script"
echo "============================"
echo ""

if ! command -v dotnet &>/dev/null; then
    echo "ERROR: .NET 8 SDK not found. Please install from https://dotnet.microsoft.com/download/dotnet/8.0"
    exit 1
fi

echo "Building RePlayer for Windows..."
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:PublishTrimmed=false -o publish

if [ $? -ne 0 ]; then
    echo ""
    echo "Build FAILED!"
    exit 1
fi

echo ""
echo "============================"
echo "  Build successful!"
echo "  Output: publish/RePlayer.exe"
echo "============================"
