@echo off
setlocal
cd /d "%~dp0"

set "VCPKG_DIR=C:\dev\vcpkg"
set "TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake"

if not exist "%TOOLCHAIN_FILE%" (
    echo vcpkg not found in %VCPKG_DIR%
    pause
    exit /b 1
)

if exist vs rmdir /s /q vs

cmake -S . -B vs -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md %*
if errorlevel 1 (
    echo CMake generation failed.
    pause
    exit /b 1
)

echo Solution generated: vs\order_cache.sln
