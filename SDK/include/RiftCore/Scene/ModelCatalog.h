#pragma once
// Built-in model library. A scene node's mesh path is one of:
//   "primitive:<name>"  - a basic shape
//   "model:<name>"      - a ready-made prop assembled from shapes
//   "<file>.obj"        - a mesh file, relative to the project root
// The Renderer generates the geometry (MeshFactory::CreateBuiltin).
#include <RiftCore/Common/Types.h>
#include <cstring>

namespace RiftCore {

    struct ModelCatalogEntry {
        const char* path;      // value stored in SceneMeshDesc::meshPath
        const char* label;     // name shown in the editor
        const char* category;  // "Shapes" or "Props"
        const char* collider;  // suggested collider: "box" or "sphere"
    };

    inline const ModelCatalogEntry* GetModelCatalog(u32& count) {
        static const ModelCatalogEntry k[] = {
            { "primitive:cube",       "Cube",        "Shapes", "box"    },
            { "primitive:sphere",     "Sphere",      "Shapes", "sphere" },
            { "primitive:plane",      "Plane",       "Shapes", "box"    },
            { "primitive:cylinder",   "Cylinder",    "Shapes", "box"    },
            { "primitive:cone",       "Cone",        "Shapes", "box"    },
            { "primitive:capsule",    "Capsule",     "Shapes", "box"    },
            { "primitive:torus",      "Torus",       "Shapes", "box"    },
            { "primitive:pyramid",    "Pyramid",     "Shapes", "box"    },
            { "primitive:wedge",      "Wedge",       "Shapes", "box"    },
            { "primitive:tube",       "Tube",        "Shapes", "box"    },
            { "primitive:hemisphere", "Dome",        "Shapes", "box"    },
            { "primitive:disc",       "Disc",        "Shapes", "box"    },
            { "primitive:stairs",     "Stairs",      "Shapes", "box"    },
            { "primitive:gem",        "Gem",         "Shapes", "sphere" },
            { "model:tree_pine",      "Pine Tree",   "Props",  "box"    },
            { "model:tree_round",     "Round Tree",  "Props",  "box"    },
            { "model:rock",           "Rock",        "Props",  "sphere" },
            { "model:table",          "Table",       "Props",  "box"    },
            { "model:chair",          "Chair",       "Props",  "box"    },
            { "model:barrel",         "Barrel",      "Props",  "box"    },
            { "model:crate",          "Crate",       "Props",  "box"    },
            { "model:lamp_post",      "Street Lamp", "Props",  "box"    },
            { "model:fence",          "Fence",       "Props",  "box"    },
            { "model:rocket",         "Rocket",      "Props",  "box"    },
            { "model:car",            "Car",         "Props",  "box"    },
            { "model:house",          "House",       "Props",  "box"    },
            { "model:tower",          "Tower",       "Props",  "box"    },
            { "model:bridge",         "Bridge",      "Props",  "box"    },
            { "model:satellite",      "Satellite",   "Props",  "box"    },
            { "model:robot",          "Robot",       "Props",  "box"    },
        };
        count = static_cast<u32>(sizeof(k) / sizeof(k[0]));
        return k;
    }

    // Accepts "cube", "primitive:cube", "tree_pine", "model:tree_pine".
    inline const ModelCatalogEntry* FindModel(const char* name) {
        if (!name) return nullptr;
        u32 n = 0;
        const ModelCatalogEntry* k = GetModelCatalog(n);
        for (u32 i = 0; i < n; i++) {
            if (std::strcmp(k[i].path, name) == 0) return &k[i];
            const char* colon = std::strchr(k[i].path, ':');
            if (colon && std::strcmp(colon + 1, name) == 0) return &k[i];
        }
        return nullptr;
    }

} // namespace RiftCore
