@echo off
rem Starts the RiftCore Editor, building it first if needed.
setlocal
cd /d "%~dp0"

if not exist Build\bin\RiftCoreEditor.exe (
    call BuildAll.bat nopause
    if errorlevel 1 exit /b 1
)
start "" "Build\bin\RiftCoreEditor.exe" %*
