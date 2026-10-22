@echo off
echo ============================================
echo   Building Face Attendance System
echo ============================================
echo.

:: Try MinGW first
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Compiling with GCC (MinGW)...
    gcc server.c -o server.exe -lws2_32 -lm -O2
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Build successful: server.exe
        echo.
        echo Run with:  server.exe
        echo Open:      http://localhost:8080
    ) else (
        echo [ERR] Build failed!
    )
    goto :end
)

:: Try MSVC
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Compiling with MSVC...
    cl /O2 /Fe:server.exe server.c ws2_32.lib
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Build successful: server.exe
        echo.
        echo Run with:  server.exe
        echo Open:      http://localhost:8080
    ) else (
        echo [ERR] Build failed!
    )
    goto :end
)

echo [ERR] No C compiler found! Install MinGW or MSVC.
echo       Download MinGW: https://winlibs.com

:end
echo.
pause
