#pragma once
#include <cmath>

namespace math {
    struct Vector2 {
        float x, y;
        bool operator==(const Vector2& other) const {
            return x == other.x && y == other.y;
        }
        Vector2 operator+(const Vector2& other) const {
            return { x + other.x, y + other.y };
        }
        Vector2 operator-(const Vector2& other) const {
            return { x - other.x, y - other.y };
        }
        Vector2 operator*(float scalar) const {
            return { x * scalar, y * scalar };
        }
        Vector2 operator/(float scalar) const {
            return { x / scalar, y / scalar };
        }
        float magnitude() const {
            return sqrt(x * x + y * y);
        }
        float dot(const Vector2& other) const {
            return x * other.x + y * other.y;
        }
    };

    struct Vector3 {
        float x, y, z;

        Vector3() : x(0.0f), y(0.0f), z(0.0f) {} // Default constructor
        Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {} // Constructor with three floats

        bool operator==(const Vector3& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
        Vector3 operator+(const Vector3& other) const {
            return { x + other.x, y + other.y, z + other.z };
        }
        Vector3 operator-(const Vector3& other) const {
            return { x - other.x, y - other.y, z - other.z };
        }
        Vector3 operator*(float scalar) const {
            return { x * scalar, y * scalar, z * scalar };
        }
        Vector3 operator/(float scalar) const {
            return { x / scalar, y / scalar, z / scalar };
        }
        float magnitude() const {
            return sqrt(x * x + y * y + z * z);
        }
        float dot(const Vector3& other) const {
            return x * other.x + y * other.y + z * other.z;
        }

        Vector3 Normalized() const {
            float mag = magnitude();
            if (mag > 1e-6f) { // Avoid division by zero
                return { x / mag, y / mag, z / mag };
            }
            return { 0.0f, 0.0f, 0.0f }; // Return zero vector if magnitude is too small
        }
    };

    struct Matrix3 {
        float data[9];

        Matrix3() {
            // Initialize to identity matrix
            data[0] = 1; data[1] = 0; data[2] = 0;
            data[3] = 0; data[4] = 1; data[5] = 0;
            data[6] = 0; data[7] = 0; data[8] = 1;
        }

        static Matrix3 from_yaw(float yaw);

        Matrix3 operator*(const Matrix3& other) const {
            Matrix3 result;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    result.data[i * 3 + j] = 0;
                    for (int k = 0; k < 3; ++k) {
                        result.data[i * 3 + j] += data[i * 3 + k] * other.data[k * 3 + j];
                    }
                }
            }
            return result;
        }
    };

    struct Matrix4 {
        float data[16];
    };

    struct CFrame {
        Matrix3 rotation;
        Vector3 position;
    };
}
