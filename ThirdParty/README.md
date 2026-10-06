# Third-party code

Everything RiftCore depends on is vendored here as plain source, so a fresh
clone builds with only CMake and a C++17 compiler - no package manager, no
git submodules.

| Library | Version | License | Used for |
|---|---|---|---|
| [GLFW](https://www.glfw.org) (`glfw/`) | 3.4 | zlib | window, input, OpenGL context (built as `glfw3.dll`) |
| [glad](https://glad.dav1d.de) (`glad/`) | generated, OpenGL 4.6 compatibility | MIT / Khronos | OpenGL function loader |
| [Dear ImGui](https://github.com/ocornut/imgui) (`imgui/`) | 1.92.6 | MIT | editor UI (+ GLFW / OpenGL3 backends) |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) (`imguizmo/`) | - | MIT | transform gizmos |
| [nlohmann/json](https://github.com/nlohmann/json) (`json/`) | single header | MIT | scene files |
| [stb_image](https://github.com/nothings/stb) (`stb/`) | single header | MIT / public domain | texture loading |
| [miniaudio](https://miniaud.io) (`miniaudio/`) | single header | MIT-0 / public domain | audio |

Each folder keeps the library's own license file where it ships one.

Python is the one optional external dependency: the Scripting module embeds
whichever Python 3 (with development files) CMake finds on the machine.
