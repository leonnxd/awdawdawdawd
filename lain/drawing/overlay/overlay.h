#pragma once
#include <memory>
#include "../../util/avatarmanager/avatarmanager.h"

namespace overlay {
	void load_interface();
	inline bool visible = false;

	inline std::unique_ptr<AvatarManager> avatar_manager;
	inline std::vector<uint64_t> last_known_players;
	inline uint64_t g_spectated_address = 0; // Global spectated address
	inline uint64_t g_spectated_player_userid = 0;
	inline uint64_t g_selected_player_address = 0; // Global selected player address for card display
	inline bool g_playerlist_just_opened = false; // Flag to ignore clicks on the first frame the playerlist is opened
	inline int g_ignore_mouse_frames = 0; // Number of frames to ignore mouse input after opening overlay

	static void initialize_avatar_system();
	static void update_avatars();
	static void update_lobby_players();  
	static AvatarManager* get_avatar_manager();
	static void cleanup_avatar_system();

}
