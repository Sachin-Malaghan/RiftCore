#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <cmath>

#ifdef RENDERER_EXPORTS
    #define RENDERER_API RIFTCORE_EXPORT
#else
    #define RENDERER_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    namespace Math {

        // ── Constants ─────────────────────────────────────
        constexpr f32 PI = 3.14159265358979f;

        inline f32 ToRadians(f32 degrees) {
            return degrees * PI / 180.0f;
        }

        inline f32 ToDegrees(f32 radians) {
            return radians * 180.0f / PI;
        }

        // ── Vector helpers ────────────────────────────────
        inline Vec3 Normalize(const Vec3& v) {
            f32 len = std::sqrt(
                v.x*v.x + v.y*v.y + v.z*v.z);
            if (len < 0.0001f) return {0, 0, 1};
            return {v.x/len, v.y/len, v.z/len};
        }

        inline Vec3 Cross(const Vec3& a, const Vec3& b) {
            return {
                a.y*b.z - a.z*b.y,
                a.z*b.x - a.x*b.z,
                a.x*b.y - a.y*b.x
            };
        }

        inline f32 Dot(const Vec3& a, const Vec3& b) {
            return a.x*b.x + a.y*b.y + a.z*b.z;
        }

        inline f32 Clamp(f32 v, f32 lo, f32 hi) {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        // ── Basic transform matrices ──────────────────────
        // IMPORTANT: Multiply must be defined BEFORE
        // any function that calls it (TRS, TRSFull, etc.)

        inline Mat4 Translate(const Vec3& t) {
            Mat4 m = Mat4::Identity();
            m.cols[3][0] = t.x;
            m.cols[3][1] = t.y;
            m.cols[3][2] = t.z;
            return m;
        }

        inline Mat4 Scale(const Vec3& s) {
            Mat4 m = Mat4::Identity();
            m.cols[0][0] = s.x;
            m.cols[1][1] = s.y;
            m.cols[2][2] = s.z;
            return m;
        }

        inline Mat4 ScaleUniform(f32 s) {
            return Scale({s, s, s});
        }

        inline Mat4 RotateX(f32 degrees) {
            f32 r = ToRadians(degrees);
            f32 c = std::cos(r);
            f32 s = std::sin(r);
            Mat4 m = Mat4::Identity();
            m.cols[1][1] =  c;
            m.cols[1][2] =  s;
            m.cols[2][1] = -s;
            m.cols[2][2] =  c;
            return m;
        }

        inline Mat4 RotateY(f32 degrees) {
            f32 r = ToRadians(degrees);
            f32 c = std::cos(r);
            f32 s = std::sin(r);
            Mat4 m = Mat4::Identity();
            m.cols[0][0] =  c;
            m.cols[0][2] =  s;
            m.cols[2][0] = -s;
            m.cols[2][2] =  c;
            return m;
        }

        inline Mat4 RotateZ(f32 degrees) {
            f32 r = ToRadians(degrees);
            f32 c = std::cos(r);
            f32 s = std::sin(r);
            Mat4 m = Mat4::Identity();
            m.cols[0][0] =  c;
            m.cols[0][1] =  s;
            m.cols[1][0] = -s;
            m.cols[1][1] =  c;
            return m;
        }

        // ── Multiply — MUST be before TRS/TRSFull ─────────
        inline Mat4 Multiply(const Mat4& a, const Mat4& b) {
            Mat4 result{};
            for (int col = 0; col < 4; col++) {
                for (int row = 0; row < 4; row++) {
                    f32 sum = 0.0f;
                    for (int k = 0; k < 4; k++) {
                        sum += a.cols[k][row] *
                               b.cols[col][k];
                    }
                    result.cols[col][row] = sum;
                }
            }
            return result;
        }

        // ── View + Projection matrices ────────────────────
        inline Mat4 LookAt(
            const Vec3& eye,
            const Vec3& target,
            const Vec3& up
        ) {
            Vec3 f = Normalize({
                target.x-eye.x,
                target.y-eye.y,
                target.z-eye.z
            });
            Vec3 r = Normalize(Cross(f, up));
            Vec3 u = Cross(r, f);

            Mat4 m = Mat4::Identity();
            m.cols[0][0] =  r.x;
            m.cols[1][0] =  r.y;
            m.cols[2][0] =  r.z;
            m.cols[0][1] =  u.x;
            m.cols[1][1] =  u.y;
            m.cols[2][1] =  u.z;
            m.cols[0][2] = -f.x;
            m.cols[1][2] = -f.y;
            m.cols[2][2] = -f.z;
            m.cols[3][0] = -Dot(r, eye);
            m.cols[3][1] = -Dot(u, eye);
            m.cols[3][2] =  Dot(f, eye);
            m.cols[3][3] =  1.0f;
            return m;
        }

        inline Mat4 Perspective(
            f32 fovYDegrees,
            f32 aspect,
            f32 nearZ,
            f32 farZ
        ) {
            f32 tanHalf = std::tan(
                ToRadians(fovYDegrees) * 0.5f);
            Mat4 m{};
            m.cols[0][0] = 1.0f / (aspect * tanHalf);
            m.cols[1][1] = 1.0f / tanHalf;
            m.cols[2][2] = -(farZ + nearZ) /
                            (farZ - nearZ);
            m.cols[2][3] = -1.0f;
            m.cols[3][2] = -(2.0f * farZ * nearZ) /
                            (farZ - nearZ);
            return m;
        }

        // ── TRS helpers — AFTER Multiply ─────────────────
        // Standard game engine transform order:
        // Scale first → Rotate → Translate (TRS)

        // Simple TRS with Y rotation only
        inline Mat4 TRS(
            const Vec3& position,
            f32         rotationYDegrees,
            const Vec3& scaleVec
        ) {
            Mat4 T = Translate(position);
            Mat4 R = RotateY(rotationYDegrees);
            Mat4 S = Scale(scaleVec);
            return Multiply(Multiply(T, R), S);
        }

        // Full TRS with X, Y, Z rotation
        inline Mat4 TRSFull(
            const Vec3& position,
            f32         rotXDegrees,
            f32         rotYDegrees,
            f32         rotZDegrees,
            const Vec3& scaleVec
        ) {
            Mat4 T  = Translate(position);
            Mat4 Rx = RotateX(rotXDegrees);
            Mat4 Ry = RotateY(rotYDegrees);
            Mat4 Rz = RotateZ(rotZDegrees);
            Mat4 S  = Scale(scaleVec);
            // Combined rotation: Ry * Rx * Rz
            Mat4 R = Multiply(Multiply(Ry, Rx), Rz);
            return Multiply(Multiply(T, R), S);
        }

    } // namespace Math

    // ── Camera class ──────────────────────────────────────────
    class RENDERER_API Camera {
    public:
        Camera();

        void SetPosition   (const Vec3& pos);
        void SetTarget     (const Vec3& target);
        void SetUp         (const Vec3& up);
        void SetFOV        (f32 fovDegrees);
        void SetAspectRatio(f32 aspect);
        void SetClipPlanes (f32 nearZ, f32 farZ);

        void MoveForward(f32 amount);
        void MoveRight  (f32 amount);
        void MoveUp     (f32 amount);
        void RotateYaw  (f32 degrees);
        void RotatePitch(f32 degrees);

        const Mat4& GetViewMatrix()       const { return view_;       }
        const Mat4& GetProjectionMatrix() const { return projection_; }
        Mat4        GetViewProjection()   const {
            return Math::Multiply(projection_, view_);
        }

        Vec3 GetPosition() const { return position_; }
        Vec3 GetForward()  const { return forward_;  }
        Vec3 GetRight()    const { return right_;    }
        Vec3 GetUp()       const { return up_;       }

        void Update();

    private:
        void RecalculateVectors();

        Vec3 position_ = { 0.0f, 1.0f,  5.0f };
        Vec3 target_   = { 0.0f, 0.0f,  0.0f };
        Vec3 worldUp_  = { 0.0f, 1.0f,  0.0f };
        Vec3 forward_  = { 0.0f, 0.0f, -1.0f };
        Vec3 right_    = { 1.0f, 0.0f,  0.0f };
        Vec3 up_       = { 0.0f, 1.0f,  0.0f };

        f32  yaw_      = -90.0f;
        f32  pitch_    =   0.0f;
        f32  fov_      =  60.0f;
        f32  aspect_   =  16.0f / 9.0f;
        f32  nearZ_    =   0.1f;
        f32  farZ_     = 100.0f;

        Mat4 view_       = Mat4::Identity();
        Mat4 projection_ = Mat4::Identity();
    };

} // namespace RiftCore

#pragma warning(pop)
