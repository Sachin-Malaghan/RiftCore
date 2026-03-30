// ============================================================
// IECS.h
// Entity Component System interface.
// ============================================================
#pragma once

#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"

#include <functional>
#include <typeindex>

namespace RiftCore {

    // ── Entity ────────────────────────────────────────────────
    // Just a typed integer ID. Has NO data or methods.
    // Data lives in Components, behavior in Systems.
    using EntityID = u64;
    constexpr EntityID INVALID_ENTITY = 0;

    // ── Component concept ─────────────────────────────────────
    // Components are plain data structs — no virtual functions
    // Example:
    //   struct TransformComponent { Vec3 position; Quat rotation; };
    //   struct MeshComponent      { MeshHandle mesh; };
    //   struct PhysicsComponent   { f32 mass; Vec3 velocity; };

    // ── System function type ──────────────────────────────────
    using SystemFunction = std::function<void(f32 deltaTime)>;

    // ── IECS interface ────────────────────────────────────────
    class IECS {
    public:
        virtual ~IECS() = default;

        // ── Entity management ─────────────────────────────────
        virtual EntityID  CreateEntity()                        = 0;
        virtual void      DestroyEntity(EntityID entity)        = 0;
        virtual bool      IsAlive(EntityID entity)        const = 0;
        virtual u32       GetEntityCount()                const = 0;

        // ── Component management ──────────────────────────────
        // AddComponent<T>(entity, T{...})
        // RemoveComponent<T>(entity)
        // GetComponent<T>(entity) → T* or nullptr
        // HasComponent<T>(entity) → bool

        template<typename T>
        T* AddComponent(EntityID entity, T component) {
            return static_cast<T*>(
                AddComponentInternal(
                    entity,
                    std::type_index(typeid(T)),
                    &component,
                    sizeof(T)
                )
            );
        }

        template<typename T>
        void RemoveComponent(EntityID entity) {
            RemoveComponentInternal(
                entity,
                std::type_index(typeid(T))
            );
        }

        template<typename T>
        T* GetComponent(EntityID entity) {
            return static_cast<T*>(
                GetComponentInternal(
                    entity,
                    std::type_index(typeid(T))
                )
            );
        }

        template<typename T>
        bool HasComponent(EntityID entity) const {
            return HasComponentInternal(
                entity,
                std::type_index(typeid(T))
            );
        }

        // ── Query / iteration ─────────────────────────────────
        // Iterate all entities that have BOTH T and U components
        template<typename T, typename U>
        void ForEach(std::function<void(EntityID, T&, U&)> fn) {
            ForEachInternal(
                {std::type_index(typeid(T)), std::type_index(typeid(U))},
                [&fn](EntityID id, std::vector<void*>& ptrs) {
                    fn(id,
                       *static_cast<T*>(ptrs[0]),
                       *static_cast<U*>(ptrs[1]));
                }
            );
        }

        template<typename T>
        void ForEach(std::function<void(EntityID, T&)> fn) {
            ForEachInternal(
                {std::type_index(typeid(T))},
                [&fn](EntityID id, std::vector<void*>& ptrs) {
                    fn(id, *static_cast<T*>(ptrs[0]));
                }
            );
        }

        // ── System registration ───────────────────────────────
        virtual void RegisterSystem(
            const String&  name,
            SystemFunction fn,
            i32            priority = 0
        ) = 0;

        virtual void UnregisterSystem(const String& name) = 0;

        // Runs all registered systems in priority order
        virtual void UpdateSystems(f32 deltaTime) = 0;

    protected:
        // Internal virtual functions that templates call
        virtual void* AddComponentInternal(
            EntityID entity, std::type_index type,
            const void* data, usize size
        ) = 0;

        virtual void  RemoveComponentInternal(
            EntityID entity, std::type_index type
        ) = 0;

        virtual void* GetComponentInternal(
            EntityID entity, std::type_index type
        ) const = 0;

        virtual bool  HasComponentInternal(
            EntityID entity, std::type_index type
        ) const = 0;

        virtual void  ForEachInternal(
            std::vector<std::type_index> types,
            std::function<void(EntityID, std::vector<void*>&)> fn
        ) = 0;
    };

} // namespace RiftCore