#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "../../util/classes/classes.h"

namespace wallcheck {

    struct CachedPart {
        math::Vector3 position{};
        math::Vector3 size{};
        math::Matrix3 rotation{};
        uintptr_t address{};
    };

    void initialize();
    void shutdown();

    void update_cache();
    bool cache_ready();

    bool is_obstructed(const math::Vector3& from, const math::Vector3& to, uintptr_t ignore_address = 0);
    bool can_see(const math::Vector3& from, const math::Vector3& to);

    bool is_valid_vector(const math::Vector3& v);
    bool is_valid_matrix(const math::Matrix3& m);
    bool is_valid_address(uintptr_t address);
}