#pragma once

//implement complete file 
namespace RiftCore {
    class IRHI {
    public:
        virtual ~IRHI() = default;
        virtual bool Initialize() = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
    };
}
