#pragma once

#include "imgui/imgui.h"
#include <vector>
#include <random>

struct StarParticle {
    ImVec2 pos;
    ImVec2 velocity;
    float size;
    float opacity;
    int num_points; // Number of points for the star
};

class StarEffect {
public:
    StarEffect();
    void Init(int screen_width, int screen_height);
    void Update(float delta_time);
    void Render(ImDrawList* draw_list, const float star_color[4]);

private:
    std::vector<StarParticle> star_particles;
    int screen_width;
    int screen_height;
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist_x;
    std::uniform_real_distribution<float> dist_y;
    std::uniform_real_distribution<float> dist_size;
    std::uniform_real_distribution<float> dist_opacity;
    std::uniform_real_distribution<float> dist_velocity_x;
    std::uniform_real_distribution<float> dist_velocity_y;
    std::uniform_int_distribution<int> dist_num_points; // Distribution for number of star points

    void ResetStarParticle(StarParticle& particle);
};
