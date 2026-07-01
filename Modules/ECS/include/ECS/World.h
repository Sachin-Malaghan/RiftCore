#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/ECS/IECS.h>

#include <ECS/ComponentPool.h>

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>
#include <mutex>
#include <atomic>

#ifdef ECS_EXPORTS
    #define ECS_API RIFTCORE_EXPORT
#else
    #define ECS_API RIFTCORE_IMPORT
#endif






namespace RiftCore {

    struct SystemEntry {
        String         name;
        SystemFunction function;
        i32            priority = 0;
        bool           enabled  = true;
    };

    class ECS_API World : public IECS {
    public:
        World();
        ~World();

        RIFTCORE_NOCOPY_NOMOVE(World);

        // ── Entity management ─────────────────────────────────
        EntityID CreateEntity()                      override;
        void     DestroyEntity(EntityID entity)      override;
        bool     IsAlive(EntityID entity)      const override;
        u32      GetEntityCount()              const override;

        // ── System management ─────────────────────────────────
        void RegisterSystem(
            const String&  name,
            SystemFunction fn,
            i32            priority = 0
        ) override;

        void UnregisterSystem(const String& name) override;
        void UpdateSystems(f32 deltaTime)         override;

        // ── Typed component access ────────────────────────────
        template<typename T>
        T* Add(EntityID entity, T component) {
            auto pool = GetOrCreatePool<T>();
            return pool->AddComponent(entity, std::move(component));
        }

        template<typename T>
        void Remove(EntityID entity) {
            auto pool = GetPool<T>();
            if (pool) pool->RemoveComponent(entity);
        }

        template<typename T>
        T* Get(EntityID entity) {
            auto pool = GetPool<T>();
            if (!pool) return nullptr;
            return pool->GetComponent(entity);
        }

        template<typename T>
        bool Has(EntityID entity) const {
            auto pool = GetPoolConst<T>();
            if (!pool) return false;
            return pool->HasComponent(entity);
        }

        // ── Iteration ─────────────────────────────────────────
        template<typename T>
        void ForEach(std::function<void(EntityID, T&)> fn) {
            auto pool = GetPool<T>();
            if (!pool) return;
            auto& entities    = pool->GetEntities();
            auto& components  = pool->GetComponents();
            for (usize i = 0; i < entities.size(); ++i) {
                fn(entities[i], components[i]);
            }
        }

        template<typename T, typename U>
        void ForEach(std::function<void(EntityID, T&, U&)> fn) {
            auto poolT = GetPool<T>();
            auto poolU = GetPool<U>();
            if (!poolT || !poolU) return;

            auto& entities = poolT->GetEntities();
            for (usize i = 0; i < entities.size(); ++i) {
                EntityID id = entities[i];
                U* u = poolU->GetComponent(id);
                if (u) {
                    fn(id, poolT->GetComponents()[i], *u);
                }
            }
        }

        template<typename T, typename U, typename V>
        void ForEach(
            std::function<void(EntityID, T&, U&, V&)> fn
        ) {
            auto poolT = GetPool<T>();
            auto poolU = GetPool<U>();
            auto poolV = GetPool<V>();
            if (!poolT || !poolU || !poolV) return;

            auto& entities = poolT->GetEntities();
            for (usize i = 0; i < entities.size(); ++i) {
                EntityID id = entities[i];
                U* u = poolU->GetComponent(id);
                V* v = poolV->GetComponent(id);
                if (u && v) {
                    fn(id, poolT->GetComponents()[i], *u, *v);
                }
            }
        }

        // ── Stats ─────────────────────────────────────────────
        u32 GetComponentPoolCount() const {
            return static_cast<u32>(pools_.size());
        }

    protected:
        // IECS internal virtuals
        void* AddComponentInternal(
            EntityID entity, std::type_index type,
            const void* data, usize size
        ) override;

        void RemoveComponentInternal(
            EntityID entity, std::type_index type
        ) override;

        void* GetComponentInternal(
            EntityID entity, std::type_index type
        ) const override;

        bool HasComponentInternal(
            EntityID entity, std::type_index type
        ) const override;

        void ForEachInternal(
            std::vector<std::type_index> types,
            std::function<void(EntityID,
                std::vector<void*>&)> fn
        ) override;

    private:
        template<typename T>
        ComponentPool<T>* GetOrCreatePool() {
            auto key = std::type_index(typeid(T));
            auto it  = pools_.find(key);
            if (it == pools_.end()) {
                pools_[key] = std::make_unique<ComponentPool<T>>();
                it = pools_.find(key);
            }
            return static_cast<ComponentPool<T>*>(it->second.get());
        }

        template<typename T>
        ComponentPool<T>* GetPool() {
            auto key = std::type_index(typeid(T));
            auto it  = pools_.find(key);
            if (it == pools_.end()) return nullptr;
            return static_cast<ComponentPool<T>*>(it->second.get());
        }

        template<typename T>
        const ComponentPool<T>* GetPoolConst() const {
            auto key = std::type_index(typeid(T));
            auto it  = pools_.find(key);
            if (it == pools_.end()) return nullptr;
            return static_cast<const ComponentPool<T>*>(
                it->second.get()
            );
        }

        // Entity management
        std::atomic<EntityID>     nextEntityID_{ 1 };
        std::vector<EntityID>     aliveEntities_;
        std::vector<EntityID>     freeList_;
        mutable std::mutex        entityMutex_;

        // Component pools - one per component type
        std::unordered_map<
            std::type_index,
            std::unique_ptr<IComponentPool>
        > pools_;

        // Systems - sorted by priority
        std::vector<SystemEntry>  systems_;
        mutable std::mutex        systemMutex_;
    };

} // namespace RiftCore

#pragma warning(pop)
