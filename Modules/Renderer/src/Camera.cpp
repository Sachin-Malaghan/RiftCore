#include <Renderer/Camera.h>
#include <cmath>








namespace RiftCore {

    Camera::Camera() {
        RecalculateVectors();
        Update();
    }

    void Camera::SetPosition(const Vec3& pos) {
        position_ = pos;
        Update();
    }

    void Camera::SetTarget(const Vec3& target) {
        target_ = target;
        Vec3 dir = {
            target_.x - position_.x,
            target_.y - position_.y,
            target_.z - position_.z
        };
        dir = Math::Normalize(dir);
        pitch_ = Math::ToRadians(
            std::asin(dir.y) * 180.0f / 3.14159f);
        yaw_   = std::atan2(dir.z, dir.x) *
                 180.0f / 3.14159f;
        RecalculateVectors();
        Update();
    }

    void Camera::SetUp(const Vec3& up) {
        worldUp_ = up;
        Update();
    }

    void Camera::SetFOV(f32 fov) {
        fov_ = fov;
        Update();
    }

    void Camera::SetAspectRatio(f32 aspect) {
        aspect_ = aspect;
        Update();
    }

    void Camera::SetClipPlanes(f32 nearZ, f32 farZ) {
        nearZ_ = nearZ;
        farZ_  = farZ;
        Update();
    }

    void Camera::MoveForward(f32 amount) {
        position_.x += forward_.x * amount;
        position_.y += forward_.y * amount;
        position_.z += forward_.z * amount;
        Update();
    }

    void Camera::MoveRight(f32 amount) {
        position_.x += right_.x * amount;
        position_.y += right_.y * amount;
        position_.z += right_.z * amount;
        Update();
    }

    void Camera::MoveUp(f32 amount) {
        position_.y += amount;
        Update();
    }

    void Camera::RotateYaw(f32 degrees) {
        yaw_ += degrees;
        RecalculateVectors();
        Update();
    }

    void Camera::RotatePitch(f32 degrees) {
        pitch_ += degrees;
        // Clamp pitch to avoid gimbal lock
        if (pitch_ >  89.0f) pitch_ =  89.0f;
        if (pitch_ < -89.0f) pitch_ = -89.0f;
        RecalculateVectors();
        Update();
    }

    void Camera::RecalculateVectors() {
        f32 yawR   = Math::ToRadians(yaw_);
        f32 pitchR = Math::ToRadians(pitch_);

        forward_ = Math::Normalize({
            std::cos(pitchR) * std::cos(yawR),
            std::sin(pitchR),
            std::cos(pitchR) * std::sin(yawR)
        });

        right_ = Math::Normalize(
            Math::Cross(forward_, worldUp_));
        up_    = Math::Normalize(
            Math::Cross(right_, forward_));
    }

    void Camera::Update() {
        Vec3 lookTarget = {
            position_.x + forward_.x,
            position_.y + forward_.y,
            position_.z + forward_.z
        };

        view_       = Math::LookAt(position_, lookTarget, up_);
        projection_ = Math::Perspective(
            fov_, aspect_, nearZ_, farZ_);
    }

} // namespace RiftCore
