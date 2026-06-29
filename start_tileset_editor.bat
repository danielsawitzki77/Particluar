@echo off
:: Particluar Tileset Editor — auto-install and launch
:: Opens the browser-based tileset configurator and level editor.

cd /d "%~dp0tools\tileset-editor"

:: Check if node_modules exists; install if missing
if not exist "node_modules\" (
    echo Installing dependencies (first run only)...
    call npm install
    if errorlevel 1 (
        echo ERROR: npm install failed. Make sure Node.js is installed.
        pause
        exit /b 1
    )
    echo Done.
    echo.
)

echo Starting Tileset Editor...
echo Press Ctrl+C or use the Close button in the browser to stop.
echo.
call npm start
