#include "../classes.h"
#include "../../driver/driver.h"
#include "../../offsets.h"
#include "../math/math.h"
#include "../../globals.h"

void roblox::instance::spectate(uint64_t stringhere) {
	write<std::uint64_t>(globals::instances::camera.address + offsets::CameraSubject, stringhere);
}
void roblox::instance::unspectate() {
	write<std::uint64_t>(globals::instances::camera.address + offsets::CameraSubject, globals::instances::lp.humanoid.address);
}
math::Vector3 roblox::camera::getPos() {
	return read<math::Vector3>(this->address + offsets::CameraPos);
}
math::Matrix3 roblox::camera::getRot() {
	return read<math::Matrix3>(this->address + offsets::CameraRotation);
}
void roblox::camera::writePos(math::Vector3 arg) {
	write<math::Vector3>(this->address + offsets::CameraPos, arg);
}
void roblox::camera::writeRot(math::Matrix3 arg) {
	write<math::Matrix3>(this->address + offsets::CameraRotation, arg);
}

int roblox::camera::getType() {
	return read<int>(this->address + offsets::CameraType);
}
void roblox::camera::setType(int type) {
	write<int>(this->address + offsets::CameraType, type);
}
roblox::instance roblox::camera::getSubject() {
	return read<roblox::instance>(this->address + offsets::CameraSubject);
}



int roblox::camera::getMode() {
	return read<int>(this->address + offsets::CameraMode);
}
void roblox::camera::setMode(int type) {
	write<int>(this->address + offsets::CameraMode, type);
}
