// Python must be included first. The official Windows installer ships no
// debug library, so Debug builds link the release Python as well.
#ifdef RIFTCORE_WITH_PYTHON
    #define PY_SSIZE_T_CLEAN
    #if defined(_MSC_VER) && defined(_DEBUG)
        #undef _DEBUG
        #include <Python.h>
        #define _DEBUG
    #else
        #include <Python.h>
    #endif
#endif

#include <Scripting/ScriptingModule.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Physics/IPhysics.h>
#include <RiftCore/Scene/SceneUtil.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace RiftCore {

    static ScriptingModule* g_module = nullptr;

#ifdef RIFTCORE_WITH_PYTHON

    // ============================================================
    //  riftcore Python module
    // ============================================================
    namespace {

        // "O&" converters -------------------------------------------------
        int ToVec3(PyObject* o, void* out) {
            Vec3* v = static_cast<Vec3*>(out);
            PyObject* seq = PySequence_Fast(o, "expected a sequence of 3 numbers");
            if (!seq) return 0;
            if (PySequence_Fast_GET_SIZE(seq) != 3) {
                Py_DECREF(seq);
                PyErr_SetString(PyExc_ValueError, "expected a sequence of 3 numbers");
                return 0;
            }
            f64 c[3];
            for (int i = 0; i < 3; i++) {
                c[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
            }
            Py_DECREF(seq);
            if (PyErr_Occurred()) return 0;
            *v = { static_cast<f32>(c[0]), static_cast<f32>(c[1]), static_cast<f32>(c[2]) };
            return 1;
        }

        // A scale may be one number (uniform) or three.
        int ToScale(PyObject* o, void* out) {
            if (PyFloat_Check(o) || PyLong_Check(o)) {
                f32 s = static_cast<f32>(PyFloat_AsDouble(o));
                if (PyErr_Occurred()) return 0;
                *static_cast<Vec3*>(out) = { s, s, s };
                return 1;
            }
            return ToVec3(o, out);
        }

        PyObject* FromVec3(const Vec3& v) {
            return Py_BuildValue("(fff)", v.x, v.y, v.z);
        }

        ISceneSystem* SceneOrError() {
            ISceneSystem* s = g_module ? g_module->Scene() : nullptr;
            if (!s) PyErr_SetString(PyExc_RuntimeError, "no scene system is loaded");
            return s;
        }

        ISceneNode* NodeOrError(ISceneSystem* scene, unsigned int id) {
            ISceneNode* n = scene->GetNode(id);
            if (!n) PyErr_Format(PyExc_ValueError, "no node with id %u", id);
            return n;
        }

        IPhysics* PhysicsOrError() {
            IPhysics* p = g_module ? g_module->Physics() : nullptr;
            if (!p) PyErr_SetString(PyExc_RuntimeError, "no physics system is loaded");
            return p;
        }

        // ---- Output / logging -------------------------------------------
        PyObject* py_write(PyObject*, PyObject* args) {
            const char* text = nullptr;
            if (!PyArg_ParseTuple(args, "s", &text)) return nullptr;
            if (g_module) g_module->AppendOutput(text);
            Py_RETURN_NONE;
        }

        PyObject* py_log(PyObject*, PyObject* args) {
            const char* text = nullptr;
            if (!PyArg_ParseTuple(args, "s", &text)) return nullptr;
            if (g_module) {
                if (ILogger* log = g_module->Logger()) log->Info("Script", text);
                g_module->AppendOutput(text);
                g_module->AppendOutput("\n");
            }
            Py_RETURN_NONE;
        }

        // ---- Scene ------------------------------------------------------
        PyObject* py_spawn(PyObject*, PyObject* args, PyObject* kw) {
            static const char* kwlist[] = {
                "kind", "name", "position", "rotation", "scale", "color", "physics",
                "mass", "metallic", "roughness", "restitution", "friction", "parent",
                nullptr };
            const char* kind = nullptr; const char* name = nullptr; const char* physics = nullptr;
            SceneNodeDesc d;
            d.mesh.albedo = { 0.8f, 0.8f, 0.8f };
            f32 mass = 1.0f;
            unsigned int parent = 0;
            if (!PyArg_ParseTupleAndKeywords(args, kw, "s|zO&O&O&O&zfffffI",
                    const_cast<char**>(kwlist), &kind, &name,
                    ToVec3, &d.position, ToVec3, &d.rotation, ToScale, &d.scale,
                    ToVec3, &d.mesh.albedo, &physics, &mass,
                    &d.mesh.metallic, &d.mesh.roughness,
                    &d.physics.restitution, &d.physics.friction, &parent)) {
                return nullptr;
            }
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            if (!SceneUtil::MakeNodeDesc(kind, d)) {
                PyErr_Format(PyExc_ValueError,
                    "unknown model '%s' (see riftcore.models())", kind);
                return nullptr;
            }
            if (name && *name) d.name = name;
            d.parentID = parent;
            SceneUtil::SetPhysicsMode(d, physics, mass);
            auto r = scene->CreateNode(d);
            if (r.IsErr()) {
                PyErr_SetString(PyExc_RuntimeError, r.Error().message.c_str());
                return nullptr;
            }
            return PyLong_FromUnsignedLong(r.Value());
        }

        PyObject* py_destroy(PyObject*, PyObject* args) {
            unsigned int id = 0;
            if (!PyArg_ParseTuple(args, "I", &id)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            scene->DestroyNode(id);
            Py_RETURN_NONE;
        }

        PyObject* py_clear(PyObject*, PyObject*) {
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            String name = scene->GetSceneInfo().name;
            scene->NewScene(name.empty() ? "Untitled" : name);
            Py_RETURN_NONE;
        }

        PyObject* py_find(PyObject*, PyObject* args) {
            const char* name = nullptr;
            if (!PyArg_ParseTuple(args, "s", &name)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            ISceneNode* n = scene->FindNode(name);
            if (!n) Py_RETURN_NONE;
            return PyLong_FromUnsignedLong(n->GetID());
        }

        PyObject* py_nodes(PyObject*, PyObject*) {
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            PyObject* list = PyList_New(0);
            scene->ForEachNode([&](ISceneNode* n) {
                PyObject* id = PyLong_FromUnsignedLong(n->GetID());
                PyList_Append(list, id);
                Py_DECREF(id);
            });
            return list;
        }

        PyObject* py_exists(PyObject*, PyObject* args) {
            unsigned int id = 0;
            if (!PyArg_ParseTuple(args, "I", &id)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            return PyBool_FromLong(scene->GetNode(id) != nullptr);
        }

        PyObject* py_get(PyObject*, PyObject* args) {
            unsigned int id = 0;
            if (!PyArg_ParseTuple(args, "I", &id)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            SceneNodeDesc d;
            ISceneNode* n = NodeOrError(scene, id);
            if (!n || !scene->GetNodeDesc(id, d)) return nullptr;
            const char* physics = !d.hasPhysics ? "none"
                                : d.physics.isStatic ? "static" : "dynamic";
            return Py_BuildValue(
                "{s:I,s:s,s:I,s:O,s:N,s:N,s:N,s:s,s:N,s:f,s:f,s:s,s:f,s:s}",
                "id", id, "name", d.name.c_str(), "parent", d.parentID,
                "active", n->IsActive() ? Py_True : Py_False,
                "position", FromVec3(d.position), "rotation", FromVec3(d.rotation),
                "scale", FromVec3(d.scale),
                "model", d.hasMesh ? d.mesh.meshPath.c_str() : "",
                "color", FromVec3(d.mesh.albedo),
                "metallic", d.mesh.metallic, "roughness", d.mesh.roughness,
                "physics", physics, "mass", d.physics.mass,
                "collider", d.hasPhysics ? d.physics.colliderShape.c_str() : "");
        }

        PyObject* py_set_name(PyObject*, PyObject* args) {
            unsigned int id = 0; const char* name = nullptr;
            if (!PyArg_ParseTuple(args, "Is", &id, &name)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            ISceneNode* n = scene ? NodeOrError(scene, id) : nullptr;
            if (!n) return nullptr;
            n->SetName(name);
            Py_RETURN_NONE;
        }

        PyObject* py_set_active(PyObject*, PyObject* args) {
            unsigned int id = 0; int active = 1;
            if (!PyArg_ParseTuple(args, "Ip", &id, &active)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            ISceneNode* n = scene ? NodeOrError(scene, id) : nullptr;
            if (!n) return nullptr;
            n->SetActive(active != 0);
            Py_RETURN_NONE;
        }

        // Transform getters / setters ---------------------------------------
        enum class Part { Position, Rotation, Scale };

        PyObject* GetPart(PyObject* args, Part part) {
            unsigned int id = 0;
            if (!PyArg_ParseTuple(args, "I", &id)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            ISceneNode* n = scene ? NodeOrError(scene, id) : nullptr;
            if (!n) return nullptr;
            switch (part) {
                case Part::Position: return FromVec3(n->GetLocalPosition());
                case Part::Rotation: return FromVec3(n->GetLocalRotation());
                default:             return FromVec3(n->GetLocalScale());
            }
        }

        PyObject* SetPart(PyObject* args, Part part) {
            unsigned int id = 0; Vec3 v;
            if (!PyArg_ParseTuple(args, "IO&", &id,
                    part == Part::Scale ? ToScale : ToVec3, &v)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            ISceneNode* n = scene ? NodeOrError(scene, id) : nullptr;
            if (!n) return nullptr;
            switch (part) {
                case Part::Position: n->SetLocalPosition(v); break;
                case Part::Rotation: n->SetLocalRotation(v); break;
                default:             n->SetLocalScale(v);    break;
            }
            Py_RETURN_NONE;
        }

        PyObject* py_get_position(PyObject*, PyObject* a) { return GetPart(a, Part::Position); }
        PyObject* py_get_rotation(PyObject*, PyObject* a) { return GetPart(a, Part::Rotation); }
        PyObject* py_get_scale   (PyObject*, PyObject* a) { return GetPart(a, Part::Scale); }
        PyObject* py_set_position(PyObject*, PyObject* a) { return SetPart(a, Part::Position); }
        PyObject* py_set_rotation(PyObject*, PyObject* a) { return SetPart(a, Part::Rotation); }
        PyObject* py_set_scale   (PyObject*, PyObject* a) { return SetPart(a, Part::Scale); }

        // Appearance / physics setup ---------------------------------------
        PyObject* py_set_color(PyObject*, PyObject* args) {
            unsigned int id = 0; Vec3 c;
            if (!PyArg_ParseTuple(args, "IO&", &id, ToVec3, &c)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene || !NodeOrError(scene, id)) return nullptr;
            SceneNodeDesc d;
            scene->GetNodeDesc(id, d);
            d.mesh.albedo = c;
            scene->UpdateNode(id, d);
            Py_RETURN_NONE;
        }

        PyObject* py_set_material(PyObject*, PyObject* args, PyObject* kw) {
            static const char* kwlist[] = { "id", "metallic", "roughness", nullptr };
            unsigned int id = 0; f32 metallic = -1.0f, roughness = -1.0f;
            if (!PyArg_ParseTupleAndKeywords(args, kw, "I|ff", const_cast<char**>(kwlist),
                    &id, &metallic, &roughness)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene || !NodeOrError(scene, id)) return nullptr;
            SceneNodeDesc d;
            scene->GetNodeDesc(id, d);
            if (metallic  >= 0.0f) d.mesh.metallic  = metallic;
            if (roughness >= 0.0f) d.mesh.roughness = roughness;
            scene->UpdateNode(id, d);
            Py_RETURN_NONE;
        }

        PyObject* py_set_model(PyObject*, PyObject* args) {
            unsigned int id = 0; const char* kind = nullptr;
            if (!PyArg_ParseTuple(args, "Is", &id, &kind)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene || !NodeOrError(scene, id)) return nullptr;
            SceneNodeDesc d, model;
            scene->GetNodeDesc(id, d);
            if (!SceneUtil::MakeNodeDesc(kind, model) || !model.hasMesh) {
                PyErr_Format(PyExc_ValueError, "unknown model '%s'", kind);
                return nullptr;
            }
            d.hasMesh       = true;
            d.mesh.meshPath = model.mesh.meshPath;
            if (d.hasPhysics) SceneUtil::FitCollider(d);
            scene->UpdateNode(id, d);
            Py_RETURN_NONE;
        }

        PyObject* py_set_physics(PyObject*, PyObject* args, PyObject* kw) {
            static const char* kwlist[] = {
                "id", "mode", "mass", "restitution", "friction", nullptr };
            unsigned int id = 0; const char* mode = nullptr;
            f32 mass = 1.0f, restitution = -1.0f, friction = -1.0f;
            if (!PyArg_ParseTupleAndKeywords(args, kw, "Iz|fff", const_cast<char**>(kwlist),
                    &id, &mode, &mass, &restitution, &friction)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene || !NodeOrError(scene, id)) return nullptr;
            SceneNodeDesc d;
            scene->GetNodeDesc(id, d);
            if (restitution >= 0.0f) d.physics.restitution = restitution;
            if (friction    >= 0.0f) d.physics.friction    = friction;
            SceneUtil::SetPhysicsMode(d, mode, mass);
            scene->UpdateNode(id, d);
            Py_RETURN_NONE;
        }

        // Dynamics ----------------------------------------------------------
        enum class Push { Velocity, Impulse, Force };

        PyObject* PushBody(PyObject* args, Push kind) {
            unsigned int id = 0; Vec3 v;
            if (!PyArg_ParseTuple(args, "IO&", &id, ToVec3, &v)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            ISceneNode* n = scene ? NodeOrError(scene, id) : nullptr;
            IPhysics* physics = n ? PhysicsOrError() : nullptr;
            if (!physics) return nullptr;
            switch (kind) {
                case Push::Velocity: physics->SetVelocity (n->GetEntityID(), v); break;
                case Push::Impulse:  physics->ApplyImpulse(n->GetEntityID(), v); break;
                default:             physics->ApplyForce  (n->GetEntityID(), v); break;
            }
            Py_RETURN_NONE;
        }

        PyObject* py_set_velocity (PyObject*, PyObject* a) { return PushBody(a, Push::Velocity); }
        PyObject* py_apply_impulse(PyObject*, PyObject* a) { return PushBody(a, Push::Impulse); }
        PyObject* py_apply_force  (PyObject*, PyObject* a) { return PushBody(a, Push::Force); }

        PyObject* py_get_velocity(PyObject*, PyObject* args) {
            unsigned int id = 0;
            if (!PyArg_ParseTuple(args, "I", &id)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            ISceneNode* n = scene ? NodeOrError(scene, id) : nullptr;
            IPhysics* physics = n ? PhysicsOrError() : nullptr;
            if (!physics) return nullptr;
            return FromVec3(physics->GetVelocity(n->GetEntityID()));
        }

        PyObject* py_set_gravity(PyObject*, PyObject* args) {
            Vec3 g;
            if (!PyArg_ParseTuple(args, "O&", ToVec3, &g)) return nullptr;
            IPhysics* physics = PhysicsOrError();
            if (!physics) return nullptr;
            physics->SetGravity(g);
            Py_RETURN_NONE;
        }

        PyObject* py_get_gravity(PyObject*, PyObject*) {
            IPhysics* physics = PhysicsOrError();
            if (!physics) return nullptr;
            return FromVec3(physics->GetGravity());
        }

        PyObject* py_raycast(PyObject*, PyObject* args) {
            Vec3 origin, dir; f32 maxDist = 1000.0f;
            if (!PyArg_ParseTuple(args, "O&O&|f", ToVec3, &origin, ToVec3, &dir, &maxDist)) {
                return nullptr;
            }
            IPhysics* physics = PhysicsOrError();
            if (!physics) return nullptr;
            RaycastHit hit = physics->Raycast(origin, dir, maxDist);
            if (!hit.hit) Py_RETURN_NONE;
            return Py_BuildValue("{s:f,s:N,s:N}", "distance", hit.distance,
                "point", FromVec3(hit.point), "normal", FromVec3(hit.normal));
        }

        // Catalog / scene files ---------------------------------------------
        PyObject* py_models(PyObject*, PyObject*) {
            u32 n = 0;
            const ModelCatalogEntry* k = GetModelCatalog(n);
            PyObject* list = PyList_New(n);
            for (u32 i = 0; i < n; i++) {
                PyList_SET_ITEM(list, i, PyUnicode_FromString(k[i].path));
            }
            return list;
        }

        PyObject* py_new_scene(PyObject*, PyObject* args) {
            const char* name = "Untitled";
            if (!PyArg_ParseTuple(args, "|s", &name)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            scene->NewScene(name);
            Py_RETURN_NONE;
        }

        PyObject* SceneFile(PyObject* args, bool save) {
            const char* path = nullptr;
            if (!PyArg_ParseTuple(args, "s", &path)) return nullptr;
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            auto r = save ? scene->SaveScene(path) : scene->LoadScene(path);
            if (r.IsErr()) {
                PyErr_SetString(PyExc_IOError, r.Error().message.c_str());
                return nullptr;
            }
            Py_RETURN_NONE;
        }

        PyObject* py_save_scene(PyObject*, PyObject* a) { return SceneFile(a, true); }
        PyObject* py_load_scene(PyObject*, PyObject* a) { return SceneFile(a, false); }

        PyObject* py_node_count(PyObject*, PyObject*) {
            ISceneSystem* scene = SceneOrError();
            if (!scene) return nullptr;
            return PyLong_FromUnsignedLong(scene->GetNodeCount());
        }

        // Per-frame callbacks -----------------------------------------------
        PyObject* py_on_update(PyObject*, PyObject* args) {
            PyObject* fn = nullptr;
            if (!PyArg_ParseTuple(args, "O", &fn)) return nullptr;
            if (!PyCallable_Check(fn)) {
                PyErr_SetString(PyExc_TypeError, "on_update expects a function taking dt");
                return nullptr;
            }
            Py_INCREF(fn);
            if (g_module) g_module->AddUpdateCallback(fn);
            Py_INCREF(fn);
            return fn;                       // usable as a decorator
        }

        PyObject* py_clear_callbacks(PyObject*, PyObject*) {
            if (g_module) g_module->ClearUpdateCallbacks();
            Py_RETURN_NONE;
        }

        PyObject* py_time(PyObject*, PyObject*) {
            return PyFloat_FromDouble(g_module ? g_module->GetTime() : 0.0);
        }

        PyObject* py_is_simulating(PyObject*, PyObject*) {
            return PyBool_FromLong(g_module && g_module->IsSimulating());
        }

        #define RC_KW(fn) reinterpret_cast<PyCFunction>(reinterpret_cast<void(*)(void)>(fn))

        PyMethodDef kMethods[] = {
            { "_write", py_write, METH_VARARGS, "internal: stdout sink" },
            { "log", py_log, METH_VARARGS, "log(text): write to the engine log and the console" },
            { "spawn", RC_KW(py_spawn), METH_VARARGS | METH_KEYWORDS,
              "spawn(kind, name=None, position=(0,0,0), rotation=(0,0,0), scale=1, "
              "color=(.8,.8,.8), physics=None, mass=1, metallic=0, roughness=.5, "
              "restitution=.4, friction=.5, parent=0) -> id\n"
              "kind: a name from models(), an .obj path, 'empty' or 'light'. "
              "physics: None, 'static' or 'dynamic'." },
            { "destroy", py_destroy, METH_VARARGS, "destroy(id): remove a node and its children" },
            { "clear", py_clear, METH_NOARGS, "clear(): remove every node" },
            { "find", py_find, METH_VARARGS, "find(name) -> id or None" },
            { "nodes", py_nodes, METH_NOARGS, "nodes() -> list of node ids" },
            { "exists", py_exists, METH_VARARGS, "exists(id) -> bool" },
            { "get", py_get, METH_VARARGS, "get(id) -> dict describing the node" },
            { "node_count", py_node_count, METH_NOARGS, "node_count() -> int" },
            { "set_name", py_set_name, METH_VARARGS, "set_name(id, name)" },
            { "set_active", py_set_active, METH_VARARGS, "set_active(id, bool): show / hide" },
            { "get_position", py_get_position, METH_VARARGS, "get_position(id) -> (x, y, z)" },
            { "set_position", py_set_position, METH_VARARGS, "set_position(id, (x, y, z))" },
            { "get_rotation", py_get_rotation, METH_VARARGS, "get_rotation(id) -> degrees (x, y, z)" },
            { "set_rotation", py_set_rotation, METH_VARARGS, "set_rotation(id, (x, y, z)) in degrees" },
            { "get_scale", py_get_scale, METH_VARARGS, "get_scale(id) -> (x, y, z)" },
            { "set_scale", py_set_scale, METH_VARARGS, "set_scale(id, s) or set_scale(id, (x, y, z))" },
            { "set_color", py_set_color, METH_VARARGS, "set_color(id, (r, g, b)) with 0..1 values" },
            { "set_material", RC_KW(py_set_material), METH_VARARGS | METH_KEYWORDS,
              "set_material(id, metallic=None, roughness=None)" },
            { "set_model", py_set_model, METH_VARARGS, "set_model(id, kind): change the mesh" },
            { "set_physics", RC_KW(py_set_physics), METH_VARARGS | METH_KEYWORDS,
              "set_physics(id, mode, mass=1, restitution=None, friction=None); "
              "mode: None, 'static' or 'dynamic'" },
            { "get_velocity", py_get_velocity, METH_VARARGS, "get_velocity(id) -> (x, y, z)" },
            { "set_velocity", py_set_velocity, METH_VARARGS, "set_velocity(id, (x, y, z))" },
            { "apply_impulse", py_apply_impulse, METH_VARARGS, "apply_impulse(id, (x, y, z))" },
            { "apply_force", py_apply_force, METH_VARARGS, "apply_force(id, (x, y, z)) for one step" },
            { "get_gravity", py_get_gravity, METH_NOARGS, "get_gravity() -> (x, y, z)" },
            { "set_gravity", py_set_gravity, METH_VARARGS, "set_gravity((x, y, z))" },
            { "raycast", py_raycast, METH_VARARGS,
              "raycast(origin, direction, max_distance=1000) -> dict or None" },
            { "models", py_models, METH_NOARGS, "models() -> names of the built-in models" },
            { "new_scene", py_new_scene, METH_VARARGS, "new_scene(name='Untitled')" },
            { "save_scene", py_save_scene, METH_VARARGS, "save_scene(path)" },
            { "load_scene", py_load_scene, METH_VARARGS, "load_scene(path)" },
            { "on_update", py_on_update, METH_VARARGS,
              "on_update(fn): call fn(dt) every simulated frame (also a decorator)" },
            { "clear_callbacks", py_clear_callbacks, METH_NOARGS,
              "clear_callbacks(): remove every on_update function" },
            { "time", py_time, METH_NOARGS, "time() -> simulated seconds" },
            { "is_simulating", py_is_simulating, METH_NOARGS, "is_simulating() -> bool" },
            { nullptr, nullptr, 0, nullptr }
        };

        PyModuleDef kModuleDef = {
            PyModuleDef_HEAD_INIT, "riftcore",
            "RiftCore engine scripting API. Call riftcore.api() for a list of functions.",
            -1, kMethods, nullptr, nullptr, nullptr, nullptr
        };

        PyMODINIT_FUNC PyInit_riftcore() {
            return PyModule_Create(&kModuleDef);
        }

        // Routes print() and tracebacks to the engine, and adds api().
        const char* kPrelude = R"PY(
import sys, riftcore

class _RiftCoreOut:
    def write(self, text):
        riftcore._write(str(text))
        return len(text)
    def flush(self):
        pass

sys.stdout = sys.stderr = _RiftCoreOut()

def _api():
    """Print every riftcore function with a one-line description."""
    for name in sorted(dir(riftcore)):
        fn = getattr(riftcore, name)
        if name.startswith('_') or not callable(fn):
            continue
        doc = (fn.__doc__ or '').strip().splitlines()
        print(doc[0] if doc else name)

riftcore.api = _api
for _p in ('Assets/Scripts', '.'):
    if _p not in sys.path:
        sys.path.insert(0, _p)
)PY";

        // Prints the pending Python error to the console. SystemExit must
        // not terminate the host application.
        void ReportPythonError() {
            if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
                PyErr_Clear();
                if (g_module) g_module->AppendOutput("[script called exit()]\n");
                return;
            }
            PyErr_Print();
        }

    } // namespace

#endif // RIFTCORE_WITH_PYTHON

    // ============================================================
    //  ScriptingModule
    // ============================================================

    ScriptingModule::ScriptingModule() = default;

    ScriptingModule::~ScriptingModule() {
        Shutdown();
    }

    ISceneSystem* ScriptingModule::Scene() const {
        return context_ ? context_->Get<ISceneSystem>() : nullptr;
    }

    IPhysics* ScriptingModule::Physics() const {
        return context_ ? context_->Get<IPhysics>() : nullptr;
    }

    ILogger* ScriptingModule::Logger() const {
        return context_ ? context_->Logger() : nullptr;
    }

    void ScriptingModule::AppendOutput(const char* text) {
        if (text) output_ += text;
    }

    const char* ScriptingModule::ConsumeOutput() {
        consumed_.swap(output_);
        output_.clear();
        return consumed_.c_str();
    }

    VoidResult ScriptingModule::Initialize(const ModuleInitParams& params) {
        if (initialized_) return VoidResult::Ok();
        context_     = params.context;
        initialized_ = true;
        g_module     = this;
        if (context_) context_->Register<IScripting>(this);
        ILogger* log = Logger();

#ifdef RIFTCORE_WITH_PYTHON
        PyImport_AppendInittab("riftcore", PyInit_riftcore);

        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        config.install_signal_handlers = 0;
        config.parse_argv              = 0;
    #ifdef RIFTCORE_PYTHON_HOME
        // Use the Python the engine was built against unless the user
        // points somewhere else with PYTHONHOME.
        std::error_code ec;
        if (!std::getenv("PYTHONHOME") &&
            std::filesystem::is_directory(std::filesystem::path(RIFTCORE_PYTHON_HOME) / "Lib", ec)) {
            PyConfig_SetBytesString(&config, &config.home, RIFTCORE_PYTHON_HOME);
        }
    #endif
        PyStatus status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);

        if (PyStatus_Exception(status)) {
            available_ = false;
            if (log) log->Warning("Scripting", String("Python could not start: ") +
                (status.err_msg ? status.err_msg : "unknown error") +
                ". Install Python " RIFTCORE_PYTHON_VERSION " or set PYTHONHOME.");
            return VoidResult::Ok();
        }

        available_ = true;
        if (PyRun_SimpleString(kPrelude) != 0) {
            if (log) log->Warning("Scripting", "Python prelude failed.");
        }
        if (log) log->Info("Scripting", String("Python ") + Py_GetVersion());
#else
        available_ = false;
        if (log) log->Warning("Scripting",
            "Built without Python (not found at configure time): scripting is disabled.");
#endif
        return VoidResult::Ok();
    }

    void ScriptingModule::Shutdown() {
        if (!initialized_) return;
        initialized_ = false;
#ifdef RIFTCORE_WITH_PYTHON
        if (available_) {
            ClearUpdateCallbacks();
            Py_FinalizeEx();
        }
#endif
        available_ = false;
        if (context_) context_->Unregister<IScripting>();
        if (g_module == this) g_module = nullptr;
    }

    void ScriptingModule::AddUpdateCallback(void* pyCallable) {
        updateCallbacks_.push_back(pyCallable);
    }

    void ScriptingModule::ClearUpdateCallbacks() {
#ifdef RIFTCORE_WITH_PYTHON
        for (void* cb : updateCallbacks_) Py_DECREF(static_cast<PyObject*>(cb));
#endif
        updateCallbacks_.clear();
    }

    void ScriptingModule::OnUpdate(f32 deltaTime) {
        if (!initialized_ || !available_ || !simulating_) return;
        time_ += deltaTime;
#ifdef RIFTCORE_WITH_PYTHON
        // A callback may register or clear callbacks: iterate over a copy.
        std::vector<void*> callbacks = updateCallbacks_;
        for (void* cb : callbacks) {
            PyObject* fn = static_cast<PyObject*>(cb);
            Py_INCREF(fn);
            PyObject* r = PyObject_CallFunction(fn, "f", deltaTime);
            if (!r) {
                // A failing callback is removed so it does not spam every frame.
                ReportPythonError();
                AppendOutput("[on_update callback removed after error]\n");
                auto it = std::find(updateCallbacks_.begin(), updateCallbacks_.end(), cb);
                if (it != updateCallbacks_.end()) {
                    updateCallbacks_.erase(it);
                    Py_DECREF(fn);
                }
            } else {
                Py_DECREF(r);
            }
            Py_DECREF(fn);
        }
#endif
    }

    ModuleDescriptor ScriptingModule::GetDescriptor() const {
        ModuleDescriptor desc;
        desc.name        = "Scripting";
        desc.version     = "2.0.0";
        desc.apiVersion  = RIFTCORE_API_VERSION;
        desc.description = "Embedded Python scripting (riftcore module)";
        return desc;
    }

    VoidResult ScriptingModule::Run(const std::string& source, const char* fileName,
                                    bool interactive)
    {
        if (!initialized_) return VoidResult::Err("Scripting module not initialized");
#ifdef RIFTCORE_WITH_PYTHON
        if (!available_) return VoidResult::Err("Python is not available");

        PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));

        // A single expression or statement echoes its value like the Python
        // prompt does; anything longer runs as a normal script.
        PyObject* code = nullptr;
        if (interactive) {
            code = Py_CompileString(source.c_str(), fileName, Py_single_input);
            if (!code) PyErr_Clear();
        }
        if (!code) code = Py_CompileString(source.c_str(), fileName, Py_file_input);
        if (!code) {
            ReportPythonError();
            return VoidResult::Err("Python syntax error");
        }

        PyObject* result = PyEval_EvalCode(code, globals, globals);
        Py_DECREF(code);
        if (!result) {
            ReportPythonError();
            return VoidResult::Err("Python error");
        }
        Py_DECREF(result);
        return VoidResult::Ok();
#else
        RIFTCORE_UNUSED(source); RIFTCORE_UNUSED(fileName); RIFTCORE_UNUSED(interactive);
        return VoidResult::Err("Engine was built without Python");
#endif
    }

    VoidResult ScriptingModule::ExecuteString(const char* code) {
        if (!code) return VoidResult::Err("Code string is null");
        return Run(code, "<console>", true);
    }

    VoidResult ScriptingModule::LoadScript(const char* filePath) {
        if (!filePath) return VoidResult::Err("Script path is null");
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return VoidResult::Err(String("Cannot open script: ") + filePath);
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return Run(ss.str(), filePath, false);
    }

    void ScriptingModule::RegisterFunction(const char* name, void(*fn)()) {
        // Native callbacks are exposed through the riftcore module instead.
        RIFTCORE_UNUSED(name);
        RIFTCORE_UNUSED(fn);
    }

    // ── DLL exports ──────────────────────────────────────────
    extern "C" {
        RIFTCORE_EXPORT IModule* CreateModule() {
            return new ScriptingModule();
        }

        RIFTCORE_EXPORT void DestroyModule(IModule* module) {
            delete module;
        }
    }

} // namespace RiftCore
