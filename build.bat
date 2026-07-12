@echo off
REM build.bat — build script for breakdance on Windows
REM
REM Usage:
REM   build.bat [OPTIONS]
REM
REM Options:
REM   --deps              Install vcpkg packages then exit
REM   --build             Configure and build (default when no action flag given)
REM   --jobs N            Parallel compile jobs (default: all logical cores)
REM   --video FILE        Embed FILE as assets\video.mp4 before building
REM   --vcpkg DIR         Path to vcpkg root (or set VCPKG_ROOT env var)
REM   --help              Show this help
REM
REM Examples:
REM   build.bat --deps --vcpkg C:\vcpkg
REM   build.bat --build --vcpkg C:\vcpkg
REM   build.bat --build --jobs 4 --vcpkg C:\vcpkg
REM   build.bat --deps --build --video C:\clip.mp4 --vcpkg C:\vcpkg

setlocal ENABLEDELAYEDEXPANSION

REM ── Defaults ─────────────────────────────────────────────────────────────
set DO_DEPS=0
set DO_BUILD=0
set JOBS=0
set VIDEO_PATH=
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

REM ── Argument parsing ──────────────────────────────────────────────────────
set NO_ARGS=1
:parse_loop
if "%~1"=="" goto parse_done
set NO_ARGS=0
if /I "%~1"=="--deps"  ( set DO_DEPS=1  & shift & goto parse_loop )
if /I "%~1"=="--build" ( set DO_BUILD=1 & shift & goto parse_loop )
if /I "%~1"=="--help"  ( goto show_help )
if /I "%~1"=="-h"      ( goto show_help )
if /I "%~1"=="--jobs" (
    set JOBS=%~2
    if "!JOBS!"=="" ( echo ERROR: --jobs requires a number. & exit /b 1 )
    shift & shift & goto parse_loop
)
if /I "%~1"=="--video" (
    set VIDEO_PATH=%~2
    if "!VIDEO_PATH!"=="" ( echo ERROR: --video requires a file path. & exit /b 1 )
    shift & shift & goto parse_loop
)
if /I "%~1"=="--vcpkg" (
    set VCPKG_ROOT=%~2
    if "!VCPKG_ROOT!"=="" ( echo ERROR: --vcpkg requires a directory path. & exit /b 1 )
    shift & shift & goto parse_loop
)
echo ERROR: Unknown argument: %~1
exit /b 1

:parse_done
REM Bare invocation → just build
if %NO_ARGS%==1 set DO_BUILD=1

REM ── Resolve job count ─────────────────────────────────────────────────────
if %JOBS%==0 (
    REM Use NUMBER_OF_PROCESSORS (set by Windows for all logical cores)
    set JOBS=%NUMBER_OF_PROCESSORS%
)
echo [build] Jobs: %JOBS% thread(s)

REM ── Validate vcpkg ───────────────────────────────────────────────────────
if "%VCPKG_ROOT%"=="" (
    echo ERROR: vcpkg root not set. Use --vcpkg DIR or set VCPKG_ROOT.
    exit /b 1
)
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg.exe not found at %VCPKG_ROOT%
    exit /b 1
)

REM ── Install dependencies ──────────────────────────────────────────────────
if %DO_DEPS%==1 (
    echo [build] Installing vcpkg packages ^(x64-windows-static^) with %JOBS% thread^(s^)...
    set VCPKG_MAX_CONCURRENCY=%JOBS%
    "%VCPKG_ROOT%\vcpkg.exe" install sdl2:x64-windows-static ffmpeg:x64-windows-static
    if errorlevel 1 ( echo ERROR: vcpkg install failed. & exit /b 1 )
    echo [build] Dependencies installed.
)

REM ── Build ─────────────────────────────────────────────────────────────────
if %DO_BUILD%==1 (
    REM Copy video if supplied
    if not "!VIDEO_PATH!"=="" (
        echo [build] Copying video: !VIDEO_PATH! -^> assets\video.mp4
        if not exist "%SCRIPT_DIR%\assets" mkdir "%SCRIPT_DIR%\assets"
        cmake -E copy_if_different "!VIDEO_PATH!" "%SCRIPT_DIR%\assets\video.mp4"
        if errorlevel 1 ( echo ERROR: Failed to copy video. & exit /b 1 )
    )

    if not exist "%SCRIPT_DIR%\assets\video.mp4" (
        echo WARNING: assets\video.mp4 not found.
        echo          The app will show an error at runtime until you embed a video.
        echo          Re-run: build.bat --build --video C:\path\to\your_video.mp4
    )

    REM Configure
    set BUILD_DIR=%SCRIPT_DIR%\build
    if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

    echo [build] Configuring...
    cmake -S "%SCRIPT_DIR%" -B "!BUILD_DIR!" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DVCPKG_ROOT="%VCPKG_ROOT%" ^
        -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
        -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if errorlevel 1 ( echo ERROR: cmake configure failed. & exit /b 1 )

    REM Compile
    echo [build] Compiling with %JOBS% thread^(s^)...
    cmake --build "!BUILD_DIR!" --config Release --parallel %JOBS%
    if errorlevel 1 ( echo ERROR: build failed. & exit /b 1 )

    echo.
    echo [build] Done!  Binary: !BUILD_DIR!\Release\breakdance.exe
)

endlocal
goto :eof

REM ── Help ──────────────────────────────────────────────────────────────────
:show_help
echo build.bat [OPTIONS]
echo.
echo Options:
echo   --deps              Install vcpkg packages then exit
echo   --build             Configure and build
echo   --jobs N            Parallel compile jobs (default: all logical cores)
echo   --video FILE        Embed FILE as assets\video.mp4 before building
echo   --vcpkg DIR         Path to vcpkg root (or set VCPKG_ROOT env var)
echo   --help              Show this help
echo.
echo Examples:
echo   build.bat --deps --vcpkg C:\vcpkg
echo   build.bat --build --vcpkg C:\vcpkg
echo   build.bat --build --jobs 4 --vcpkg C:\vcpkg
echo   build.bat --deps --build --video C:\clip.mp4 --vcpkg C:\vcpkg
endlocal
