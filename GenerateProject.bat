@echo off
echo =========================================
echo Regenerating RiftCore Visual Studio Files
echo =========================================
if exist Build rmdir /s /q Build
cmake -S . -B Build
echo.
echo Generation Complete! Your .sln file is ready in the Build folder.
pause
