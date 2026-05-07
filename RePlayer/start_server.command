#!/bin/bash
echo "============================"
echo "  RePlayer Local Server"
echo "============================"
echo ""

if command -v python3 &>/dev/null; then
    echo "Starting server with python3..."
    echo "Open http://localhost:8080 in your browser"
    echo "Press Ctrl+C to stop"
    echo ""
    python3 -m http.server 8080
elif command -v python &>/dev/null; then
    echo "Starting server with python..."
    echo "Open http://localhost:8080 in your browser"
    echo "Press Ctrl+C to stop"
    echo ""
    python -m http.server 8080
else
    echo "ERROR: Python not found. Please install Python 3 and try again."
fi
