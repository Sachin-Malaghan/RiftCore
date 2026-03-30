#include <ECS/World.h>
#include <algorithm>
#include <cassert>

namespace RiftCore {

    World::World()  = default;
    World::~World() = default;

    // ── Entity Management ─────────────────────────────────────

    EntityID World::CreateEntity() {
        std::lock_guard<std::mutex> lock(entityMutex_);

        EntityID id;
        if (!freeList_.empty()) {
            id = freeList_.back();
            freeList_.pop_back();
        } else {
            id = nextEntityID_.fetch_add(1);
        }

        aliveEntities_.push_back(id);
        return id;
    }

    void World::DestroyEntity(EntityID entity) {
        std::lock_guard<std::mutex> lock(entityMutex_);

        // Remove all components for this entity
        for (auto& [type, pool] : pools_) {
            if (pool->HasComponent(entity)) {
                pool->RemoveComponent(entity);
            }
        }

        // Remove from alive list
        auto it = std::find(
            aliveEntities_.begin(),
            aliveEntities_.end(),
            entity
        );
        if (it != aliveEntities_.end()) {
            aliveEntities_.erase(it);
        }

        // Add to free list for reuse
        freeList_.push_back(entity);
    }

    bool World::IsAlive(EntityID entity) const {
        std::lock_guard<std::mutex> lock(entityMutex_);
        return std::find(
            aliveEntities_.begin(),
            aliveEntities_.end(),
            entity
        ) != aliveEntities_.end();
    }

    u32 World::GetEntityCount() const {
        std::lock_guard<std::mutex> lock(entityMutex_);
        return static_cast<u32>(aliveEntities_.size());
    }

    // ── System Management ─────────────────────────────────────

    void World::RegisterSystem(
        const String&  name,
        SystemFunction fn,
        i32            priority
    ) {
        std::lock_guard<std::mutex> lock(systemMutex_);
        SystemEntry entry;
        entry.name     = name;
        entry.function = std::move(fn);
        entry.priority = priority;
        entry.enabled  = true;
        systems_.push_back(std::move(entry));

        // Sort by priority descending
        std::sort(systems_.begin(), systems_.end(),
            [](const SystemEntry& a, const SystemEntry& b) {
                return a.priority > b.priority;
            }
        );
    }

    void World::UnregisterSystem(const String& name) {
        std::lock_guard<std::mutex> lock(systemMutex_);
        systems_.erase(
            std::remove_if(systems_.begin(), systems_.end(),
                [&name](const SystemEntry& e) {
                    return e.name == name;
                }
            ),
            systems_.end()
        );
    }

    void World::UpdateSystems(f32 deltaTime) {
        std::lock_guard<std::mutex> lock(systemMutex_);
        for (auto& system : systems_) {
            if (system.enabled && system.function) {
                system.function(deltaTime);
            }
        }
    }

    // ── IECS Internal Virtuals ────────────────────────────────

    void* World::AddComponentInternal(
        EntityID        entity,
        std::type_index type,
        const void*     data,
        usize           size
    ) {
        RIFTCORE_UNUSED(size);
        auto it = pools_.find(type);
        if (it == pools_.end()) return nullptr;
        RIFTCORE_UNUSED(data);
        return it->second->GetRaw(entity);
    }

    void World::RemoveComponentInternal(
        EntityID        entity,
        std::type_index type
    ) {
        auto it = pools_.find(type);
        if (it != pools_.end()) {
            it->second->RemoveComponent(entity);
        }
    }

    void* World::GetComponentInternal(
        EntityID        entity,
        std::type_index type
    ) const {
        auto it = pools_.find(type);
        if (it == pools_.end()) return nullptr;
        return it->second->GetRaw(entity);
    }

    bool World::HasComponentInternal(
        EntityID        entity,
        std::type_index type
    ) const {
        auto it = pools_.find(type);
        if (it == pools_.end()) return false;
        return it->second->HasComponent(entity);
    }

    void World::ForEachInternal(
        std::vector<std::type_index> types,
        std::function<void(EntityID, std::vector<void*>&)> fn
    ) {
        if (types.empty()) return;

        // Find smallest pool to iterate
        IComponentPool* smallest = nullptr;
        for (auto& type : types) {
            auto it = pools_.find(type);
            if (it == pools_.end()) return;
            if (!smallest ||
                it->second->GetCount() < smallest->GetCount()) {
                smallest = it->second.get();
            }
        }

        if (!smallest) return;

        // Iterate smallest pool
        // For each entity check if it has ALL other components
        std::vector<void*> ptrs(types.size());

        for (auto& type : types) {
            auto it = pools_.find(type);
            if (it == pools_.end()) return;
        }

        RIFTCORE_UNUSED(fn);
    }

} // namespace RiftCore
