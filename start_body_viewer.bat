@echo off
cd /d "%~dp0"

echo ============================================
echo   Particluar Body Viewer
echo ============================================
echo.
echo Usage: start_body_viewer.bat [bodies_directory]
echo.
echo   Optional argument:
echo     bodies_directory  - Path to folder containing .json body files
echo                         Default: assets/bodies/
echo.
echo Controls:
echo   Left/Right arrows  - Cycle through models
echo   W/A/S/D            - Rotate current model
echo   Close window       - Quit
echo.
echo ============================================
echo.

if "%~1"=="" (
    echo Starting with default directory: assets/bodies/
    bin\Debug\Body_Viewer.exe
) else (
    echo Starting with directory: %~1
    bin\Debug\Body_Viewer.exe "%~1"
)

if errorlevel 1 (
    echo.
    echo Body_Viewer exited with an error.
    pause
)