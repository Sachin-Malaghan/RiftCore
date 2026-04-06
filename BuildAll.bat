@echo off
echo =========================================
echo Building RiftCore Engine (Debug)
echo =========================================
cmake --build Build --config Debug
echo.
echo Build Complete! Check the Build\bin folder for your Editor and Runtime.
pause
