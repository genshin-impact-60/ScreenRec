@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
title ScreenRec pack

echo ========================================
echo   ScreenRec pack
echo ========================================
echo.

set "APP_NAME=ScreenRec"
set "APP_VERSION=1.0.0"
set "BUILD_DIR=%~dp0build-release"
set "DIST_DIR=%~dp0dist\%APP_NAME%"
set "ZIP_PATH=%~dp0dist\%APP_NAME%-%APP_VERSION%-win64.zip"

if not defined QT_DIR set "QT_DIR=D:\APP\Qt\6.10.2\mingw_64"
if not defined MINGW_DIR set "MINGW_DIR=D:\APP\Qt\Tools\mingw1310_64"
if not defined CMAKE_EXE set "CMAKE_EXE=D:\APP\Cmake\bin\cmake.exe"

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    for /d %%D in ("D:\APP\Qt\*\mingw_64") do (
        if exist "%%D\bin\windeployqt.exe" set "QT_DIR=%%D"
    )
)
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    for /d %%D in ("C:\Qt\*\mingw_64") do (
        if exist "%%D\bin\windeployqt.exe" set "QT_DIR=%%D"
    )
)

if not exist "%MINGW_DIR%\bin\g++.exe" (
    for /d %%D in ("D:\APP\Qt\Tools\mingw*_64") do (
        if exist "%%D\bin\g++.exe" set "MINGW_DIR=%%D"
    )
)
if not exist "%MINGW_DIR%\bin\g++.exe" (
    for /d %%D in ("C:\Qt\Tools\mingw*_64") do (
        if exist "%%D\bin\g++.exe" set "MINGW_DIR=%%D"
    )
)

if not exist "%CMAKE_EXE%" (
    where cmake >nul 2>&1
    if not errorlevel 1 (
        for /f "delims=" %%I in ('where cmake') do (
            set "CMAKE_EXE=%%I"
            goto cmake_found
        )
    )
)
:cmake_found

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [FAIL] Qt not found. Set QT_DIR to the mingw_64 folder, e.g.
    echo        set QT_DIR=D:\APP\Qt\6.10.2\mingw_64
    goto fail
)
if not exist "%MINGW_DIR%\bin\g++.exe" (
    echo [FAIL] MinGW not found. Set MINGW_DIR to the compiler folder, e.g.
    echo        set MINGW_DIR=D:\APP\Qt\Tools\mingw1310_64
    goto fail
)
if not exist "%CMAKE_EXE%" (
    echo [FAIL] cmake.exe not found. Set CMAKE_EXE to the full path, e.g.
    echo        set CMAKE_EXE=D:\APP\Cmake\bin\cmake.exe
    goto fail
)

set "PATH=%MINGW_DIR%\bin;%QT_DIR%\bin;%PATH%"

echo Qt      : %QT_DIR%
echo MinGW   : %MINGW_DIR%
echo CMake   : %CMAKE_EXE%
echo Output  : %DIST_DIR%
echo.

echo [1/4] CMake configure Release ...
"%CMAKE_EXE%" -S "%~dp0." -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 (
    echo [FAIL] CMake configure failed
    goto fail
)

echo.
echo [2/4] Build %APP_NAME%.exe ...
"%CMAKE_EXE%" --build "%BUILD_DIR%" -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [FAIL] Build failed
    goto fail
)
if not exist "%BUILD_DIR%\%APP_NAME%.exe" (
    echo [FAIL] Missing %BUILD_DIR%\%APP_NAME%.exe
    goto fail
)

echo.
echo [3/4] Deploy Qt runtime ...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
copy /y "%BUILD_DIR%\%APP_NAME%.exe" "%DIST_DIR%\" >nul
if exist "%~dp0README.md" copy /y "%~dp0README.md" "%DIST_DIR%\" >nul

"%QT_DIR%\bin\windeployqt.exe" --release --compiler-runtime --no-translations --force --dir "%DIST_DIR%" "%DIST_DIR%\%APP_NAME%.exe"
if errorlevel 1 (
    echo [FAIL] windeployqt failed
    goto fail
)

if exist "%QT_DIR%\plugins\multimedia" (
    if not exist "%DIST_DIR%\multimedia" mkdir "%DIST_DIR%\multimedia"
    copy /y "%QT_DIR%\plugins\multimedia\*.dll" "%DIST_DIR%\multimedia\" >nul
)

if exist "%QT_DIR%\bin\avcodec-*.dll" copy /y "%QT_DIR%\bin\avcodec-*.dll" "%DIST_DIR%\" >nul
if exist "%QT_DIR%\bin\avformat-*.dll" copy /y "%QT_DIR%\bin\avformat-*.dll" "%DIST_DIR%\" >nul
if exist "%QT_DIR%\bin\avutil-*.dll" copy /y "%QT_DIR%\bin\avutil-*.dll" "%DIST_DIR%\" >nul
if exist "%QT_DIR%\bin\swresample-*.dll" copy /y "%QT_DIR%\bin\swresample-*.dll" "%DIST_DIR%\" >nul
if exist "%QT_DIR%\bin\swscale-*.dll" copy /y "%QT_DIR%\bin\swscale-*.dll" "%DIST_DIR%\" >nul

for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_DIR%\bin\%%F" copy /y "%MINGW_DIR%\bin\%%F" "%DIST_DIR%\" >nul
    if exist "%QT_DIR%\bin\%%F" copy /y "%QT_DIR%\bin\%%F" "%DIST_DIR%\" >nul
)

if not exist "%DIST_DIR%\platforms\qwindows.dll" (
    echo [FAIL] Missing platforms\qwindows.dll
    goto fail
)
if not exist "%DIST_DIR%\multimedia\ffmpegmediaplugin.dll" (
    echo [FAIL] Missing multimedia\ffmpegmediaplugin.dll
    goto fail
)

echo.
echo [4/4] Create zip ...
if exist "%ZIP_PATH%" del /f /q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%' -DestinationPath '%ZIP_PATH%' -Force"
if errorlevel 1 (
    echo [WARN] zip failed; folder is ready at dist\%APP_NAME%
) else (
    echo zip : %ZIP_PATH%
)

echo.
echo ========================================
echo   Pack OK
echo   Folder : %DIST_DIR%
echo   Zip    : %ZIP_PATH%
echo ========================================
echo.
if /i not "%PACK_NOPAUSE%"=="1" explorer.exe "%DIST_DIR%"
if /i not "%PACK_NOPAUSE%"=="1" pause
exit /b 0

:fail
echo.
echo Pack failed. Window stays open so you can read the error.
if /i not "%PACK_NOPAUSE%"=="1" pause
exit /b 1