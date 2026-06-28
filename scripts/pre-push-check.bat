@echo off
REM Pre-push build verification - builds both Debug and Release configurations.
REM Exit code 0 = success, non-zero = build failure (push should be blocked).

setlocal enabledelayedexpansion

set "SLN=%~dp0..\Particluar.sln"

echo === Pre-push build verification ===
echo.

echo Building Debug|x64...
msbuild "%SLN%" /p:Configuration=Debug /p:Platform=x64 /v:quiet /m
if !errorlevel! neq 0 (
    echo.
    echo ERROR: Debug build failed. Push blocked.
    exit /b 1
)
echo Debug OK.
echo.

echo Building Release|x64...
msbuild "%SLN%" /p:Configuration=Release /p:Platform=x64 /v:quiet /m
if !errorlevel! neq 0 (
    echo.
    echo ERROR: Release build failed. Push blocked.
    exit /b 1
)
echo Release OK.
echo.

echo === All builds passed. Push allowed. ===
exit /b 0
