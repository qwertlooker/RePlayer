@echo off
chcp 65001 >nul 2>&1
echo ============================
echo   RePlayer Local Server
echo ============================
echo.

where python3 >nul 2>&1
if %errorlevel%==0 (
    echo Starting server with python3...
    echo Open http://localhost:8080 in your browser
    echo Press Ctrl+C to stop
    echo.
    python3 -m http.server 8080
    goto :end
)

where python >nul 2>&1
if %errorlevel%==0 (
    echo Starting server with python...
    echo Open http://localhost:8080 in your browser
    echo Press Ctrl+C to stop
    echo.
    python -m http.server 8080
    goto :end
)

where py >nul 2>&1
if %errorlevel%==0 (
    echo Starting server with py...
    echo Open http://localhost:8080 in your browser
    echo Press Ctrl+C to stop
    echo.
    py -m http.server 8080
    goto :end
)

echo ERROR: Python not found. Please install Python 3 and try again.

:end
pause
