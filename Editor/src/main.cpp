// RiftCore Editor entry point. The application lives in App/EditorApp.

#include "App/EditorApp.h"

#include <iostream>

// Options: --scene <file.json>  --script <file.py>  --select <node name>  --play
//          --show <tab>[,<tab>...]   (place, house, drawings, world, stats, console, python)
//          --house "<prompt>"        design a house on start-up;  --sheet <n>  drawing to show
//          --style <0-3>            Solid, Realistic, Shaded with edges, Hidden line
//          --std-view <0-5>         Top, Front, Right, Left, Back, Isometric
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
