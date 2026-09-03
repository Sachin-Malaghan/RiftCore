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
| Physics | Stub | Done | amplify it
| Audio | Stub | Planned |
| Asset | Stub | Planned |
| Scene | Stub | Planned |
| Scripting | Stub | Planned |
| VR | Stub | Planned |
| in editor Panels are comming
| panel needs to be connected to backend actions 


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

SYSTEM CONTEXT: RIFTCORE ENGINE ARCHITECTURE
Project Name: RiftCore
Domain: Custom C++ 3D Game Engine / Simulation Framework
Core Philosophy: Data-Oriented Design (DoD), Strict Interface Segregation, Modular Plugin Architecture (DLL-based late binding), Exception-less Error Handling (Result<T>).

1. High-Level Layered Architecture
The engine is strictly divided into 4 layers to prevent circular dependencies and allow hot-swapping of backend implementations.

Layer 1: SDK (The Contract)
Purpose: Contains pure virtual interfaces, primitive types, and math libraries. Other layers only link against the SDK.

Key Interfaces: IModule, IRHI (Render Hardware Interface), IECS, IAudio, IPhysics, ISceneSystem, IAssetSystem, IJobSystem.

Core Types: * Result<T> and VoidResult for monadic error handling (no try/catch).

Type-safe Handles (e.g., Handle<TextureTag>) to prevent ID mismatch.

Custom Math library (Vec3, Mat4, Quat).

Layer 2: Core (The Foundation)
Purpose: Engine lifecycle, resource management, and cross-module communication.

EngineContext (Service Locator Pattern): A type-safe map (std::type_index -> void*) holding pointers to active systems. Modules request dependencies via context->Require<IPhysics>().

EventBus: Pub/Sub system for decoupled communication (e.g., InputSystem publishes KeyPressedEvent). Supports immediate and queued dispatch.

Memory Allocators: Three strategies: SystemAllocator (standard heap), LinearAllocator (per-frame temporary allocations, resets at EndFrame), and PoolAllocator (fixed-size blocks for ECS components).

Job System: Multi-threaded ThreadPool using std::priority_queue and std::shared_future (JobState / JobHandle) for asynchronous tasks and ParallelFor loops.

Layer 3: Modules (The Implementations)
Purpose: Concrete logic compiled as dynamic libraries (DLLs). Loaded at runtime by the PluginLoader via CreateModule() C-exports.

Current Modules:

OpenGLBackend: Implements IRHI using a Command List pattern (IRHICommandList) to prepare for future Vulkan/DX12 support.

Physics: Impulse-based rigid body dynamics with Broadphase (AABB) and Narrowphase collision detection. Runs on a fixed timestep accumulator.

ECS: Sparse-set / Dense-array hybrid (ComponentPool<T>). Systems iterate over contiguous memory via ForEach templates to maximize CPU cache locality.

Audio: Abstraction over miniaudio.

Renderer: High-level rendering, material systems, and OBJ/Texture loaders.

Layer 4: Application / Editor (The Consumer)
Purpose: Orchestrates the engine for specific tools (e.g., HousePlanEditor).

SceneSystem: Manages hierarchical SceneNodes. Acts as a facade, automatically creating corresponding ECS Entities and Physics RigidBody objects when a node is created. Uses a dirty_ flag pattern for transform hierarchies.

EditorUI & Gizmos: WYSIWYG interface using ImGui. Uses HUDCallbacks to decouple UI buttons from engine logic. GizmoSystem handles Screen-to-Ray casting and Matrix Decomposition for 3D manipulation.

2. Critical Data Flows/
The Frame Loop: Input Update -> JobSystem/ECS Systems Update -> Fixed Physics Step -> Scene Transform Sync -> Renderer Submit (DrawCalls) -> RHI Execute & Present -> Linear Allocator Reset.

Scene to ECS/Physics Sync: SceneNode (Hierarchy/Transforms) explicitly stores EntityID and physicsBodyID_. When a node moves, it updates the physics body and ECS transform component, bridging human-readable hierarchies with data-oriented arrays.

3. Current Development State
Complete: SDK, Core, OpenGL RHI, Physics, ECS, Editor UI, Asset Loading.

Pending / Stubs: IScripting (needs Lua/C#/python binding implementation) and IVRModule (needs OpenXR/OpenVR implementation and dual-eye render pass logic).








