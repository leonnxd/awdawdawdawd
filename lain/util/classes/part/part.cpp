#include "../classes.h"
#include "../math/math.h"

math::Vector3 roblox::instance::get_part_size() {
	return read<math::Vector3>(this->primitive() + offsets::PartSize);
}

void roblox::instance::write_part_size(math::Vector3 size) {
	 write<math::Vector3>(this->primitive() + offsets::PartSize, size);
}

math::Vector3 roblox::instance::get_move_dir() {
	return read<math::Vector3>(this->address + offsets::MoveDirection);
}

void roblox::instance::write_move_dir(math::Vector3 size) {
	write<math::Vector3>(this->address + offsets::MoveDirection, size);
}
