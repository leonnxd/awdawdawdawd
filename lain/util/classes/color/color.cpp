#include "../classes.h"
#include "../math/math.h"

uint32_t roblox::Vector3ToIntBGR(math::Vector3 color) {
	int blue = static_cast<int>(color.z * 255.0f);
	int green = static_cast<int>(color.y * 255.0f);
	int red = static_cast<int>(color.x * 255.0f);

	if (blue > 255) blue = 255;
	if (blue < 0) blue = 0;
	if (green > 255) green = 255;
	if (green < 0) green = 0;
	if (red > 255) red = 255;
	if (red < 0) red = 0;

	uint32_t result = (static_cast<uint32_t>(blue) << 16) |
		(static_cast<uint32_t>(green) << 8) |
		static_cast<uint32_t>(red);
	return result;
}



int roblox::instance::read_color() {
	return read<int>(this->primitive() + offsets::MeshPartColor3);
}
void roblox::instance::write_color(int col) {
	write<int>(this->primitive() + offsets::MeshPartColor3, col);
}
