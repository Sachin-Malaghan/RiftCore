#include <GizmoSystem.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <cmath>
#include <algorithm>
#include <cstring>


namespace RiftCore {

    void GizmoSystem::SetViewport(
        f32 x, f32 y, f32 w, f32 h
    ) {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(x, y, w, h);
    }

    GizmoResult GizmoSystem::Draw(
        Mat4&       objectMatrix,
        const Mat4& viewMatrix,
        const Mat4& projMatrix
    ) {
        GizmoResult result;

        // Map operation
        ImGuizmo::OPERATION op;
        switch (operation_) {
            case GizmoOperation::Rotate:
                op = ImGuizmo::ROTATE; break;
            case GizmoOperation::Scale:
                op = ImGuizmo::SCALE;  break;
            default:
                op = ImGuizmo::TRANSLATE; break;
        }

        ImGuizmo::MODE mode =
            (space_ == GizmoSpace::Local)
            ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        const f32* viewPtr = &viewMatrix.cols[0][0];
        const f32* projPtr = &projMatrix.cols[0][0];
        f32*       matPtr  = &objectMatrix.cols[0][0];

        f32 snap[3] = {
            snapValue_, snapValue_, snapValue_
        };

        bool changed = ImGuizmo::Manipulate(
            viewPtr,
            projPtr,
            op,
            mode,
            matPtr,
            nullptr,
            snapEnabled_ ? snap : nullptr
        );

        isOver_  = ImGuizmo::IsOver();
        isUsing_ = ImGuizmo::IsUsing();

        if (changed) {
            result.changed = true;
            DecomposeMatrix(objectMatrix,
                result.position,
                result.rotation,
                result.scale);
        }

        return result;
    }

    bool GizmoSystem::RaycastAABB(
        const Vec3& o, const Vec3& d,
        const Vec3& bMin, const Vec3& bMax,
        f32& outT
    ) {
        f32 tmin = -1e9f, tmax = 1e9f;

        auto testAxis = [&](f32 orig, f32 dir,
                            f32 lo, f32 hi) -> bool {
            if (std::abs(dir) > 1e-6f) {
                f32 t1 = (lo - orig) / dir;
                f32 t2 = (hi - orig) / dir;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
            } else {
                if (orig < lo || orig > hi) return false;
            }
            return true;
        };

        if (!testAxis(o.x, d.x, bMin.x, bMax.x))
            return false;
        if (!testAxis(o.y, d.y, bMin.y, bMax.y))
            return false;
        if (!testAxis(o.z, d.z, bMin.z, bMax.z))
            return false;

        if (tmax < 0 || tmin > tmax) return false;
        outT = tmin > 0 ? tmin : tmax;
        return outT >= 0;
    }

    void GizmoSystem::ScreenToRay(
        f32 mouseX, f32 mouseY,
        f32 vpX,    f32 vpY,
        f32 vpW,    f32 vpH,
        const Mat4& view,
        const Mat4& proj,
        Vec3& outOrigin,
        Vec3& outDir
    ) {
        f32 ndcX = ((mouseX-vpX)/vpW)*2.0f - 1.0f;
        f32 ndcY = 1.0f - ((mouseY-vpY)/vpH)*2.0f;

        const f32* v = &view.cols[0][0];
        outOrigin = {
            -(v[0]*v[12] + v[1]*v[13] + v[2]*v[14]),
            -(v[4]*v[12] + v[5]*v[13] + v[6]*v[14]),
            -(v[8]*v[12] + v[9]*v[13] + v[10]*v[14])
        };

        const f32* p = &proj.cols[0][0];
        f32 rx =  ndcX / p[0];
        f32 ry =  ndcY / p[5];
        f32 rz = -1.0f;

        outDir = {
            rx*v[0] + ry*v[4] + rz*v[8],
            rx*v[1] + ry*v[5] + rz*v[9],
            rx*v[2] + ry*v[6] + rz*v[10]
        };

        f32 len = std::sqrt(
            outDir.x*outDir.x +
            outDir.y*outDir.y +
            outDir.z*outDir.z);
        if (len > 0.0001f) {
            outDir.x /= len;
            outDir.y /= len;
            outDir.z /= len;
        }
    }

    void GizmoSystem::DecomposeMatrix(
        const Mat4& matrix,
        Vec3& outPos, Vec3& outRot, Vec3& outScale
    ) {
        f32 mat[16];
        std::memcpy(mat, &matrix.cols[0][0],
                    16*sizeof(f32));
        ImGuizmo::DecomposeMatrixToComponents(
            mat,
            &outPos.x,
            &outRot.x,
            &outScale.x);
    }

    Mat4 GizmoSystem::ComposeMatrix(
        const Vec3& pos,
        const Vec3& rot,
        const Vec3& scale
    ) {
        f32 mat[16];
        ImGuizmo::RecomposeMatrixFromComponents(
            &pos.x, &rot.x, &scale.x, mat);
        Mat4 result;
        std::memcpy(&result.cols[0][0], mat,
                    16*sizeof(f32));
        return result;
    }

} // namespace RiftCore
