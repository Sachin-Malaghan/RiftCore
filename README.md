# RiftCore Engine

A production-grade modular C++ game engine built from scratch.

## Architecture

RiftCore is fully modular — each system is a separate DLL
loaded at runtime with zero hard dependencies between modules.

## Modules Built

| Module | Status | Description |
|--------|--------|-------------|
| Core | Complete | Logger, EventBus, Memory, PluginManager |
| JobSystem | Complete | Thread pool, 19 workers, ParallelFor |
| ECS | Complete | World, ComponentPools, Systems |
| OpenGLBackend | Complete | OpenGL 4.6, GLFW window |
| Input | Complete | Keyboard, Mouse, GLFW callbacks |
| Renderer | Complete | Camera, Lighting, Textures, OBJ loader, HUD |
| Physics | Stub | Planned |
| Audio | Stub | Planned |
| Asset | Stub | Planned |
| Scene | Stub | Planned |
| Scripting | Stub | Planned |
| VR | Stub | Planned |

## Features

- Modular DLL architecture (plug and play)
- OpenGL 4.6 rendering with Blinn-Phong lighting
- Procedural texture generation + stb_image file loading
- OBJ model loading with MTL material support
- UV mapping and texture tiling
- FPS camera (WASD + arrow keys)
- Wireframe debug mode
- ImGui HUD overlay with real-time stats
- Multi-object selection and transform system
- ECS world with component pools
- Thread pool job system (19 workers)
- Full event bus (publish/subscribe)

## Requirements

- Visual Studio 2022
- CMake 3.20+
- vcpkg with: glfw3, glad, imgui, opengl

## Build Instructions

bash
cd RiftCore
cmake --preset windows-debug
cmake --build Build/Windows-Debug --config Debug


## Controls (Demo)

| Key | Action |
|-----|--------|
| WASD | Move camera |
| Arrow Keys | Rotate camera |
| TAB | Cycle selected object |
| IJKL | Move selected object |
| Num4/6 | Rotate Y axis |
| Num8/2 | Rotate X axis |
| Z/X | Scale down/up |
| C | Reset transform |
| F | Toggle wireframe |
| H | Toggle HUD |
| ESC | Quit |

## GPU Tested

NVIDIA Quadro M4000 - OpenGL 4.6 - 60fps stable

## License

MIT
