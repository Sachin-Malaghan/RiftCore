@echo off
rem Regenerates the Visual Studio solution in Build\ (keeps existing binaries).
setlocal
cd /d "%~dp0"

echo =========================================
echo  Generating RiftCore Visual Studio files
echo =========================================
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64
echo.
echo Done. Open Build\RiftCore.sln or run OpenSolution.bat
pause
