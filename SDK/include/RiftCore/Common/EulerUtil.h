#pragma once
// Euler <-> matrix <-> quaternion helpers that match the engine's
// transform convention (Math::TRSFull: R = Ry * Rx * Rz, degrees).
#include <RiftCore/Common/Types.h>
#include <cmath>

namespace RiftCore::EulerUtil {

    struct Mat3 { f32 m[3][3]; };   // m[row][col]

    inline Mat3 FromEulerDeg(const Vec3& e) {
        const f32 k = 3.14159265358979f / 180.0f;
        f32 a = e.x * k, b = -e.y * k, c = e.z * k;   // engine RotateY is Ry(-y)
        f32 ca = std::cos(a), sa = std::sin(a);
        f32 cb = std::cos(b), sb = std::sin(b);
        f32 cc = std::cos(c), sc = std::sin(c);
        Mat3 r;
        r.m[0][0] = cb*cc + sb*sa*sc;  r.m[0][1] = -cb*sc + sb*sa*cc; r.m[0][2] = sb*ca;
        r.m[1][0] = ca*sc;             r.m[1][1] = ca*cc;             r.m[1][2] = -sa;
        r.m[2][0] = -sb*cc + cb*sa*sc; r.m[2][1] = sb*sc + cb*sa*cc;  r.m[2][2] = cb*ca;
        return r;
    }

    inline Vec3 ToEulerDeg(const Mat3& r) {
        const f32 k = 180.0f / 3.14159265358979f;
        f32 s = -r.m[1][2];
        if (s > 1.0f)  s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        f32 a = std::asin(s), b, c;
        if (std::fabs(s) < 0.9999f) {
            b = std::atan2(r.m[0][2], r.m[2][2]);
            c = std::atan2(r.m[1][0], r.m[1][1]);
        } else {                       // gimbal lock: fold roll into yaw
            b = std::atan2(-r.m[2][0], r.m[0][0]);
            c = 0.0f;
        }
        return { a * k, -b * k, c * k };
    }

    inline Quat ToQuat(const Mat3& r) {
        Quat q;
        f32 tr = r.m[0][0] + r.m[1][1] + r.m[2][2];
        if (tr > 0.0f) {
            f32 s = std::sqrt(tr + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (r.m[2][1] - r.m[1][2]) / s;
            q.y = (r.m[0][2] - r.m[2][0]) / s;
            q.z = (r.m[1][0] - r.m[0][1]) / s;
        } else if (r.m[0][0] > r.m[1][1] && r.m[0][0] > r.m[2][2]) {
            f32 s = std::sqrt(1.0f + r.m[0][0] - r.m[1][1] - r.m[2][2]) * 2.0f;
            q.w = (r.m[2][1] - r.m[1][2]) / s;
            q.x = 0.25f * s;
            q.y = (r.m[0][1] + r.m[1][0]) / s;
            q.z = (r.m[0][2] + r.m[2][0]) / s;
        } else if (r.m[1][1] > r.m[2][2]) {
            f32 s = std::sqrt(1.0f + r.m[1][1] - r.m[0][0] - r.m[2][2]) * 2.0f;
            q.w = (r.m[0][2] - r.m[2][0]) / s;
            q.x = (r.m[0][1] + r.m[1][0]) / s;
            q.y = 0.25f * s;
            q.z = (r.m[1][2] + r.m[2][1]) / s;
        } else {
            f32 s = std::sqrt(1.0f + r.m[2][2] - r.m[0][0] - r.m[1][1]) * 2.0f;
            q.w = (r.m[1][0] - r.m[0][1]) / s;
            q.x = (r.m[0][2] + r.m[2][0]) / s;
            q.y = (r.m[1][2] + r.m[2][1]) / s;
            q.z = 0.25f * s;
        }
        return q;
    }

    inline Mat3 FromQuat(const Quat& q) {
        f32 x = q.x, y = q.y, z = q.z, w = q.w;
        Mat3 r;
        r.m[0][0] = 1 - 2*(y*y + z*z); r.m[0][1] = 2*(x*y - z*w);     r.m[0][2] = 2*(x*z + y*w);
        r.m[1][0] = 2*(x*y + z*w);     r.m[1][1] = 1 - 2*(x*x + z*z); r.m[1][2] = 2*(y*z - x*w);
        r.m[2][0] = 2*(x*z - y*w);     r.m[2][1] = 2*(y*z + x*w);     r.m[2][2] = 1 - 2*(x*x + y*y);
        return r;
    }

    inline Quat EulerDegToQuat(const Vec3& e) { return ToQuat(FromEulerDeg(e)); }
    inline Vec3 QuatToEulerDeg(const Quat& q) { return ToEulerDeg(FromQuat(q)); }

} // namespace RiftCore::EulerUtil
