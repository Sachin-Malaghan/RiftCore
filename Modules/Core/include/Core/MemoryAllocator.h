#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Core/IMemoryAllocator.h>

#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef CORE_EXPORTS
    #define CORE_API RIFTCORE_EXPORT
#else
    #define CORE_API RIFTCORE_IMPORT
#endif





namespace RiftCore {

    class CORE_API SystemAllocator : public IAllocator {
    public:
        SystemAllocator();
        ~SystemAllocator();
        void*  Allocate(usize size, usize alignment = 8) override;
        void*  Reallocate(void* ptr, usize oldSize, usize newSize) override;
        void   Free(void* ptr) override;
        usize  GetTotalAllocated()  const { return totalAllocated_;  }
        u32    GetAllocationCount() const { return allocationCount_; }
    private:
        usize              totalAllocated_  = 0;
        u32                allocationCount_ = 0;
        mutable std::mutex mutex_;
    };

    class CORE_API LinearAllocator : public IAllocator {
    public:
        explicit LinearAllocator(usize capacityBytes);
        ~LinearAllocator();
        void*  Allocate(usize size, usize alignment = 8) override;
        void*  Reallocate(void* ptr, usize oldSize, usize newSize) override;
        void   Free(void* ptr) override;
        void   Reset() override;
        usize  GetUsed()     const { return offset_;   }
        usize  GetCapacity() const { return capacity_; }
    private:
        byte*              memory_   = nullptr;
        usize              capacity_ = 0;
        usize              offset_   = 0;
        mutable std::mutex mutex_;
    };

    class CORE_API PoolAllocator : public IAllocator {
    public:
        PoolAllocator(usize blockSize, usize blockCount);
        ~PoolAllocator();
        void*  Allocate(usize size, usize alignment = 8) override;
        void*  Reallocate(void* ptr, usize oldSize, usize newSize) override;
        void   Free(void* ptr) override;
        void   Reset() override;
        usize  GetBlockSize()  const { return blockSize_;  }
        usize  GetBlockCount() const { return blockCount_; }
        usize  GetFreeCount()  const { return freeCount_;  }
    private:
        byte*              memory_     = nullptr;
        usize              blockSize_  = 0;
        usize              blockCount_ = 0;
        usize              freeCount_  = 0;
        std::vector<void*> freeList_;
        mutable std::mutex mutex_;
    };

    class CORE_API EngineMemoryAllocator : public IMemoryAllocator {
    public:
        EngineMemoryAllocator();
        ~EngineMemoryAllocator();
        IAllocator* GetSystemAllocator() override;
        IAllocator* GetFrameAllocator()  override;
        IAllocator* GetPoolAllocator(
            const String& name,
            usize blockSize,
            usize blockCount
        ) override;
        void        BeginFrame() override;
        void        EndFrame()   override;
        MemoryStats GetStats()   const override;
        void        DumpStats()  override;
    private:
        std::unique_ptr<SystemAllocator> systemAllocator_;
        std::unique_ptr<LinearAllocator> frameAllocator_;
        std::unordered_map<String, std::unique_ptr<PoolAllocator>> poolAllocators_;
        mutable std::mutex mutex_;
        u64 frameIndex_ = 0;
    };

} // namespace RiftCore

#pragma warning(pop)
