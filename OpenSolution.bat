@echo off
if exist Build\RiftCore.sln (
    echo Opening RiftCore in Visual Studio...
    start Build\RiftCore.sln
) else (
    echo Solution not found! Please run GenerateProject.bat first to let CMake create it.
    pause
)
