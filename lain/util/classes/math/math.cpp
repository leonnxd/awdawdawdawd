#include "math.h"
#include <cmath>

namespace math {
    namespace easing {
        float linear(float t) {
            return t;
        }

        float easeInQuad(float t) {
            return t * t;
        }

        float easeOutQuad(float t) {
            return t * (2 - t);
        }

        float easeInOutQuad(float t) {
            return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
        }

        float easeInCubic(float t) {
            return t * t * t;
        }

        float easeOutCubic(float t) {
            return (--t) * t * t + 1;
        }

        float easeInOutCubic(float t) {
            return t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
        }

        float easeInSine(float t) {
            return 1 - cos((t * 3.14159265358979323846) / 2);
        }

        float easeOutSine(float t) {
            return sin((t * 3.14159265358979323846) / 2);
        }

        float easeInOutSine(float t) {
            return -(cos(3.14159265358979323846 * t) - 1) / 2;
        }
    }

    Matrix3 Matrix3::from_yaw(float yaw) {
        float rad = yaw * (3.14159265358979323846f / 180.0f);
        float c = cos(rad);
        float s = sin(rad);

        Matrix3 result{};
        result.data[0] = c; result.data[1] = 0; result.data[2] = s;
        result.data[3] = 0; result.data[4] = 1; result.data[5] = 0;
        result.data[6] = -s; result.data[7] = 0; result.data[8] = c;
        return result;
    }

    float CalculateDistance1(Vector3 first, Vector3 sec) {
        return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2) + pow(first.z - sec.z, 2));
    }
}
