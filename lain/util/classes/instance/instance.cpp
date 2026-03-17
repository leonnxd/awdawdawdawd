#include "../classes.h"
#include "../../offsets.h"
#include "../../globals.h"
#include "../math/math.h"

math::Vector2 roblox::instance::GetDimensins()
{
    return read<math::Vector2>(this->address + offsets::Dimensions);
}

math::Matrix4 roblox::instance::GetViewMatrix()
{
    return read<math::Matrix4>(this->address + offsets::viewmatrix);
}

void roblox::instance::setAnchored(bool booz)
{
    write<bool>(this->primitive() + offsets::Anchored, booz);
}

uintptr_t roblox::instance::primitive() {
    if (!is_valid_address(address)) return 0;
    return read<uintptr_t>(this->address + offsets::Primitive);
}

struct stringed {
    char data[200];
};
std::string read_string(std::uint64_t address)
{
    auto character = read<stringed>(address);
    return character.data;
}

std::string fetchstring(std::uint64_t address)
{
    int length = read<int>(address + 0x18);

    if (length >= 16u) {
        std::uintptr_t padding = read<std::uintptr_t>(address);
        return read_string(padding);
    }
    std::string name = read_string(address);
    return name;
}

std::string roblox::instance::get_name() {
    return fetchstring(read<uintptr_t>(this->address + offsets::Name));
}
float roblox::instance::read_health() {
    return read<float>(this->address + offsets::Health);
}
void roblox::instance::write_health(float health) {
    write<float>(this->address + offsets::Health, health);
}

math::CFrame roblox::instance::read_cframe() {
    return read<math::CFrame>(this->primitive() + offsets::CFrame);
}
math::Matrix3 roblox::instance::read_part_cframe() {
    return read<math::Matrix3>(this->primitive() + offsets::CFrame);
}
void roblox::instance::write_cframe(math::CFrame cframe) {
    write<math::CFrame>(this->primitive() + offsets::CFrame, cframe);
}
void roblox::instance::write_part_cframe(math::Matrix3 ma3) {
     write<math::Matrix3>(this->primitive() + offsets::CFrame, ma3);
}
float roblox::instance::read_maxhealth() {
    return read<float>(this->address + offsets::MaxHealth);
}
std::string roblox::instance::get_class_name() {
    return fetchstring(read<uintptr_t>(read<uintptr_t>(this->address + offsets::ClassDescriptor) + offsets::ChildrenEnd));
}
std::string roblox::instance::get_displayname() {
    const std::uint64_t name_pointer = read<std::uint64_t>(this->address + offsets::DisplayName);

    if (name_pointer)
    {
        std::string ptr_result = fetchstring(name_pointer);
        if (!ptr_result.empty())
        {
            return ptr_result;
        }
    }

    // Fallback to reading the regular name if display name is not available
    return get_name();
}

std::string roblox::instance::get_humdisplayname() {
    const std::uint64_t name_pointer = read<std::uint64_t>(this->address + offsets::HumanoidDisplayName);

    if (name_pointer)
    {
        std::string ptr_result = fetchstring(name_pointer);
        if (!ptr_result.empty())
        {
            return ptr_result;
        }
    }

    // Fallback to reading the regular name if display name is not available
    return get_name();
}

roblox::instance roblox::instance::findfirstchild(std::string arg) {
    roblox::instance returned;
    for (auto children : this->get_children()) {
        if (children.get_name() == arg) returned = children;
    }
    return returned;
}
roblox::instance roblox::instance::read_service(std::string arg) {
    roblox::instance returned{};
    for (auto children : this->get_children()) {
        if (children.get_class_name() == arg) returned = children;
    }
    return returned;
}

roblox::instance roblox::instance::read_parent() {
    return read<roblox::instance>(this->address + offsets::Parent);

}

math::Vector3 roblox::instance::get_pos() {
    return read<math::Vector3>(this->primitive() + offsets::Position);
}
math::Vector3 roblox::instance::get_velocity() {
    if (!is_valid_address(address)) return {};
    return read<math::Vector3>(this->primitive() + offsets::Velocity);
}

void roblox::instance::write_position(math::Vector3 arg) {
    write<math::Vector3>(this->primitive() + offsets::Position, arg);
}
void roblox::instance::write_velocity(math::Vector3 arg) {
    write<math::Vector3>(this->primitive() + offsets::Velocity, arg);
}

void roblox::instance::write_ambiance(math::Vector3 col) {
    if (!is_valid_address(this->address)) return;
    write<math::Vector3>(this->address + offsets::OutdoorAmbient, col); // Assuming OutdoorAmbient is the correct offset for ambiance
}

void roblox::instance::write_fogcolor(math::Vector3 col) {
    if (!is_valid_address(this->address)) return;
    write<math::Vector3>(this->address + offsets::FogColor, col);
}

void roblox::instance::write_fogstart(float distance) {
    if (!is_valid_address(this->address)) return;
    write<float>(this->address + offsets::FogStart, distance);
}

void roblox::instance::write_fogend(float distance) {
    if (!is_valid_address(this->address)) return;
    write<float>(this->address + offsets::FogEnd, distance);
}

void roblox::instance::write_cancollide(bool boolleanz)
{
    write<bool>(this->address + offsets::CanCollide, boolleanz);
}

bool roblox::instance::get_cancollide()
{
    return read<bool>(this->address + offsets::CanCollide);
}

math::Vector3 roblox::instance::get_cam_pos() {
    return read<math::Vector3>(this->address + offsets::CameraPos);
}
math::Matrix3 roblox::instance::get_cam_rot() {
    return read<math::Matrix3>(this->address + offsets::CameraRotation);
}

void roblox::instance::write_cam_pos(math::Vector3 pos) {
    write<math::Vector3>(this->address + offsets::CameraPos, pos);
}
void roblox::instance::write_cam_rot(math::Matrix3 rot) {
    write<math::Matrix3>(this->address + offsets::CameraRotation, rot);
}

roblox::instance roblox::instance::model_instance() {
    return read<roblox::instance>(this->address + offsets::ModelInstance);
}
roblox::instance roblox::instance::local_player() {
    return read<roblox::instance>(this->address + offsets::LocalPlayer);
}

roblox::instance roblox::instance::read_workspace()
{
    return read<roblox::instance>(this->address + offsets::Workspace);
}

math::Vector2 roblox::instance::get_dimensions() {
    return read<math::Vector2>(this->address + offsets::Dimensions);
}

math::Vector3 roblox::instance::read_vector3_value() {
    return read<math::Vector3>(this->address + offsets::Value);
}
int roblox::instance::read_int_value() {
    return read<int>(this->address + offsets::Value);
}
bool roblox::instance::read_bool_value() {
    return read<bool>(this->address + offsets::Value);
}

void roblox::instance::write_bool_value(bool val) {
    write<bool>(this->address + offsets::Value, val);
}
void roblox::instance::write_int_value(int val) {
    write<int>(this->address + offsets::Value, val);
}
void roblox::instance::write_vector3_value(math::Vector3 val) {
    write<math::Vector3>(this->address + offsets::Value, val);
}
void roblox::instance::write_float_value(float val) {
    write<float>(this->address + offsets::Value, val);
}
double roblox::instance::read_double_value() {
    return read<double>(this->address + offsets::Value);
}
void roblox::instance::write_double_value(double val) {
    write<double>(this->address + offsets::Value, val);
}
float roblox::instance::read_float_value() {
    return read<float>(this->address + offsets::Value);
}

std::vector <roblox::instance> roblox::instance::get_children() {
    std::vector<roblox::instance> children;
    std::uint64_t begin = read<std::uint64_t>(this->address + offsets::Children);
    std::uint64_t end = read<std::uint64_t>(begin + offsets::ChildrenEnd);
    if (!begin) return children; if (!end) return children;
    for (auto instances = read<std::uint64_t>(begin); instances != end; instances += 16u)
    {
        children.emplace_back(read<roblox::instance>(instances));
    }
    return children;
}
struct Quaternion final { float x, y, z, w; };

math::Vector2 roblox::worldtoscreen(math::Vector3 world) {
    Quaternion quaternion;
    math::Vector2 dimensions = globals::instances::visualengine.GetDimensins();
    math::Matrix4 viewmatrix = read<math::Matrix4>(globals::instances::visualengine.address + offsets::viewmatrix);
    quaternion.x = (world.x * viewmatrix.data[0]) + (world.y * viewmatrix.data[1]) + (world.z * viewmatrix.data[2]) + viewmatrix.data[3];
    quaternion.y = (world.x * viewmatrix.data[4]) + (world.y * viewmatrix.data[5]) + (world.z * viewmatrix.data[6]) + viewmatrix.data[7];
    quaternion.z = (world.x * viewmatrix.data[8]) + (world.y * viewmatrix.data[9]) + (world.z * viewmatrix.data[10]) + viewmatrix.data[11];
    quaternion.w = (world.x * viewmatrix.data[12]) + (world.y * viewmatrix.data[13]) + (world.z * viewmatrix.data[14]) + viewmatrix.data[15];

    if (quaternion.w < -0.01)
        return { -1, -1 };

    float inv_w = 1.0f / quaternion.w;
    math::Vector3 ndc;
    ndc.x = quaternion.x * inv_w;
    ndc.y = quaternion.y * inv_w;
    ndc.z = quaternion.z * inv_w;

    math::Vector2 screenPos = {
        (dimensions.x / 2 * ndc.x) + (dimensions.x / 2),
        -(dimensions.y / 2 * ndc.y) + (dimensions.y / 2)
    };

    if (screenPos.x < 0 || screenPos.x > dimensions.x || screenPos.y < 0 || screenPos.y > dimensions.y)
        return { -1, -1 };

    return screenPos;
}

float roblox::instance::get_health() {
    if (!is_valid()) return 0.0f;
    return read<float>(address + offsets::Health);
}

int roblox::instance::get_state() {
    if (!is_valid()) return 0;
    return read<int>(address + offsets::HumanoidState);
}

roblox::instance roblox::instance::findfirstchildofclass(std::string arg) {
    if (!is_valid()) return {};
    auto children = get_children();
    for (roblox::instance child : children) {
        if (child.get_class_name() == arg) {
            return child;
        }
    }
    return {};
}

std::string roblox::instance::get_game_id() {
    uint64_t placeId = read<uint64_t>(this->address + offsets::PlaceId);
    return std::to_string(placeId);
}
