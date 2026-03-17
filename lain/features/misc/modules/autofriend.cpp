#include "autofriend.h"
#include "../../../util/globals.h"
#include <winhttp.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "winhttp.lib")

namespace autofriend {
    AutoFriendManager g_auto_friend_manager;
    
    bool AutoFriendManager::fetchGroupMembers(const std::string& groupId) {
        if (is_loading.load()) {
            return false; // Already loading
        }
        
        is_loading.store(true);
        last_error.clear();
        
        // Run in separate thread to avoid blocking
        std::thread([this, groupId]() {
            try {
                HINTERNET hSession = WinHttpOpen(L"RobloxOverlay/1.0",
                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, 0);

                if (!hSession) {
                    last_error = "Failed to create HTTP session";
                    is_loading.store(false);
                    return;
                }

                WinHttpSetTimeouts(hSession, 30000, 30000, 30000, 30000);

                std::wstring whost(L"groups.roblox.com");
                HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);

                if (!hConnect) {
                    last_error = "Failed to connect to Roblox API";
                    WinHttpCloseHandle(hSession);
                    is_loading.store(false);
                    return;
                }

                // Build API endpoint URL with pagination support
                std::string path = "/v1/groups/" + groupId + "/users?limit=100&sortOrder=Asc";
                std::wstring wpath(path.begin(), path.end());

                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                    NULL, WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    WINHTTP_FLAG_SECURE);

                if (!hRequest) {
                    last_error = "Failed to create HTTP request";
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    is_loading.store(false);
                    return;
                }

                // Add headers
                std::wstring headers = L"Accept: application/json\r\n";
                WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

                BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

                if (bResults) {
                    bResults = WinHttpReceiveResponse(hRequest, NULL);
                }

                if (bResults) {
                    DWORD dwStatusCode = 0;
                    DWORD dwSize = sizeof(dwStatusCode);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

                    if (dwStatusCode == 200) {
                        std::string response;
                        DWORD dwBytesRead = 0;
                        char buffer[8192];

                        do {
                            if (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &dwBytesRead)) {
                                buffer[dwBytesRead] = '\0';
                                response += buffer;
                            }
                        } while (dwBytesRead > 0);

                        // Parse JSON response (simplified parsing)
                        // Note: In a real implementation, you'd use a proper JSON library
                        // This is a basic implementation for demonstration
                        
                        // Debug: Print first 1000 characters of response
                        std::string debug_response = response.substr(0, 1000);
                        std::cout << "API Response (first 1000 chars): " << debug_response << std::endl;
                        
                        // Clear previous members
                        cached_members.clear();
                        
                        // Parse JSON response - look for user objects in the data array
                        size_t dataStart = response.find("\"data\":[");
                        if (dataStart != std::string::npos) {
                            dataStart += 8; // Skip "data":[
                            size_t dataEnd = response.find("]", dataStart);
                            if (dataEnd != std::string::npos) {
                                std::string dataArray = response.substr(dataStart, dataEnd - dataStart);
                                
                                // Parse each user object in the data array
                                size_t pos = 0;
                                while ((pos = dataArray.find("{", pos)) != std::string::npos) {
                                    size_t objEnd = dataArray.find("}", pos);
                                    if (objEnd != std::string::npos) {
                                        std::string userObj = dataArray.substr(pos, objEnd - pos + 1);
                                        
                                        // Extract username
                                        size_t usernamePos = userObj.find("\"username\":");
                                        std::string username = "";
                                        if (usernamePos != std::string::npos) {
                                            usernamePos += 11; // Skip "username":
                                            size_t start = userObj.find("\"", usernamePos);
                                            if (start != std::string::npos) {
                                                start++;
                                                size_t end = userObj.find("\"", start);
                                                if (end != std::string::npos) {
                                                    username = userObj.substr(start, end - start);
                                                }
                                            }
                                        }
                                        
                                        // Extract userId
                                        size_t userIdPos = userObj.find("\"userId\":");
                                        std::string userIdStr = "";
                                        if (userIdPos != std::string::npos) {
                                            userIdPos += 9; // Skip "userId":
                                            size_t start = userIdPos;
                                            while (start < userObj.length() && (userObj[start] == ' ' || userObj[start] == ':')) {
                                                start++;
                                            }
                                            size_t end = start;
                                            while (end < userObj.length() && 
                                                   userObj[end] != ',' && 
                                                   userObj[end] != '}' && 
                                                   userObj[end] != '"') {
                                                end++;
                                            }
                                            if (end > start) {
                                                userIdStr = userObj.substr(start, end - start);
                                            }
                                        }
                                        
                                        // Extract displayName
                                        size_t displayNamePos = userObj.find("\"displayName\":");
                                        std::string displayName = username; // Default to username
                                        if (displayNamePos != std::string::npos) {
                                            displayNamePos += 14; // Skip "displayName":
                                            size_t start = userObj.find("\"", displayNamePos);
                                            if (start != std::string::npos) {
                                                start++;
                                                size_t end = userObj.find("\"", start);
                                                if (end != std::string::npos) {
                                                    displayName = userObj.substr(start, end - start);
                                                }
                                            }
                                        }
                                        
                                        // Add member if we have valid data
                                        if (!username.empty() && !userIdStr.empty()) {
                                            try {
                                                GroupMember member;
                                                member.username = username;
                                                member.displayName = displayName;
                                                member.userId = std::stoull(userIdStr);
                                                member.role = "Member";
                                                
                                                cached_members.push_back(member);
                                                std::cout << "Found member: " << username << " (Display: " << displayName << ", ID: " << userIdStr << ")" << std::endl;
                                            } catch (const std::exception& e) {
                                                std::cout << "Skipped invalid user ID: " << userIdStr << " for user: " << username << std::endl;
                                            }
                                        }
                                        
                                        pos = objEnd + 1;
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // Update globals
                        globals::misc::group_members_loaded = true;
                        globals::misc::loading_group_members = false;
                        
                        if (cached_members.empty()) {
                            globals::misc::autofriend_status = "No members found or parsing failed";
                            std::cout << "No members parsed from response" << std::endl;
                        } else {
                            // Automatically set all group members as friendly
                            for (const auto& member : cached_members) {
                                // Set both username and display name as friendly (in case the game uses either)
                                globals::bools::player_status[member.username] = true; // true = friendly
                                if (member.displayName != member.username) {
                                    globals::bools::player_status[member.displayName] = true; // also set display name as friendly
                                }
                                std::cout << "Set as friendly: " << member.username << " (Display: " << member.displayName << ", ID: " << member.userId << ")" << std::endl;
                            }
                            
                            globals::misc::autofriend_status = "Set " + std::to_string(cached_members.size()) + " members as friendly";
                            std::cout << "Set " << cached_members.size() << " group members as friendly" << std::endl;
                        }
                        
                    } else {
                        last_error = "API request failed with status: " + std::to_string(dwStatusCode);
                        globals::misc::autofriend_status = "Error: " + last_error;
                    }
                } else {
                    last_error = "Failed to receive response from API";
                    globals::misc::autofriend_status = "Error: " + last_error;
                }

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);

            } catch (const std::exception& e) {
                last_error = "Exception: " + std::string(e.what());
                globals::misc::autofriend_status = "Error: " + last_error;
            }

            is_loading.store(false);
        }).detach();
        
        return true;
    }
}
