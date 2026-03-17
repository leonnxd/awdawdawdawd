#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>

namespace autofriend {
    struct GroupMember {
        std::string username;
        std::string displayName;
        uint64_t userId;
        std::string role;
    };

    class AutoFriendManager {
    private:
        std::atomic<bool> is_loading{false};
        std::atomic<bool> is_friending{false};
        std::vector<GroupMember> cached_members;
        std::string last_error;
        
    public:
        AutoFriendManager() = default;
        ~AutoFriendManager() = default;
        
        // Fetch group members from Roblox API
        bool fetchGroupMembers(const std::string& groupId);
        
        // Get cached members
        const std::vector<GroupMember>& getCachedMembers() const { return cached_members; }
        
        // Check if currently loading
        bool isLoading() const { return is_loading.load(); }
        
        // Check if currently friending
        bool isFriending() const { return is_friending.load(); }
        
        // Get last error message
        const std::string& getLastError() const { return last_error; }
        
        // Clear cached members
        void clearCache() { cached_members.clear(); }
        
        // Get member count
        size_t getMemberCount() const { return cached_members.size(); }
        
        // Check if a specific username is in the group
        bool isPlayerInGroup(const std::string& username) const {
            for (const auto& member : cached_members) {
                if (member.username == username || member.displayName == username) {
                    return true;
                }
            }
            return false;
        }
        
        // Get member by username
        const GroupMember* getMemberByUsername(const std::string& username) const {
            for (const auto& member : cached_members) {
                if (member.username == username || member.displayName == username) {
                    return &member;
                }
            }
            return nullptr;
        }
    };
    
    // Global instance
    extern AutoFriendManager g_auto_friend_manager;
}
