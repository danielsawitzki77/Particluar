@echo off
cd /d "%~dp0"

echo ============================================
echo   Particluar Body Viewer
echo ============================================
echo.
echo Usage: start_body_viewer.bat [bodies_directory]
echo        start_body_viewer.bat --collision-test
echo.
echo   Optional arguments:
echo     bodies_directory   - Path to folder containing .json body files
echo                          Default: assets/bodies/
echo     --collision-test   - Run batch collision tests (no window)
echo.
echo Controls:
echo   Left/Right arrows  - Cycle through models
echo   W/A/S/D            - Rotate current model
echo   C                  - Toggle collision shape wireframe overlay
echo   Close window       - Quit
echo.
echo ============================================
echo.

if "%~1"=="--collision-test" (
    echo Running collision tests...
    bin\Debug\BodyViewer.exe --collision-test
    goto :done
)

if "%~1"=="" (
    echo Starting with default directory: assets/bodies/
    bin\Debug\BodyViewer.exe
) else (
    echo Starting with directory: %~1
    bin\Debug\BodyViewer.exe "%~1"
)

:done
if errorlevel 1 (
    echo.
    echo BodyViewer exited with an error.
    pause
)
