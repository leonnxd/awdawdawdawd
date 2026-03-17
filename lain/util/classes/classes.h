#pragma once

#include "math/math.h"
#include <iostream>
#include <vector>
#include "../../drawing/imgui/imgui.h"
#include "../driver/driver.h"
#include "../offsets.h"

namespace roblox {
	class instance {
	public:
		uintptr_t address;
		static std::uint64_t cached_input_object;
		bool operator==(const roblox::instance& get) const {
			return this->address == get.address;
		}

		bool is_valid() const noexcept {
			return address != 0;
		}

		math::Vector2 GetDimensins();

		math::Matrix4 GetViewMatrix();

		void setAnchored(bool booz);
		uintptr_t primitive();
		//setFramePosX//setFramePosY//initialize_mouseservice//write_mouse_position

		void setFramePosX(uint64_t pos);
		void setFramePosY(uint64_t pos);
		void initialize_mouse_service(std::uint64_t address);
		void write_mouse_position(std::uint64_t address, float x, float y);
		void rescan_silent_mouse_service();
		std::string get_name();
		float read_health();
		void write_health(float health);
		float read_maxhealth();
		std::string get_displayname();
		std::string get_humdisplayname();
		std::string get_class_name();

		std::vector<instance> get_children();

		instance findfirstchild(std::string arg);
		instance findfirstchildofclass(std::string arg);
		float get_health();
		int get_state();
		instance read_service(std::string arg);
		void write_animationid(std::string id);
		std::string read_animationid();
		instance read_parent();

		math::Vector3 get_pos();
		math::Vector3 get_velocity();

		void write_position(math::Vector3 pos);
		void write_velocity(math::Vector3 velocity);

		int read_color();
		void write_color(int col);

		void write_ambiance(math::Vector3 col);

		void write_fogcolor(math::Vector3 col);
		math::Vector3 read_fogcolor();

		void write_fogstart(float distance);
		float read_fogstart();

		void write_fogend(float distance);
		float read_fogend();

		void write_cancollide(bool boolleanz);
		bool get_cancollide();
		math::Vector3 get_cam_pos();
		math::Matrix3 get_cam_rot();

		void write_cam_pos(math::Vector3 arg);
		void write_cam_rot(math::Matrix3 arg);

		instance model_instance();
		instance local_player();

		roblox::instance read_workspace();

		math::Vector2 get_dimensions();

		math::Vector3 read_vector3_value();
		int read_int_value();
		bool read_bool_value();

		void write_vector3_value(math::Vector3 arg);
		void write_float_value(float val);
		double read_double_value();
		void write_double_value(double val);
		float read_float_value();
		void write_int_value(int arg);
		void write_bool_value(bool arg);

		roblox::instance read_userid();

		void writeworkspacegrav(float gravity);

		float read_workspacegrav();

		float read_walkspeed();
		void write_walkspeed(float value);
		float read_jumppower();
		void write_jumppower(float value);
		float read_hipheight();
		void write_hipheight(float value);
		bool read_sitting();
		void write_sitting(bool value);
		math::Vector3 get_part_size();

		void write_part_size(math::Vector3 size);

		math::Vector3 get_move_dir();

		void write_move_dir(math::Vector3 size);

		math::CFrame read_cframe();
		math::Matrix3 read_part_cframe();
		void write_cframe(math::CFrame cframe);
		void write_part_cframe(math::Matrix3 ma3);
		void spectate(uint64_t stringhere);
		void unspectate();

		std::string get_game_id();

		// fetch entity
		int fetch_entity(std::uint64_t address) const;
	};

	static instance find_child(instance parent, const std::vector<std::string>& names) {
		for (const auto& name : names) {
			if (!parent.is_valid()) {
				return {};
			}
			parent = parent.findfirstchild(name);
		}
		return parent;
	}

	class wall {
	public:
		math::Vector3 size;
		math::CFrame cframe;
		math::Vector3 position;
		math::Matrix3 rotation;
	};

	class player {
	public:
		uintptr_t address;
		bool operator==(const roblox::player& get) const {
			return this->address == get.address;
		}

		int rigtype = 0;
		roblox::instance torso;
	//	std::unordered_map<std::string, roblox::instance> cachedchildren;
		roblox::instance head;
		roblox::instance hrp;
		roblox::instance uppertorso;
		roblox::instance lowertorso;
		roblox::instance rightupperarm;
		roblox::instance leftupperarm;
		roblox::instance rightlowerarm;
		roblox::instance leftlowerarm;
		roblox::instance lefthand;
		roblox::instance righthand;
		roblox::instance leftupperleg;
		roblox::instance rightupperleg;
		roblox::instance rightlowerleg;
		roblox::instance leftlowerleg;
		roblox::instance rightfoot;
		roblox::instance leftfoot;
		roblox::instance humanoid;
		roblox::instance main;
		std::string flag;
		bool pictureDownloaded = false;
		roblox::instance team;
		roblox::instance instance;
		int maxhealth;
		int health;
		int armor;
		roblox::instance bodyeffects;
		std::vector<roblox::instance> bones;
		std::string name;
		std::string displayname;
		std::string toolname;
		roblox::instance tool;
		roblox::instance userid;
		int status;
	};

	class camera {
	public:
		uintptr_t address;

		bool operator==(const roblox::camera& get) const {
			return this->address == get.address;
		}


		math::Vector3 getPos();

		math::Matrix3 getRot();

		void writePos(math::Vector3 arg);

		void writeRot(math::Matrix3 arg);

		int getType();

		void setType(int type);

		roblox::instance getSubject();

		int getMode();

		void setMode(int type);

	};
	math::Vector2 worldtoscreen(math::Vector3 world);
	inline std::uint64_t cached_input_object;

	void cached_input_objectzz();
	uint32_t Vector3ToIntBGR(math::Vector3 color);
}
namespace engine {
	bool startup();

}
