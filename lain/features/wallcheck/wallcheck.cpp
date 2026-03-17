#include "wallcheck.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>

#include <Windows.h>

#include "../../util/driver/driver.h"
#include "../../util/globals.h"
#include "../../util/offsets.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

using namespace math;

namespace wallcheck {

    namespace {
        constexpr float k_min_dimension = 0.05f;
        constexpr float k_max_dimension = 2048.0f;
        constexpr float k_max_transparency = 0.95f;
        constexpr float k_max_ray_distance = 500.0f;
        constexpr float k_direction_epsilon = 1e-6f;
        constexpr float k_sample_radius = 0.35f;
        constexpr int k_required_clear_rays = 1;
        constexpr size_t k_max_parts_per_refresh = 100000;
        constexpr size_t k_max_invalid_reads = 2048;

        std::vector<CachedPart> g_cached_parts;
        std::mutex g_cache_mutex;

        std::thread g_worker_thread;
        std::atomic<bool> g_should_stop{ false };
        std::atomic<bool> g_is_running{ false };

        std::chrono::milliseconds g_update_interval{ 250 };

        Matrix3 transpose(const Matrix3& m) {
            Matrix3 result;
            result.data[0] = m.data[0]; result.data[1] = m.data[3]; result.data[2] = m.data[6];
            result.data[3] = m.data[1]; result.data[4] = m.data[4]; result.data[5] = m.data[7];
            result.data[6] = m.data[2]; result.data[7] = m.data[5]; result.data[8] = m.data[8];
            return result;
        }

        Vector3 multiply(const Matrix3& m, const Vector3& v) {
            return {
                m.data[0] * v.x + m.data[1] * v.y + m.data[2] * v.z,
                m.data[3] * v.x + m.data[4] * v.y + m.data[5] * v.z,
                m.data[6] * v.x + m.data[7] * v.y + m.data[8] * v.z
            };
        }

        Vector3 cross(const Vector3& a, const Vector3& b) {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        bool ray_intersects_part(const Vector3& origin, const Vector3& direction, float max_distance, const CachedPart& part) {
            Vector3 to_part = part.position - origin;
            float distance_to_part_sq = to_part.dot(to_part);
            float bounding_radius = part.size.magnitude() * 0.866f;
            float combined = bounding_radius + max_distance;
            if (distance_to_part_sq > combined * combined) {
                return false;
            }

            Matrix3 inverse_rotation = transpose(part.rotation);
            Vector3 local_origin = multiply(inverse_rotation, origin - part.position);
            Vector3 local_direction = multiply(inverse_rotation, direction);

            Vector3 half_size = part.size * 0.5f;
            Vector3 min_bounds = { -half_size.x, -half_size.y, -half_size.z };
            Vector3 max_bounds = { half_size.x, half_size.y, half_size.z };

            float tmin = -std::numeric_limits<float>::infinity();
            float tmax = std::numeric_limits<float>::infinity();

            const float origin_components[3] = { local_origin.x, local_origin.y, local_origin.z };
            const float direction_components[3] = { local_direction.x, local_direction.y, local_direction.z };
            const float min_components[3] = { min_bounds.x, min_bounds.y, min_bounds.z };
            const float max_components[3] = { max_bounds.x, max_bounds.y, max_bounds.z };

            for (int axis = 0; axis < 3; ++axis) {
                float d = direction_components[axis];
                float o = origin_components[axis];
                float mn = min_components[axis];
                float mx = max_components[axis];

                if (std::fabs(d) < k_direction_epsilon) {
                    if (o < mn || o > mx) {
                        return false;
                    }
                    continue;
                }

                float t1 = (mn - o) / d;
                float t2 = (mx - o) / d;
                if (t1 > t2) std::swap(t1, t2);

                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);

                if (tmin > tmax) {
                    return false;
                }
                if (tmax < 0.0f) {
                    return false;
                }
                if (tmin > max_distance) {
                    return false;
                }
            }

            return (tmin > 0.0f || tmax > 0.0f) && tmin <= max_distance && tmin <= tmax;
        }

        void worker_loop() {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
            while (!g_should_stop) {
                update_cache();
                std::this_thread::sleep_for(g_update_interval);
            }
        }
    }

    bool is_valid_vector(const Vector3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
            !std::isnan(v.x) && !std::isnan(v.y) && !std::isnan(v.z) &&
            std::abs(v.x) < 1e6f && std::abs(v.y) < 1e6f && std::abs(v.z) < 1e6f;
    }

    bool is_valid_matrix(const Matrix3& m) {
        for (float value : m.data) {
            if (!std::isfinite(value) || std::isnan(value) || std::abs(value) > 10.0f) {
                return false;
            }
        }
        return true;
    }

    bool is_valid_address(uintptr_t address) {
        return ::is_valid_address(address);
    }

    void initialize() {
        if (g_is_running.load()) {
            return;
        }
        g_should_stop = false;
        g_worker_thread = std::thread(worker_loop);
        g_is_running = true;
    }

    void shutdown() {
        if (!g_is_running.load()) {
            return;
        }
        g_should_stop = true;
        if (g_worker_thread.joinable()) {
            g_worker_thread.join();
        }
        g_is_running = false;
    }

    void update_cache() {
        if (!globals::instances::datamodel.is_valid()) {
            return;
        }

        roblox::instance workspace = globals::instances::workspace;
        if (!workspace.is_valid()) {
            workspace = globals::instances::datamodel.findfirstchildofclass("Workspace");
            if (!workspace.is_valid()) {
                return;
            }
        }

        uintptr_t first = read<uintptr_t>(workspace.address + offsets::PrimitivesPointer1);
        if (!is_valid_address(first)) {
            return;
        }

        uintptr_t second = read<uintptr_t>(first + offsets::PrimitivesPointer2);
        if (!is_valid_address(second)) {
            return;
        }

        std::unordered_set<uintptr_t> seen_primitives;
        std::vector<CachedPart> new_parts;
        new_parts.reserve(4096);

        size_t invalid_reads = 0;

        for (size_t offset = 0; offset < k_max_parts_per_refresh; offset += sizeof(uintptr_t)) {
            if (g_should_stop) {
                break;
            }

            uintptr_t primitive = read<uintptr_t>(second + offset);
            if (!is_valid_address(primitive)) {
                if (++invalid_reads > k_max_invalid_reads) {
                    break;
                }
                continue;
            }
            invalid_reads = 0;

            if (!seen_primitives.insert(primitive).second) {
                continue;
            }

            bool anchored = read<bool>(primitive + offsets::Anchored);
            if (!anchored) {
                continue;
            }

            bool can_collide = read<bool>(primitive + offsets::CanCollide);
            if (!can_collide) {
                continue;
            }

            float transparency = read<float>(primitive + offsets::Transparency);
            if (transparency > k_max_transparency) {
                continue;
            }

            Vector3 size = read<Vector3>(primitive + offsets::PartSize);
            if (!is_valid_vector(size)) {
                continue;
            }

            float max_component = std::max(std::max(std::fabs(size.x), std::fabs(size.y)), std::fabs(size.z));
            float min_component = std::min(std::min(std::fabs(size.x), std::fabs(size.y)), std::fabs(size.z));
            if (max_component > k_max_dimension) {
                continue;
            }
            if (min_component < k_min_dimension) {
                continue;
            }

            CFrame cframe = read<CFrame>(primitive + offsets::CFrame);
            Vector3 position = cframe.position;
            if (!is_valid_vector(position)) {
                continue;
            }

            Matrix3 rotation = cframe.rotation;
            if (!is_valid_matrix(rotation)) {
                continue;
            }

            CachedPart part{};
            part.position = position;
            part.size = size;
            part.rotation = rotation;
            part.address = primitive;
            new_parts.emplace_back(part);
        }

        if (!new_parts.empty()) {
            std::lock_guard<std::mutex> lock(g_cache_mutex);
            g_cached_parts = std::move(new_parts);
        }
    }

    bool cache_ready() {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        return !g_cached_parts.empty();
    }

    bool is_obstructed(const Vector3& from, const Vector3& to, uintptr_t ignore_address) {
        if (!is_valid_vector(from) || !is_valid_vector(to)) {
            return false;
        }

        Vector3 delta = to - from;
        float distance = delta.magnitude();
        if (distance < k_direction_epsilon) {
            return false;
        }
        if (distance > k_max_ray_distance) {
            return false;
        }

        std::vector<CachedPart> parts_copy;
        {
            std::lock_guard<std::mutex> lock(g_cache_mutex);
            parts_copy = g_cached_parts;
        }

        if (parts_copy.empty()) {
            return false;
        }

        if (parts_copy.empty()) {
            return false;
        }

        Vector3 primary_dir = delta / distance;

        Vector3 up_reference{ 0.0f, 1.0f, 0.0f };
        Vector3 right = cross(primary_dir, up_reference);
        if (right.magnitude() < 1e-3f) {
            right = cross(primary_dir, Vector3{ 1.0f, 0.0f, 0.0f });
            if (right.magnitude() < 1e-3f) {
                right = cross(primary_dir, Vector3{ 0.0f, 0.0f, 1.0f });
            }
        }
        right = right.Normalized();
        Vector3 up = cross(right, primary_dir).Normalized();

        std::array<Vector3, 5> sample_targets{
            to,
            to + up * k_sample_radius,
            to - up * k_sample_radius,
            to + right * k_sample_radius,
            to - right * k_sample_radius
        };

        int clear_rays = 0;

        for (const auto& sample_target : sample_targets) {
            Vector3 sample_delta = sample_target - from;
            float sample_distance = sample_delta.magnitude();
            if (sample_distance < k_direction_epsilon) {
                ++clear_rays;
                if (clear_rays >= k_required_clear_rays) {
                    return false;
                }
                continue;
            }

            Vector3 sample_dir = sample_delta / sample_distance;
            bool blocked = false;

            for (const auto& part : parts_copy) {
                if (part.address == ignore_address) {
                    continue;
                }

                float part_distance = (part.position - from).magnitude();
                if (part_distance > sample_distance + part.size.magnitude() + 2.0f) {
                    continue;
                }

                if (ray_intersects_part(from, sample_dir, sample_distance, part)) {
                    blocked = true;
                    break;
                }
            }

            if (!blocked) {
                ++clear_rays;
                if (clear_rays >= k_required_clear_rays) {
                    return false;
                }
            }
        }

        return clear_rays < k_required_clear_rays;
    }

    bool can_see(const Vector3& from, const Vector3& to) {
        if (!g_is_running.load()) {
            initialize();
        }
        return !is_obstructed(from, to);
    }
}