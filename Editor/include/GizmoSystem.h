#pragma once
#include <RiftCore/Common/Types.h>
#include <cstring>

namespace RiftCore {

    enum class GizmoOperation {
        Translate = 0,
        Rotate    = 1,
        Scale     = 2
    };

    enum class GizmoSpace {
        World = 0,
        Local = 1
    };

    struct GizmoResult {
        bool changed  = false;
        Vec3 position = Vec3::Zero();
        Vec3 rotation = Vec3::Zero();
        Vec3 scale    = Vec3::One();
    };

    class GizmoSystem {
    public:
        GizmoSystem()  = default;
        ~GizmoSystem() = default;

        // Setup viewport rect for ImGuizmo
        void SetViewport(f32 x, f32 y, f32 w, f32 h);

        // Draw gizmo for selected object
        // Returns result with changed=true if dragged
        GizmoResult Draw(
            Mat4&       objectMatrix,
            const Mat4& viewMatrix,
            const Mat4& projMatrix
        );

        // Settings
        void SetOperation(GizmoOperation op) {
            operation_ = op;
        }
        void SetSpace(GizmoSpace space) {
            space_ = space;
        }
        void SetSnapEnabled(bool enabled) {
            snapEnabled_ = enabled;
        }
        void SetSnapValue(f32 value) {
            snapValue_ = value;
        }

        GizmoOperation GetOperation() const {
            return operation_;
        }
        GizmoSpace GetSpace() const {
            return space_;
        }
        bool IsOver()  const { return isOver_;  }
        bool IsUsing() const { return isUsing_; }

        // Raycast helper
        static bool RaycastAABB(
            const Vec3& origin, const Vec3& dir,
            const Vec3& boxMin, const Vec3& boxMax,
            f32& outT
        );

        // Screen to world ray
        static void ScreenToRay(
            f32 mouseX,  f32 mouseY,
            f32 vpX,     f32 vpY,
            f32 vpW,     f32 vpH,
            const Mat4& view, const Mat4& proj,
            Vec3& outOrigin, Vec3& outDir
        );

        // Matrix decompose/compose
        static void DecomposeMatrix(
            const Mat4& matrix,
            Vec3& outTranslation,
            Vec3& outRotation,
            Vec3& outScale
        );

        static Mat4 ComposeMatrix(
            const Vec3& translation,
            const Vec3& rotation,
            const Vec3& scale
        );

    private:
        GizmoOperation operation_   = GizmoOperation::Translate;
        GizmoSpace     space_       = GizmoSpace::World;
        bool           snapEnabled_ = false;
        f32            snapValue_   = 0.5f;
        bool           isOver_      = false;
        bool           isUsing_     = false;
    };

} // namespace RiftCore
