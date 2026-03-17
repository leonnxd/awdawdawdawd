#include "star_effect.h"
#include <chrono>

StarEffect::StarEffect() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
    // Distributions will be initialized in Init() once screen dimensions are known
}

void StarEffect::Init(int screen_width, int screen_height) {
    this->screen_width = screen_width;
    this->screen_height = screen_height;

    dist_x = std::uniform_real_distribution<float>(0.0f, (float)screen_width);
    dist_y = std::uniform_real_distribution<float>(0.0f, (float)screen_height);
    dist_size = std::uniform_real_distribution<float>(1.0f, 5.0f);
    dist_opacity = std::uniform_real_distribution<float>(0.4f, 0.8f);
    dist_velocity_x = std::uniform_real_distribution<float>(-5.0f, 5.0f);
    dist_velocity_y = std::uniform_real_distribution<float>(20.0f, 50.0f);
    dist_num_points = std::uniform_int_distribution<int>(4, 6); // Stars with 4 to 6 points

    star_particles.resize(200); // Number of star particles
    for (auto& particle : star_particles) {
        ResetStarParticle(particle);
        particle.pos.y = dist_y(rng); // Distribute particles vertically on init
    }
}

void StarEffect::ResetStarParticle(StarParticle& particle) {
    particle.pos.x = dist_x(rng);
    particle.pos.y = -10.0f; // Start above the screen
    particle.velocity.x = dist_velocity_x(rng);
    particle.velocity.y = dist_velocity_y(rng);
    particle.size = dist_size(rng);
    particle.opacity = dist_opacity(rng);
    particle.num_points = dist_num_points(rng);
}

void StarEffect::Update(float delta_time) {
    for (auto& particle : star_particles) {
        particle.pos.x += particle.velocity.x * delta_time;
        particle.pos.y += particle.velocity.y * delta_time;

        // Wrap around screen
        if (particle.pos.y > screen_height) {
            ResetStarParticle(particle);
        }
        if (particle.pos.x < -particle.size) {
            particle.pos.x = screen_width + particle.size;
        } else if (particle.pos.x > screen_width + particle.size) {
            particle.pos.x = -particle.size;
        }
    }
}

void StarEffect::Render(ImDrawList* draw_list, const float star_color[4]) {
    for (const auto& particle : star_particles) {
        // Scale alpha per-particle to keep twinkle effect while honoring theme color
        ImU32 color = ImGui::GetColorU32(ImVec4(
            star_color[0],
            star_color[1],
            star_color[2],
            star_color[3] * particle.opacity));
        // Draw a star using AddNgon. For a star shape, we can draw a polygon with num_points.
        // A simple star can be approximated by a regular polygon.
        // For a more "star-like" shape, we would need to draw two polygons (outer and inner points)
        // or use a custom drawing function. For simplicity, we'll use AddNgon for a polygon.
        draw_list->AddNgonFilled(particle.pos, particle.size, color, particle.num_points);
    }
}
