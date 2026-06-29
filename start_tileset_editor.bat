@echo off
setlocal
:: Particluar Tileset Editor - auto-install and launch
:: Opens the browser-based tileset configurator and level editor.

cd /d "%~dp0tools\tileset-editor"

:: Check if node_modules exists; install if missing
if not exist "node_modules\" (
    echo Installing dependencies [first run only]...
    npm install
    if errorlevel 1 (
        echo ERROR: npm install failed. Make sure Node.js is installed.
        pause
        exit /b 1
    )
    echo Install complete.
)

echo.
echo Starting Tileset Editor...
echo Use the Close button in the browser or Ctrl+C here to stop.
echo.
npx ts-node src/server.ts
endlocal
