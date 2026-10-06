// RiftCore Editor entry point. The application lives in App/EditorApp.

#include "App/EditorApp.h"

#include <iostream>

// Options: --scene <file.json>  --script <file.py>  --select <node name>  --play
//          --show <tab>[,<tab>...]   (place, house, drawings, world, stats, console, python)
//          --house "<prompt>"        design a house on start-up;  --sheet <n>  drawing to show
int main(int argc, char** argv)
{
    RiftCore::EditorApp app;
    if (!app.Init(argc, argv)) {
        std::cerr << "RiftCore Editor failed to start. See RiftCoreEditor.log.\n";
        app.Shutdown();
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}
