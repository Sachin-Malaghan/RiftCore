@echo off
rem Configures (first run) and builds the whole engine in Release.
rem Needs only CMake 3.20+ and Visual Studio 2022 with the C++ workload.
setlocal
cd /d "%~dp0"

echo =========================================
echo  Building RiftCore (Release)
echo =========================================
if not exist Build\CMakeCache.txt (
    cmake -S . -B Build -G "Visual Studio 17 2022" -A x64
    if errorlevel 1 goto :fail
)
cmake --build Build --config Release -- -m -nologo -v:m
if errorlevel 1 goto :fail

echo.
echo Build complete. Start the editor with RunEditor.bat
if not "%1"=="nopause" pause
exit /b 0

:fail
echo.
echo BUILD FAILED - see the messages above.
if not "%1"=="nopause" pause
exit /b 1
