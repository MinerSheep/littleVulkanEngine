@echo off
setlocal

REM play.bat — cross-build Windows version and run it
REM
REM Usage:
REM   play.bat         build engine+game, then run
REM   play.bat -n      skip build, just run
REM   play.bat -c      clean rebuild first
REM   play.bat -h      show help

set BUILD=1
set CLEAN=0

:parse
if "%~1"=="" goto parsed
if /I "%~1"=="-n" (
    set BUILD=0
) else if /I "%~1"=="--no-build" (
    set BUILD=0
) else if /I "%~1"=="-c" (
    set CLEAN=1
) else if /I "%~1"=="--clean" (
    set CLEAN=1
) else if /I "%~1"=="-h" (
    goto help
) else if /I "%~1"=="--help" (
    goto help
) else (
    echo play.bat: unknown option '%~1'
    exit /b 2
)

shift
goto parse

:parsed

set REPO=%~dp0
pushd "%REPO%"

if "%CLEAN%"=="1" (
    echo ^>^> clean...
    make -C engine clean
    make -C game clean
)

if "%BUILD%"=="1" (
    echo ^>^> building engine...
    make -C engine WINDOWS=1 CROSS_COMPILE=x86_64-w64-mingw32-
    if errorlevel 1 exit /b %errorlevel%

    echo ^>^> building game...
    make -C game WINDOWS=1 CROSS_COMPILE=x86_64-w64-mingw32-
    if errorlevel 1 exit /b %errorlevel%
)

echo ^>^> launching game.exe

if exist "game\game.exe" (
    start "" "game\game.exe"
) else (
    echo Could not find game\game.exe
    exit /b 1
)

popd
exit /b 0

:help
echo play.bat
echo.
echo   play.bat         build engine+game, then run
echo   play.bat -n      skip build, just run
echo   play.bat -c      clean rebuild first
echo   play.bat -h      show help
exit /b 0