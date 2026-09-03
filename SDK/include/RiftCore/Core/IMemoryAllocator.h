// ============================================================
// IMemoryAllocator.h
// Memory allocation interface.
// Supports multiple allocator strategies:
//   - System (malloc/free wrapper)
//   - Linear/Frame (fast, no individual free, reset per frame)
//   - Pool (fixed-size blocks, O(1) alloc/free)
// ============================================================
#pragma once

#include "../Common/Platform.h"
#include "../Common/Types.h"










namespace RiftCore {

    // ── Allocation info — useful for debugging ─────────────────
    struct AllocationInfo {
        void*       ptr       = nullptr;
        usize       size      = 0;
        const char* file      = nullptr;
        int         line      = 0;
        const char* tag       = nullptr;  // "Renderer", "Physics", etc.
    };

    // ── Memory stats ──────────────────────────────────────────
    struct MemoryStats {
        usize totalAllocated   = 0;  // total bytes ever allocated
        usize totalFreed       = 0;  // total bytes ever freed
        usize currentUsage     = 0;  // currently live bytes
        usize peakUsage        = 0;  // maximum seen at any time
        usize allocationCount  = 0;  // number of active allocations
    };

    // ── IAllocator — base allocator interface ─────────────────
    class IAllocator {
    public:
        virtual ~IAllocator() = default;

        virtual void*  Allocate(usize size, usize alignment = 8)              = 0;
        virtual void*  Reallocate(void* ptr, usize oldSize, usize newSize)    = 0;
        virtual void   Free(void* ptr)                                         = 0;

        // For linear/frame allocators only
        virtual void   Reset() {}

        // Typed helper
        template<typename T, typename... Args>
        T* New(Args&&... args) {
            void* mem = Allocate(sizeof(T), alignof(T));
            return new(mem) T(std::forward<Args>(args)...);
        }

        template<typename T>
        void Delete(T* ptr) {
            if (ptr) {
                ptr->~T();
                Free(ptr);
            }
        }
    };

    // ── IMemoryAllocator — engine-level memory manager ────────
    class IMemoryAllocator {
    public:
        virtual ~IMemoryAllocator() = default;

        // ── Allocator access ──────────────────────────────────
        // Get the default system allocator
        virtual IAllocator* GetSystemAllocator()   = 0;

        // Get a frame/linear allocator — resets every frame
        // Use for temporary per-frame allocations
        virtual IAllocator* GetFrameAllocator()    = 0;

        // Get or create a named pool allocator
        // blockSize = size of each element, blockCount = pool capacity
        virtual IAllocator* GetPoolAllocator(
            const String& name,
            usize blockSize,
            usize blockCount
        ) = 0;

        // ── Frame boundary ────────────────────────────────────
        // Call at start of each frame to reset frame allocator
        virtual void BeginFrame() = 0;
        virtual void EndFrame()   = 0;

        // ── Stats ─────────────────────────────────────────────
        virtual MemoryStats GetStats() const = 0;
        virtual void        DumpStats()      = 0;
    };

} // namespace RiftCore
