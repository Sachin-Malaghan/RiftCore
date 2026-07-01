#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/ECS/IECS.h>

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cassert>

#ifdef ECS_EXPORTS
    #define ECS_API RIFTCORE_EXPORT
#else
    #define ECS_API RIFTCORE_IMPORT
#endif




namespace RiftCore {

    // Base class for type-erased component pool
    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;
        virtual void  RemoveComponent(EntityID entity)   = 0;
        virtual bool  HasComponent(EntityID entity) const = 0;
        virtual void* GetRaw(EntityID entity)             = 0;
        virtual usize GetCount() const                    = 0;
        virtual std::type_index GetType() const           = 0;
    };

    // Typed component pool
    // Stores components in dense contiguous array
    // Maps EntityID to array index for O(1) access
    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        ComponentPool() = default;

        // Add component to entity
        // Returns pointer to stored component
        T* AddComponent(EntityID entity, T component) {
            assert(!HasComponent(entity) &&
                   "Entity already has this component");

            u32 index = static_cast<u32>(components_.size());
            components_.push_back(std::move(component));
            entityToIndex_[entity] = index;
            indexToEntity_.push_back(entity);

            return &components_.back();
        }

        // Remove component from entity
        // Uses swap-with-last for O(1) removal
        void RemoveComponent(EntityID entity) override {
            auto it = entityToIndex_.find(entity);
            if (it == entityToIndex_.end()) return;

            u32 removeIdx = it->second;
            u32 lastIdx   = static_cast<u32>(
                components_.size() - 1
            );

            if (removeIdx != lastIdx) {
                // Swap removed with last element
                components_[removeIdx]  = std::move(
                    components_[lastIdx]
                );

                EntityID lastEntity        = indexToEntity_[lastIdx];
                entityToIndex_[lastEntity] = removeIdx;
                indexToEntity_[removeIdx]  = lastEntity;
            }

            components_.pop_back();
            indexToEntity_.pop_back();
            entityToIndex_.erase(entity);
        }

        bool HasComponent(EntityID entity) const override {
            return entityToIndex_.count(entity) > 0;
        }

        T* GetComponent(EntityID entity) {
            auto it = entityToIndex_.find(entity);
            if (it == entityToIndex_.end()) return nullptr;
            return &components_[it->second];
        }

        void* GetRaw(EntityID entity) override {
            return static_cast<void*>(GetComponent(entity));
        }

        usize GetCount() const override {
            return components_.size();
        }

        std::type_index GetType() const override {
            return std::type_index(typeid(T));
        }

        // Direct access to dense array
        // Use for cache-friendly iteration
        std::vector<T>& GetComponents() {
            return components_;
        }

        std::vector<EntityID>& GetEntities() {
            return indexToEntity_;
        }

    private:
        std::vector<T>                        components_;
        std::vector<EntityID>                 indexToEntity_;
        std::unordered_map<EntityID, u32>     entityToIndex_;
    };

} // namespace RiftCore

#pragma warning(pop)
