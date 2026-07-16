#include <Core/MemoryAllocator.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <iostream>
#include <sstream>






namespace RiftCore {

    // Align a pointer up to the given alignment
    static usize AlignUp(usize value, usize alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // SystemAllocator
    SystemAllocator::SystemAllocator()  = default;
    SystemAllocator::~SystemAllocator() = default;

    void* SystemAllocator::Allocate(usize size, usize alignment) {
        if (size == 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);

#ifdef RIFTCORE_PLATFORM_WINDOWS
        void* ptr = _aligned_malloc(size, alignment);
#else
        void* ptr = nullptr;
        posix_memalign(&ptr, alignment, size);
#endif
        if (ptr) {
            totalAllocated_  += size;
            allocationCount_ += 1;
        }
        return ptr;
    }

    void* SystemAllocator::Reallocate(void* ptr,
                                       usize oldSize,
                                       usize newSize) {
        if (!ptr) return Allocate(newSize);
        std::lock_guard<std::mutex> lock(mutex_);
#ifdef RIFTCORE_PLATFORM_WINDOWS
        void* newPtr = _aligned_realloc(ptr, newSize, 8);
#else
        void* newPtr = realloc(ptr, newSize);
#endif
        if (newPtr) {
            totalAllocated_ += (newSize - oldSize);
        }
        return newPtr;
    }

    void SystemAllocator::Free(void* ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mutex_);
#ifdef RIFTCORE_PLATFORM_WINDOWS
        _aligned_free(ptr);
#else
        free(ptr);
#endif
        allocationCount_ = allocationCount_ > 0
                         ? allocationCount_ - 1 : 0;
    }

    // LinearAllocator
    LinearAllocator::LinearAllocator(usize capacityBytes)
        : capacity_(capacityBytes)
        , offset_(0) {
        memory_ = static_cast<byte*>(
            std::malloc(capacityBytes)
        );
        assert(memory_ && "LinearAllocator: malloc failed");
        std::memset(memory_, 0, capacityBytes);
    }

    LinearAllocator::~LinearAllocator() {
        std::free(memory_);
        memory_ = nullptr;
    }

    void* LinearAllocator::Allocate(usize size, usize alignment) {
        std::lock_guard<std::mutex> lock(mutex_);
        usize aligned = AlignUp(offset_, alignment);
        if (aligned + size > capacity_) {
            assert(false && "LinearAllocator: out of memory");
            return nullptr;
        }
        void* ptr = memory_ + aligned;
        offset_   = aligned + size;
        return ptr;
    }

    void* LinearAllocator::Reallocate(void*, usize, usize newSize) {
        return Allocate(newSize);
    }

    void LinearAllocator::Free(void*) {
        // No individual free in linear allocator
    }

    void LinearAllocator::Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        offset_ = 0;
    }

    // PoolAllocator
    PoolAllocator::PoolAllocator(usize blockSize, usize blockCount)
        : blockSize_(blockSize < sizeof(void*) ? sizeof(void*) : blockSize)
        , blockCount_(blockCount)
        , freeCount_(blockCount) {
        memory_ = static_cast<byte*>(
            std::malloc(blockSize_ * blockCount_)
        );
        assert(memory_ && "PoolAllocator: malloc failed");

        freeList_.reserve(blockCount_);
        for (usize i = 0; i < blockCount_; ++i) {
            freeList_.push_back(memory_ + i * blockSize_);
        }
    }

    PoolAllocator::~PoolAllocator() {
        std::free(memory_);
        memory_ = nullptr;
    }

    void* PoolAllocator::Allocate(usize size, usize) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (freeList_.empty()) {
            assert(false && "PoolAllocator: pool exhausted");
            return nullptr;
        }
        RIFTCORE_UNUSED(size);
        void* ptr = freeList_.back();
        freeList_.pop_back();
        freeCount_--;
        return ptr;
    }

    void* PoolAllocator::Reallocate(void* ptr, usize, usize) {
        return ptr;
    }

    void PoolAllocator::Free(void* ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mutex_);
        freeList_.push_back(ptr);
        freeCount_++;
    }

    void PoolAllocator::Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        freeList_.clear();
        for (usize i = 0; i < blockCount_; ++i) {
            freeList_.push_back(memory_ + i * blockSize_);
        }
        freeCount_ = blockCount_;
    }

    // EngineMemoryAllocator
    EngineMemoryAllocator::EngineMemoryAllocator() {
        systemAllocator_ = std::make_unique<SystemAllocator>();
        // 64MB frame allocator
        frameAllocator_  = std::make_unique<LinearAllocator>(
            64 * 1024 * 1024
        );
    }

    EngineMemoryAllocator::~EngineMemoryAllocator() = default;

    IAllocator* EngineMemoryAllocator::GetSystemAllocator() {
        return systemAllocator_.get();
    }

    IAllocator* EngineMemoryAllocator::GetFrameAllocator() {
        return frameAllocator_.get();
    }

    IAllocator* EngineMemoryAllocator::GetPoolAllocator(
        const String& name,
        usize blockSize,
        usize blockCount
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = poolAllocators_.find(name);
        if (it != poolAllocators_.end()) {
            return it->second.get();
        }
        auto pool = std::make_unique<PoolAllocator>(
            blockSize, blockCount
        );
        auto* ptr = pool.get();
        poolAllocators_[name] = std::move(pool);
        return ptr;
    }

    void EngineMemoryAllocator::BeginFrame() {
        frameIndex_++;
        frameAllocator_->Reset();
    }

    void EngineMemoryAllocator::EndFrame() {}

    MemoryStats EngineMemoryAllocator::GetStats() const {
        MemoryStats stats;
        stats.allocationCount = systemAllocator_->GetAllocationCount();
        stats.totalAllocated  = systemAllocator_->GetTotalAllocated();
        return stats;
    }

    void EngineMemoryAllocator::DumpStats() {
        auto stats = GetStats();
        std::cout << "[Memory] Allocations: "
                  << stats.allocationCount
                  << " | Total: "
                  << (stats.totalAllocated / 1024)
                  << " KB\n";
    }

} // namespace RiftCore
