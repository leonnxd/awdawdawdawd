#pragma once
#include "structs.h"

namespace math {
    inline Vector3 AddVector3(const Vector3& first, const Vector3& sec) {
        return { first.x + sec.x, first.y + sec.y, first.z + sec.z };
    }

    inline Vector3 DivVector3(const Vector3& first, const Vector3& sec) {
        return { first.x / sec.x, first.y / sec.y, first.z / sec.z };
    }
    float CalculateDistance1(Vector3 first, Vector3 sec);
    Matrix3 LookAtToMatrix(const Vector3& cameraPosition, const Vector3& targetPosition);
    Vector3 crossProduct(const Vector3& vec1, const Vector3& vec2);
    Vector3 normalize(const Vector3& vec);
    Matrix3 from_yaw(float yaw);

    namespace easing {
        float linear(float t);
        float easeInQuad(float t);
        float easeOutQuad(float t);
        float easeInOutQuad(float t);
        float easeInCubic(float t);
        float easeOutCubic(float t);
        float easeInOutCubic(float t);
        float easeInSine(float t);
        float easeOutSine(float t);
        float easeInOutSine(float t);
    }
}
