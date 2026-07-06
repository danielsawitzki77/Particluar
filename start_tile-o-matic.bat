@echo off
setlocal
:: Tile-O-Matic — Tileset Configurator + Level Editor
:: Opens a browser window with the web-based editor on launch.

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
echo Starting Tile-O-Matic...
echo Use the Close button in the browser or Ctrl+C here to stop.
echo.
npx ts-node src/server.ts
endlocal
