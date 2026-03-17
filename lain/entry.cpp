#include <iostream>
#include "util/classes/classes.h"
#include <string>
#include <Windows.h>
#include "util/protection/hider.h"
#include "util/protection/vmp/vmprotect.h"
#include "util/protection/hashes/hash.h"
#include "util/console/console.h"

#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <Windows.h>
#include <fstream>
// Keep KeyAuth includes for now to satisfy existing references; we won't use them in the new console flow
#include "util/protection/keyauth/auth.hpp"
#include "util/protection/keyauth/json.hpp"
#include "util/protection/keyauth/skStr.h"
#include "util/protection/keyauth/utils.hpp"
// Add WinHTTP for remote verification to Discord bot service
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <iostream>
#include <string>
#include <functional>
#include <map>
#include <random>
#include <Windows.h>
#include "util/globals.h"
#include "features/wallcheck/wallcheck.h"
#include <TlHelp32.h>
#include "features/misc/misc.h" // Include misc.h for desync function
#include <Psapi.h>
#include <intrin.h>
#include <chrono>
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

extern "C" {
    __declspec(dllimport) void __stdcall VMProtectBegin(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginVirtualization(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginMutation(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginUltra(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginVirtualizationLockByKey(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginUltraLockByKey(const char*);
    __declspec(dllimport) void __stdcall VMProtectEnd(void);

    __declspec(dllimport) BOOL __stdcall VMProtectIsDebuggerPresent(BOOL);
    __declspec(dllimport) BOOL __stdcall VMProtectIsVirtualMachinePresent(void);
    __declspec(dllimport) BOOL __stdcall VMProtectIsValidImageCRC(void);
    __declspec(dllimport) char* __stdcall VMProtectDecryptStringA(const char* value);
    __declspec(dllimport) wchar_t* __stdcall VMProtectDecryptStringW(const wchar_t* value);
    __declspec(dllimport) BOOL __stdcall VMProtectFreeString(void* value);
}


#define vmmutation VMProtectBeginMutation(("MUT_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmvirt VMProtectBeginVirtualization(("VIRT_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmultra VMProtectBeginUltra(("ULTRA_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmmutation_lock VMProtectBeginMutation(("MUT_LOCK_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmvirt_lock VMProtectBeginVirtualizationLockByKey(("VIRT_LOCK_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmultra_lock VMProtectBeginUltraLockByKey(("ULTRA_LOCK_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmend VMProtectEnd();

#define vmpstr(str) VMProtectDecryptStringA(str)
#define vmpwstr(str) VMProtectDecryptStringW(str)
#define skstr(str) skCrypt(str).decrypt()

#define VMPIsDebuggerPresent() VMProtectIsDebuggerPresent(TRUE)
#define VMPIsVirtualMachine() VMProtectIsVirtualMachinePresent()
#define VMPIsValidCRC() VMProtectIsValidImageCRC()

#define ENCRYPT_STR(s) VMProtectDecryptStringA(skCrypt(s).encrypt())
#define DECRYPT_FREE(ptr) if(ptr) { VMProtectFreeString(ptr); ptr = nullptr; }

#define VM_CRITICAL_BEGIN() vmultra_lock
#define VM_CRITICAL_END() vmend

FunctionHider functionHider;
RuntimeFunctionEncryptor functionEncryptor;

std::string tm_to_readable_time(tm ctx);
static std::time_t string_to_timet(std::string timestamp);
static std::tm timet_to_tm(time_t timestamp);
void enableANSIColors();

static bool g_authSuccess = false;
static std::string g_sessionToken = skstr("");
static std::chrono::steady_clock::time_point g_lastCheck;
static std::atomic<bool> g_securityThreadActive{ false };

static inline void trimInPlace(std::string& s) {
    size_t start = 0; while (start < s.size() && (s[start]==' '||s[start]=='\t'||s[start]=='\r'||s[start]=='\n')) start++;
    size_t end = s.size(); while (end>start && (s[end-1]==' '||s[end-1]=='\t'||s[end-1]=='\r'||s[end-1]=='\n')) end--;
    if (start>0 || end<s.size()) s = s.substr(start, end-start);
}
static inline void toUpperInPlace(std::string& s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
}
static std::atomic<bool> g_authValidated{ false };
static std::atomic<bool> g_serviceAuth{ false }; // set true after service-based verification


const std::string compilation_date = skstr(__DATE__);
const std::string compilation_time = skstr(__TIME__);

// Basic app metadata and changelog for console display
static const char* kAppVersion = "1.0.0";
static const char* kAppName = "Obs";
static const char* kChangelog[] = {
    "- Initial console key login",
    "- Remember Me (auto-login)",
    "- Version and changelog display",
    "- Launch gated until key validated"
};

__forceinline void performAntiTamperCheck() {
    vmultra_lock
        if (g_serviceAuth) { vmend return; }
        if (!VMPIsValidCRC()) {
            char* msg = vmpstr(skstr("Invalid CRC detected"));
            exit(0xDEAD);
            DECRYPT_FREE(msg);
        }

    if (VMPIsDebuggerPresent()) {
        char* msg = vmpstr(skstr("Debugger detected"));
        DECRYPT_FREE(msg);
    }

    //if (VMPIsVirtualMachine()) {
                //}
    vmend
}

class SecureMemoryProtector {
private:
    uint32_t m_magic1 = 0xDEADBEEF;
    uint32_t m_magic2 = 0xCAFEBABE;
    uint32_t m_magic3 = 0x13371337;

public:
    __forceinline bool validateMemory() {
        vmultra_lock
            performAntiTamperCheck();

        if (m_magic1 != 0xDEADBEEF || m_magic2 != 0xCAFEBABE || m_magic3 != 0x13371337) {
            char* msg = vmpstr(skstr("Memory corruption detected"));
                      DECRYPT_FREE(msg);
        }

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)this, &mbi, sizeof(mbi))) {
            if (mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_EXECUTE_READWRITE) {
                exit(0xF00D);
            }
        }

        return true;
        vmend
    }

    __forceinline void scrambleMemory() {
        vmmutation_lock
            m_magic1 ^= GetTickCount();
        m_magic2 ^= GetCurrentThreadId();
        m_magic3 ^= GetCurrentProcessId();

        m_magic1 = _rotl(m_magic1, 7);
        m_magic2 = _rotr(m_magic2, 13);
        m_magic3 ^= m_magic1 + m_magic2;
        vmend
    }
};

static SecureMemoryProtector g_memoryProtector;

// Step 1: simple key storage
class KeyStorage {
public:
    static std::string path() {
        char* appData = nullptr; size_t len = 0;
        if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData) {
            std::string p = std::string(appData) + "\\obs_auth\\";
            free(appData);
            std::error_code ec; std::filesystem::create_directories(p, ec);
            return p + "key.txt";
        }
        return std::string("key.txt");
    }
    static bool save(const std::string& key) {
        if (key.empty()) return false;
        std::string s = key;
        for (size_t i = 0; i < s.size(); ++i) s[i] ^= (char)(0x55 + (i % 11));
        std::ofstream f(path(), std::ios::binary | std::ios::trunc);
        if (!f) return false; f << s; return true;
    }
    static std::string load() {
        std::ifstream f(path(), std::ios::binary);
        if (!f) return {};
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        for (size_t i = 0; i < s.size(); ++i) s[i] ^= (char)(0x55 + (i % 11));
        return s;
    }
    static void clear() {
        std::error_code ec; std::filesystem::remove(path(), ec);
    }
};

// Step 1: remote verification client (Discord bot service to be implemented)
// Endpoint contract (proposal):
//   POST http://127.0.0.1:3000/verify  body: {"key":"...","client":"obs","version":"1.0.0"}
//   Response JSON: {"valid":true/false, "status":"ok|revoked|not_found|expired", "message":"..."}
static std::string GetMachineGuid() {
    HKEY hKey = nullptr; std::string result;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256]; DWORD bufSize = sizeof(buf); DWORD type = 0;
        if (RegQueryValueExW(hKey, L"MachineGuid", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &bufSize) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
            char out[512] = {0};
            WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof(out)-1, nullptr, nullptr);
            result = out;
        }
        RegCloseKey(hKey);
    }
    if (result.empty()) result = "unknown-hwid";
    return result;
}

class CredentialManager {
private:
    static std::string getCredentialPath() {
        VM_CRITICAL_BEGIN()
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* appData = nullptr;
            size_t len = 0;
            if (_dupenv_s(&appData, &len, vmpstr(skstr("APPDATA"))) == 0 && appData != nullptr) {
                std::string path = std::string(appData) + vmpstr(skstr("\\sdwdwswd\\"));
                free(appData);

                char* debugMsg = vmpstr(skstr("[DEBUG] Using APPDATA path: "));
                std::cout << debugMsg << path << std::endl;
                DECRYPT_FREE(debugMsg);

                g_memoryProtector.scrambleMemory();
                return path;
            }

            char* fallbackMsg = vmpstr(skstr("[DEBUG] Using fallback path: C:\\dwdsawdsawd\\"));
            std::cout << fallbackMsg << std::endl;
            DECRYPT_FREE(fallbackMsg);

            return vmpstr(skstr("C:\\wdwadswd\\"));
        }
        catch (const std::exception& e) {
            char* errorMsg = vmpstr(skstr("[DEBUG] Exception in getCredentialPath: "));
            std::cout << errorMsg << e.what() << std::endl;
            DECRYPT_FREE(errorMsg);

            return vmpstr(skstr("C:\\wdsawdaswd\\"));
        }
        VM_CRITICAL_END()
    }

public:
    static bool saveCredentials(const std::string& username, const std::string& password) {
        vmultra_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting saveCredentials"));
            std::cout << startMsg << std::endl;
            DECRYPT_FREE(startMsg);

            if (username.empty() || password.empty()) {
                char* emptyMsg = vmpstr(skstr("[DEBUG] Empty username or password"));
                std::cout << emptyMsg << std::endl;
                DECRYPT_FREE(emptyMsg);
                return false;
            }

            std::string credPath = getCredentialPath();
            char* pathMsg = vmpstr(skstr("[DEBUG] Got credential path: "));
            std::cout << pathMsg << credPath << std::endl;
            DECRYPT_FREE(pathMsg);

            if (!std::filesystem::exists(credPath)) {
                char* createMsg = vmpstr(skstr("[DEBUG] Creating directory: "));
                std::cout << createMsg << credPath << std::endl;
                DECRYPT_FREE(createMsg);

                if (!std::filesystem::create_directories(credPath)) {
                    char* failMsg = vmpstr(skstr("[DEBUG] Failed to create directory"));
                    std::cout << failMsg << std::endl;
                    DECRYPT_FREE(failMsg);
                    return false;
                }
            }

                        std::string combined = username + vmpstr(skstr("|")) + password;
            for (size_t i = 0; i < combined.length(); i++) {
                combined[i] ^= (0x42 + (i % 16));
            }

            char* lengthMsg = vmpstr(skstr("[DEBUG] Combined credentials length: "));
            std::cout << lengthMsg << combined.length() << std::endl;
            DECRYPT_FREE(lengthMsg);

            std::string filePath = credPath + vmpstr(skstr("auth.txt"));
            char* writeMsg = vmpstr(skstr("[DEBUG] Writing to file: "));
            std::cout << writeMsg << filePath << std::endl;
            DECRYPT_FREE(writeMsg);

            std::ofstream file(filePath, std::ios::trunc | std::ios::binary);
            if (!file.is_open()) {
                char* openFailMsg = vmpstr(skstr("[DEBUG] Failed to open file for writing"));
                std::cout << openFailMsg << std::endl;
                DECRYPT_FREE(openFailMsg);
                return false;
            }

            file << combined;
            file.close();

            char* successMsg = vmpstr(skstr("[DEBUG] Successfully saved credentials"));
            std::cout << successMsg << std::endl;
            DECRYPT_FREE(successMsg);

            g_memoryProtector.scrambleMemory();
            return file.good();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in saveCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return false;
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in saveCredentials"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            return false;
        }
        vmend
    }

    static std::pair<std::string, std::string> loadCredentials() {
        vmvirt_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting loadCredentials"));
            std::cout << startMsg << std::endl;
            DECRYPT_FREE(startMsg);

            std::string credPath = getCredentialPath() + vmpstr(skstr("auth.txt"));
            char* loadMsg = vmpstr(skstr("[DEBUG] Loading from file: "));
            std::cout << loadMsg << credPath << std::endl;
            DECRYPT_FREE(loadMsg);

            if (!std::filesystem::exists(credPath)) {
                char* notExistMsg = vmpstr(skstr("[DEBUG] Credential file does not exist"));
                std::cout << notExistMsg << std::endl;
                DECRYPT_FREE(notExistMsg);
                return { skstr(""), skstr("") };
            }

            std::ifstream file(credPath, std::ios::binary);
            if (!file.is_open()) {
                char* openFailMsg = vmpstr(skstr("[DEBUG] Failed to open file for reading"));
                std::cout << openFailMsg << std::endl;
                DECRYPT_FREE(openFailMsg);
                return { skstr(""), skstr("") };
            }

            std::string combined;
            std::getline(file, combined);
            file.close();


            for (size_t i = 0; i < combined.length(); i++) {
                combined[i] ^= (0x42 + (i % 16));
            }

            char* lengthMsg = vmpstr(skstr("[DEBUG] Read combined credentials length: "));
            std::cout << lengthMsg << combined.length() << std::endl;
            DECRYPT_FREE(lengthMsg);

            if (combined.empty()) {
                char* emptyMsg = vmpstr(skstr("[DEBUG] Empty credentials file"));
                std::cout << emptyMsg << std::endl;
                DECRYPT_FREE(emptyMsg);
                return { skstr(""), skstr("") };
            }

            size_t pos = combined.find('|');
            if (pos == std::string::npos || pos == 0 || pos == combined.length() - 1) {
                char* invalidMsg = vmpstr(skstr("[DEBUG] Invalid credential format"));
                std::cout << invalidMsg << std::endl;
                DECRYPT_FREE(invalidMsg);
                return { skstr(""), skstr("") };
            }

            std::string username = combined.substr(0, pos);
            std::string password = combined.substr(pos + 1);

            char* successMsg = vmpstr(skstr("[DEBUG] Successfully loaded credentials for user: "));
            std::cout << successMsg << username << std::endl;
            DECRYPT_FREE(successMsg);

            if (username.empty() || password.empty()) {
                char* emptyParseMsg = vmpstr(skstr("[DEBUG] Empty username or password after parsing"));
                std::cout << emptyParseMsg << std::endl;
                DECRYPT_FREE(emptyParseMsg);
                return { skstr(""), skstr("") };
            }

            g_memoryProtector.scrambleMemory();
            return { username, password };
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in loadCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return { skstr(""), skstr("") };
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in loadCredentials"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            return { skstr(""), skstr("") };
        }
        vmend
    }

    static bool hasStoredCredentials() {
        vmultra_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            std::string credPath = getCredentialPath() + vmpstr(skstr("auth.txt"));
            bool exists = std::filesystem::exists(credPath);

            char* hasMsg = vmpstr(skstr("[DEBUG] hasStoredCredentials: "));
            char* yesMsg = vmpstr(skstr("YES"));
            char* noMsg = vmpstr(skstr("NO"));
            std::cout << hasMsg << (exists ? yesMsg : noMsg) << std::endl;
            DECRYPT_FREE(hasMsg);
            DECRYPT_FREE(yesMsg);
            DECRYPT_FREE(noMsg);

            g_memoryProtector.scrambleMemory();
            return exists;
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in hasStoredCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return false;
        }
        vmend
    }

    static void clearCredentials() {
        vmvirt_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* clearMsg = vmpstr(skstr("[DEBUG] Clearing credentials"));
            std::cout << clearMsg << std::endl;
            DECRYPT_FREE(clearMsg);

            std::string credPath = getCredentialPath() + vmpstr(skstr("auth.txt"));
            if (std::filesystem::exists(credPath)) {
                std::filesystem::remove(credPath);

                char* successMsg = vmpstr(skstr("[DEBUG] Credentials cleared successfully"));
                std::cout << successMsg << std::endl;
                DECRYPT_FREE(successMsg);
            }

            g_memoryProtector.scrambleMemory();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in clearCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in clearCredentials"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
        }
        vmend
    }
};

class SecurityManager {
private:
    static constexpr uint32_t SECURITY_MAGIC = 0xDEADBEEF;
    static constexpr uint32_t AUTH_MAGIC = 0xCAFEBABE;
    static constexpr uint32_t EXTRA_MAGIC = 0x13371337;

    mutable uint32_t m_securityToken = SECURITY_MAGIC;
    mutable uint32_t m_authToken = AUTH_MAGIC;
    mutable uint32_t m_extraToken = EXTRA_MAGIC;

    static inline std::map<std::string, uint32_t> functionChecksums;

public:
    __forceinline bool validateSecurity() const {
        vmultra_lock
            if (g_serviceAuth) { vmend return true; }
            performAntiTamperCheck();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting security validation"));
                     DECRYPT_FREE(startMsg);

            if (m_securityToken != SECURITY_MAGIC || m_authToken != AUTH_MAGIC || m_extraToken != EXTRA_MAGIC) {
                char* tokenFailMsg = vmpstr(skstr("[DEBUG] Security token validation failed"));
                       DECRYPT_FREE(tokenFailMsg);
                exit(0xDEAD);
            }

            char* tokenPassMsg = vmpstr(skstr("[DEBUG] Token validation passed"));
                    DECRYPT_FREE(tokenPassMsg);

            if (IsDebuggerPresent() || VMPIsDebuggerPresent()) {
                char* debuggerMsg = vmpstr(skstr("[DEBUG] Debugger detected"));
                           DECRYPT_FREE(debuggerMsg);
                exit(0xBEEF);
            }

            if (checkRemoteDebugger()) {
                char* remoteDebugMsg = vmpstr(skstr("[DEBUG] Remote debugger detected"));
                           DECRYPT_FREE(remoteDebugMsg);
                exit(0xCAFE);
            }

            if (checkProcessList()) {
                char* processMsg = vmpstr(skstr("[DEBUG] Suspicious process detected"));
                             DECRYPT_FREE(processMsg);
                exit(0xF00D);
            }

            if (checkMemoryBreakpoints()) {
                char* memBpMsg = vmpstr(skstr("[DEBUG] Memory breakpoints detected"));
                              DECRYPT_FREE(memBpMsg);
                exit(0xFEED);
            }

            if (checkHardwareBreakpoints()) {
                char* hwBpMsg = vmpstr(skstr("[DEBUG] Hardware breakpoints detected"));
                             DECRYPT_FREE(hwBpMsg);
                exit(0xBEEF);
            }

            char* basicPassMsg = vmpstr(skstr("[DEBUG] Basic debugger check passed"));
                       DECRYPT_FREE(basicPassMsg);

            char* completeMsg = vmpstr(skstr("[DEBUG] Security validation completed successfully"));
                      DECRYPT_FREE(completeMsg);

            return true;
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in validateSecurity: "));
                     DECRYPT_FREE(exceptionMsg);
            exit(0xDEAD);
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in validateSecurity"));
                      DECRYPT_FREE(unknownMsg);
            exit(0xDEAD);
        }
        vmend
    }

private:
    __forceinline bool checkRemoteDebugger() const {
        vmvirt_lock
            performAntiTamperCheck();

        BOOL isDebuggerPresent = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebuggerPresent);
        return isDebuggerPresent;
        vmend
    }

    __forceinline bool checkProcessList() const {
        vmultra_lock
            performAntiTamperCheck();

        const char* debuggerProcesses[] = {
            vmpstr(skstr("ollydbg.exe")), vmpstr(skstr("x64dbg.exe")), vmpstr(skstr("x32dbg.exe")),
            vmpstr(skstr("windbg.exe")), vmpstr(skstr("ida.exe")), vmpstr(skstr("ida64.exe")),
            vmpstr(skstr("cheatengine.exe")), vmpstr(skstr("processhacker.exe")), vmpstr(skstr("dnspy.exe")),
            vmpstr(skstr("pestudio.exe")), vmpstr(skstr("immunity")), vmpstr(skstr("hxd.exe")),
            vmpstr(skstr("010editor.exe")), vmpstr(skstr("regshot.exe")), vmpstr(skstr("apimonitor.exe")),
            vmpstr(skstr("procmon.exe")), vmpstr(skstr("filemon.exe")), vmpstr(skstr("regmon.exe")),
            vmpstr(skstr("wireshark.exe")), vmpstr(skstr("fiddler.exe")), vmpstr(skstr("httpanalyzer"))
        };

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &pe32)) {
            do {
                for (const auto& debugger : debuggerProcesses) {
                    if (strstr(reinterpret_cast<const char*>(pe32.szExeFile), debugger)) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32Next(snapshot, &pe32));
        }

        CloseHandle(snapshot);
        return false;
        vmend
    }

    __forceinline bool checkMemoryBreakpoints() const {
        vmvirt_lock
            performAntiTamperCheck();

        __try {
            BYTE* testMem = (BYTE*)VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (!testMem) return false;

            memset(testMem, 0xCC, 0x1000);

            for (int i = 0; i < 0x1000; i++) {
                if (testMem[i] != 0xCC) {
                    VirtualFree(testMem, 0, MEM_RELEASE);
                    return true;
                }
            }

            VirtualFree(testMem, 0, MEM_RELEASE);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return true;
        }
        return false;
        vmend
    }

    __forceinline bool checkHardwareBreakpoints() const {
        vmultra_lock
            performAntiTamperCheck();

        CONTEXT context;
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(GetCurrentThread(), &context);

        return (context.Dr0 || context.Dr1 || context.Dr2 || context.Dr3);
        vmend
    }

    __forceinline bool checkModuleIntegrity() const {
        vmvirt_lock
            performAntiTamperCheck();

        HMODULE hMod = GetModuleHandle(NULL);
        if (!hMod) return true;

        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
            return true;
        }

        DWORD oldProtect;
        if (!VirtualProtect(modInfo.lpBaseOfDll, modInfo.SizeOfImage, PAGE_READONLY, &oldProtect)) {
            return true;
        }

        VirtualProtect(modInfo.lpBaseOfDll, modInfo.SizeOfImage, oldProtect, &oldProtect);
        return false;
        vmend
    }

    __forceinline bool checkThreadCount() const {
        vmultra_lock
            performAntiTamperCheck();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);

        int threadCount = 0;
        DWORD currentPID = GetCurrentProcessId();

        if (Thread32First(snapshot, &te32)) {
            do {
                if (te32.th32OwnerProcessID == currentPID) {
                    threadCount++;
                }
            } while (Thread32Next(snapshot, &te32));
        }

        CloseHandle(snapshot);
        return threadCount > 100;
        vmend
    }

    __forceinline bool checkHookDetection() const {
        vmvirt_lock
            performAntiTamperCheck();

        HMODULE kernel32 = GetModuleHandleA(vmpstr(skstr("kernel32.dll")));
        HMODULE ntdll = GetModuleHandleA(vmpstr(skstr("ntdll.dll")));

        if (!kernel32 || !ntdll) return true;

        FARPROC funcs[] = {
            GetProcAddress(kernel32, vmpstr(skstr("CreateFileA"))),
            GetProcAddress(kernel32, vmpstr(skstr("WriteFile"))),
            GetProcAddress(kernel32, vmpstr(skstr("ReadFile"))),
            GetProcAddress(ntdll, vmpstr(skstr("NtQueryInformationProcess"))),
            GetProcAddress(ntdll, vmpstr(skstr("NtSetInformationThread")))
        };

        for (auto func : funcs) {
            if (!func) continue;

            BYTE* funcPtr = (BYTE*)func;
            if (IsBadReadPtr(funcPtr, 1)) continue;

            if (funcPtr[0] == 0xE9 || funcPtr[0] == 0xEB || funcPtr[0] == 0x68) {
                return true;
            }
        }

        return false;
        vmend
    }

    __forceinline bool checkDLLInjection() const {
        vmultra_lock
            performAntiTamperCheck();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);

        int moduleCount = 0;
        if (Module32First(snapshot, &me32)) {
            do {
                moduleCount++;

                const char* suspiciousDlls[] = {
                    vmpstr(skstr("inject")), vmpstr(skstr("hook")), vmpstr(skstr("detour")),
                    vmpstr(skstr("patch")), vmpstr(skstr("hack")), vmpstr(skstr("cheat")),
                    vmpstr(skstr("mod")), vmpstr(skstr("speedhack")), vmpstr(skstr("debug")),
                    vmpstr(skstr("monitor"))
                };

                for (const auto& suspicious : suspiciousDlls) {
                    if (strstr(reinterpret_cast<const char*>(me32.szModule), suspicious)) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Module32Next(snapshot, &me32));
        }

        CloseHandle(snapshot);
        return moduleCount > 200;
        vmend
    }
};

static SecurityManager g_securityMgr;

using namespace KeyAuth;

std::string getDecryptedName() {
    vmultra_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("yo");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

std::string getDecryptedOwnerID() {
    vmvirt_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("Rwv1ehf0hR");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

std::string getDecryptedVersion() {
    vmultra_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("1.0");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

std::string getDecryptedURL() {
    vmvirt_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("https://keyauth.win/api/1.3/");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

__forceinline void performAdvancedAntiDebug() {
    vmultra_lock
        if (g_serviceAuth) { vmend return; }
        performAntiTamperCheck();

    if (IsDebuggerPresent() || VMPIsDebuggerPresent()) {
        char* msg = vmpstr(skstr("Debug presence detected"));
            //exit(0xDEADBEEF);
        DECRYPT_FREE(msg);
    }
    BOOL isRemoteDebuggerPresent = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebuggerPresent);
    if (isRemoteDebuggerPresent) {
        exit(0xFEEDFACE);
    }

    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    volatile int dummy = 0;
    for (int i = 0; i < 1000; i++) {
        dummy += i;
    }

    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

    if (elapsed > 0.01) {
        exit(0xDEADC0DE);
    }

    vmend
}

void securityValidationThread() {
    vmultra_lock
        performAdvancedAntiDebug();

    char* startMsg = vmpstr(skstr("[DEBUG] Security validation thread started"));
    std::cout << startMsg << std::endl;
    DECRYPT_FREE(startMsg);

    g_securityThreadActive = true;

    while (g_securityThreadActive && g_authValidated) {
        char* loopMsg = vmpstr(skstr("[DEBUG] Security validation loop iteration"));
        std::cout << loopMsg << std::endl;
        DECRYPT_FREE(loopMsg);

        performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        if (!g_securityMgr.validateSecurity()) {
            char* secFailMsg = vmpstr(skstr("[DEBUG] Security validation failed in thread - exiting"));
                DECRYPT_FREE(secFailMsg);
            g_authValidated = false;
            exit(0xDEADBEEF);
        }

        char* secPassMsg = vmpstr(skstr("[DEBUG] Security validation passed in thread"));
           DECRYPT_FREE(secPassMsg);

        // Session validation removed - using key-based authentication

        char* sessionPassMsg = vmpstr(skstr("[DEBUG] Session validation passed"));
             DECRYPT_FREE(sessionPassMsg);

        if (!VMPIsValidCRC()) {
            char* crcFailMsg = vmpstr(skstr("[DEBUG] CRC validation failed - exiting"));
                    DECRYPT_FREE(crcFailMsg);
            exit(0x13371337);
        }

        char* crcPassMsg = vmpstr(skstr("[DEBUG] CRC validation passed"));
              DECRYPT_FREE(crcPassMsg);

        char* vmPassMsg = vmpstr(skstr("[DEBUG] VM checks passed"));
           DECRYPT_FREE(vmPassMsg);

        HMODULE ntdll = GetModuleHandleA(vmpstr(skstr("ntdll.dll")));
        if (ntdll) {
            HANDLE currentProcess = GetCurrentProcess();
            DWORD processFlags = 0;
            typedef NTSTATUS(WINAPI* pNtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
            pNtQueryInformationProcess NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(ntdll, vmpstr(skstr("NtQueryInformationProcess")));

            if (NtQueryInformationProcess) {
                NTSTATUS status = NtQueryInformationProcess(currentProcess, 0x1f, &processFlags, sizeof(processFlags), NULL);
                if (NT_SUCCESS(status) && (processFlags & 0x1) && IsDebuggerPresent()) {
                    char* debugFlagMsg = vmpstr(skstr("[DEBUG] Process debug flag detected with active debugger - exiting"));
                                  DECRYPT_FREE(debugFlagMsg);
                    exit(0xF00DFACE);
                }
                char* debugCheckMsg = vmpstr(skstr("[DEBUG] Process debug flag check passed"));
                             DECRYPT_FREE(debugCheckMsg);
            }
            else {
                char* notAvailMsg = vmpstr(skstr("[DEBUG] NtQueryInformationProcess not available - skipping debug flag check"));
                          DECRYPT_FREE(notAvailMsg);
            }
        }
        else {
            char* ntdllFailMsg = vmpstr(skstr("[DEBUG] ntdll not available - skipping debug flag check"));
                   DECRYPT_FREE(ntdllFailMsg);
        }

        if (IsDebuggerPresent()) {
            char* basicDebugMsg = vmpstr(skstr("[DEBUG] Basic debugger detected - exiting"));
                   DECRYPT_FREE(basicDebugMsg);
            exit(0xBEEFCAFE);
        }

        BOOL isRemoteDebuggerPresent = FALSE;
        if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebuggerPresent) && isRemoteDebuggerPresent) {
            char* remoteDebugMsg = vmpstr(skstr("[DEBUG] Remote debugger detected - exiting"));
                    DECRYPT_FREE(remoteDebugMsg);
            exit(0xFACEFEED);
        }

        static DWORD lastTickCount = GetTickCount();
        DWORD currentTick = GetTickCount();
        if (currentTick < lastTickCount || (currentTick - lastTickCount) > 30000) {
            char* tickMsg = vmpstr(skstr("[DEBUG] Tick count anomaly detected - exiting"));
                     DECRYPT_FREE(tickMsg);
            exit(0xDEADFACE);
        }
        lastTickCount = currentTick;

        char* sleepMsg = vmpstr(skstr("[DEBUG] Security validation thread sleeping for 15 seconds"));
              DECRYPT_FREE(sleepMsg);

        g_memoryProtector.scrambleMemory();
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Increased sleep to reduce CPU usage
    }

    char* exitMsg = vmpstr(skstr("[DEBUG] Security validation thread exiting"));
     DECRYPT_FREE(exitMsg);
    vmend
}

bool validateFeatureAccess(const std::string& feature) {
    vmvirt_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    // If authenticated via key-based system, allow all features
    if (g_serviceAuth) {
        vmend
        return true;
    }
    vmend
    return false; // Deny access if not authenticated
}

void enableANSIColors() {
    vmvirt_lock
        performAntiTamperCheck();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    vmend
}

std::string tm_to_readable_time(tm ctx) {
    vmultra_lock
        performAntiTamperCheck();

    char buffer[80];
    strftime(buffer, sizeof(buffer), vmpstr(skstr("%a %m/%d/%y %H:%M:%S %Z")), &ctx);
    g_memoryProtector.scrambleMemory();
    return std::string(buffer);
    vmend
}

static std::time_t string_to_timet(std::string timestamp) {
    vmvirt_lock
        performAntiTamperCheck();

    auto cv = strtol(timestamp.c_str(), NULL, 10);
    g_memoryProtector.scrambleMemory();
    return (time_t)cv;
    vmend
}

static std::tm timet_to_tm(time_t timestamp) {
    vmultra_lock
        performAntiTamperCheck();

    std::tm context;
    localtime_s(&context, &timestamp);
    g_memoryProtector.scrambleMemory();
    return context;
    vmend
}

bool EnsureDirectoryExists(const std::string& path) {
    vmultra_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    try {
        if (!std::filesystem::exists(path)) {
            bool result = std::filesystem::create_directories(path);
            g_memoryProtector.scrambleMemory();
            return result;
        }
        return true;
    }
    catch (std::exception& e) {
        char* errorMsg = vmpstr(skstr("Error creating directory: "));
        std::cerr << errorMsg << e.what() << std::endl;
        DECRYPT_FREE(errorMsg);
        return false;
    }
    vmend
}

bool VerifyCodeIntegrity() {
    vmultra_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    if (!VMPIsValidCRC()) {
        return false;
    }

    if (VMPIsDebuggerPresent()) {
        return false;
    }

    HMODULE hMod = GetModuleHandle(NULL);
    if (hMod) {
        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMod;
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dosHeader->e_lfanew);

        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
            ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
    }

    g_memoryProtector.scrambleMemory();
    return true;
    vmend
}

void heartbeat() {
    vmvirt_lock
        performAdvancedAntiDebug();
    // If we authenticated via the new service flow, run a lightweight heartbeat that never exits the process
    if (g_serviceAuth) {
        for(;;){
            globals::firstreceived = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        vmend
        return;
    }
    char* startMsg = vmpstr(skstr("[DEBUG] Heartbeat thread starting"));
      DECRYPT_FREE(startMsg);


    char* initCompleteMsg = vmpstr(skstr("[DEBUG] Heartbeat initialization complete"));
     DECRYPT_FREE(initCompleteMsg);

    static std::chrono::steady_clock::time_point lastAuthCheck = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastMemoryCheck = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastProcessCheck = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastIntegrityCheck = std::chrono::steady_clock::now();
    static uint32_t authChecksum = 0xDEADBEEF;
    static bool initialAuth = g_authValidated;
    static uint32_t heartbeatCounter = 0;

    while (true) {
        heartbeatCounter++;
            g_memoryProtector.validateMemory();

        char* loopMsg = vmpstr(skstr("[DEBUG] Heartbeat loop iteration"));
         DECRYPT_FREE(loopMsg);

        globals::firstreceived = true;
        auto currentTime = std::chrono::steady_clock::now();

        if (!g_authValidated || !g_authSuccess) {
            char* authFailMsg = vmpstr(skstr("[DEBUG] Auth validation failed in heartbeat - exiting"));
                   DECRYPT_FREE(authFailMsg);
            exit(0xDEADBEEF);
        }

        if (g_authValidated != initialAuth) {
            char* authChangeMsg = vmpstr(skstr("[DEBUG] Auth state changed - exiting"));
                     DECRYPT_FREE(authChangeMsg);
            exit(0xCAFEBABE);
        }
        if (heartbeatCounter % 10 == 0) {
            LARGE_INTEGER start, end, freq;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&start);

            volatile int dummy = 0;
            for (int i = 0; i < 100; i++) {
                dummy += i * heartbeatCounter;
            }

            QueryPerformanceCounter(&end);
            double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

            if (elapsed > 0.001) {
                char* timingMsg = vmpstr(skstr("[DEBUG] Timing anomaly detected - exiting"));
                             DECRYPT_FREE(timingMsg);
            exit(5);
            }
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastAuthCheck).count() >= 30) {
            char* authCheckMsg = vmpstr(skstr("[DEBUG] Running periodic auth check"));
                    DECRYPT_FREE(authCheckMsg);

            if (!g_securityMgr.validateSecurity()) {
                char* secFailMsg = vmpstr(skstr("[DEBUG] Security validation failed in heartbeat - exiting"));
                           DECRYPT_FREE(secFailMsg);
            exit(34);
            }

            if (g_sessionToken.empty() || g_sessionToken.length() < 10) {
                char* tokenFailMsg = vmpstr(skstr("[DEBUG] Invalid session token - exiting"));
                            DECRYPT_FREE(tokenFailMsg);
            exit(21);
            }

            static uint32_t expectedChecksum = 0xDEADBEEF;

            if (authChecksum != expectedChecksum) {
                char* checksumFailMsg = vmpstr(skstr("[DEBUG] Checksum mismatch - exiting"));
                          DECRYPT_FREE(checksumFailMsg);
            exit(999);
            }

            lastAuthCheck = currentTime;
            char* authPassMsg = vmpstr(skstr("[DEBUG] Periodic auth check passed"));
                    DECRYPT_FREE(authPassMsg);
        }

        static uint32_t expectedChecksum = 0xDEADBEEF;

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastAuthCheck).count() >= 30) {
            char* authCheckMsg = vmpstr(skstr("[DEBUG] Running periodic auth check"));
            DECRYPT_FREE(authCheckMsg);


            g_authValidated = true;
            g_authSuccess = true;
            g_sessionToken = "randomshitidfkrah1244662";
            authChecksum = expectedChecksum;

            lastAuthCheck = currentTime;

            char* authPassMsg = vmpstr(skstr("[DEBUG] Periodic auth check passed [mango mango mango]"));
            DECRYPT_FREE(authPassMsg);
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastProcessCheck).count() >= 20) {
            char* procCheckMsg = vmpstr(skstr("[DEBUG] Running process check"));
                      DECRYPT_FREE(procCheckMsg);

            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32);

                const char* suspiciousProcesses[] = {
                    vmpstr(skstr("cheatengine")), vmpstr(skstr("processhacker")), vmpstr(skstr("x64dbg")),
                    vmpstr(skstr("x32dbg")), vmpstr(skstr("ollydbg")), vmpstr(skstr("ida")),
                    vmpstr(skstr("windbg")), vmpstr(skstr("immunity")), vmpstr(skstr("pestudio")),
                    vmpstr(skstr("dnspy")), vmpstr(skstr("reflexil")), vmpstr(skstr("megadumper")),
                    vmpstr(skstr("hxd")), vmpstr(skstr("010editor")), vmpstr(skstr("apimonitor")),
                    vmpstr(skstr("detours")), vmpstr(skstr("wireshark")), vmpstr(skstr("fiddler")),
                    vmpstr(skstr("procmon")), vmpstr(skstr("regshot")), vmpstr(skstr("autoruns"))
                };

                if (Process32First(snapshot, &pe32)) {
                    do {
                        for (const auto& suspicious : suspiciousProcesses) {
                            if (strstr(reinterpret_cast<const char*>(pe32.szExeFile), suspicious)) {
                                char* procDetectMsg = vmpstr(skstr("[DEBUG] Suspicious process detected: "));
                                std::cout << procDetectMsg << pe32.szExeFile << skstr(" - exiting") << std::endl;
                                DECRYPT_FREE(procDetectMsg);
                                CloseHandle(snapshot);
            exit(0x0);
                            }
                        }
                    } while (Process32Next(snapshot, &pe32));
                }
                CloseHandle(snapshot);
            }

            lastProcessCheck = currentTime;
            char* procPassMsg = vmpstr(skstr("[DEBUG] Process check passed"));
                      DECRYPT_FREE(procPassMsg);
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastIntegrityCheck).count() >= 60) {
            char* integrityMsg = vmpstr(skstr("[DEBUG] Running integrity checks"));
                       DECRYPT_FREE(integrityMsg);

            // Fix: check directory, not a file path
            if (!EnsureDirectoryExists(vmpstr(skstr("C:\\Obs\\login\\")))) {
                char* dirFailMsg = vmpstr(skstr("[DEBUG] Directory check failed - exiting"));
                            DECRYPT_FREE(dirFailMsg);
                exit(0xD0);
            }

            if (!VerifyCodeIntegrity()) {
                char* codeFailMsg = vmpstr(skstr("[DEBUG] Code integrity check failed - exiting"));
                          DECRYPT_FREE(codeFailMsg);
                exit(0xC0);
            }

            HANDLE moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
            if (moduleSnapshot != INVALID_HANDLE_VALUE) {
                MODULEENTRY32 me32;
                me32.dwSize = sizeof(MODULEENTRY32);

                int moduleCount = 0;
                if (Module32First(moduleSnapshot, &me32)) {
                    do {
                        moduleCount++;

                        const char* suspiciousModules[] = {
                            vmpstr(skstr("inject")), vmpstr(skstr("hook")), vmpstr(skstr("detour")),
                            vmpstr(skstr("patch")), vmpstr(skstr("mod")), vmpstr(skstr("hack"))
                        };

                        for (const auto& suspicious : suspiciousModules) {
                            if (strstr(reinterpret_cast<const char*>(me32.szModule), suspicious)) {
                                char* moduleDetectMsg = vmpstr(skstr("[DEBUG] Suspicious module detected: "));
                                DECRYPT_FREE(moduleDetectMsg);
                                CloseHandle(moduleSnapshot);
            exit(0x0);
                            }
                        }
                    } while (Module32Next(moduleSnapshot, &me32));
                }
                CloseHandle(moduleSnapshot);

                if (moduleCount > 150) {
                    char* moduleCountMsg = vmpstr(skstr("[DEBUG] Too many modules loaded - exiting"));
                    DECRYPT_FREE(moduleCountMsg);
                    exit(0x0);
                }
            }

            lastIntegrityCheck = currentTime;
            char* integrityPassMsg = vmpstr(skstr("[DEBUG] Integrity checks passed"));
                   DECRYPT_FREE(integrityPassMsg);
        }

        char* codeIntegrityMsg = vmpstr(skstr("[DEBUG] Code integrity checks passed"));
             DECRYPT_FREE(codeIntegrityMsg);

         HMODULE kernel32 = GetModuleHandleA(vmpstr(skstr("kernel32.dll")));
        if (!kernel32) {
            char* k32FailMsg = vmpstr(skstr("[DEBUG] Failed to get kernel32 handle - exiting"));
                    DECRYPT_FREE(k32FailMsg);
            exit(0x0);
        }

        FARPROC isDebuggerPresent = GetProcAddress(kernel32, vmpstr(skstr("IsDebuggerPresent")));
        if (!isDebuggerPresent) {
            char* idpFailMsg = vmpstr(skstr("[DEBUG] Failed to get IsDebuggerPresent - exiting"));
                    DECRYPT_FREE(idpFailMsg);
            exit(0x0);
        }

        BYTE* funcPtr = (BYTE*)isDebuggerPresent;
        if (funcPtr[0] == 0xE9 || funcPtr[0] == 0xEB || funcPtr[0] == 0x68 || funcPtr[0] == 0xFF) {
            char* hookMsg = vmpstr(skstr("[DEBUG] API hook detected - exiting"));
                    DECRYPT_FREE(hookMsg);
            exit(0x0);
        }

        typedef BOOL(WINAPI* pIsDebuggerPresent)();
        if (((pIsDebuggerPresent)isDebuggerPresent)()) {
            char* debugDetectMsg = vmpstr(skstr("[DEBUG] Debugger detected - exiting"));
                   DECRYPT_FREE(debugDetectMsg);
            exit(0x0);
        }

         SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        if (sysInfo.dwNumberOfProcessors < 2) {
            char* emuMsg = vmpstr(skstr("[DEBUG] Potential emulation detected - exiting"));
        
            DECRYPT_FREE(emuMsg);
            exit(0xE0);
        }

        char* sleepMsg = vmpstr(skstr("[DEBUG] Heartbeat sleeping for 5 seconds"));
        DECRYPT_FREE(sleepMsg);

        g_memoryProtector.scrambleMemory();
        std::this_thread::sleep_for(std::chrono::seconds(30)); // Increased sleep to reduce CPU usage
    }

    char* exitMsg = vmpstr(skstr("[DEBUG] Security validation thread exiting"));
     DECRYPT_FREE(exitMsg);
    vmend
}

static void ShowChangelog() {
    system("cls");
    enableANSIColors();
    std::cout << "==== " << kAppName << " Changelog ====\n";
    for (auto line : kChangelog) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\nPress Enter to return..." << std::endl;
    std::string tmp; std::getline(std::cin, tmp);
}

bool performAuthentication() {
    // Hide the initial console window
    ShowWindow(GetConsoleWindow(), SW_SHOW);

    // Initialize KeyAuth API directly
    KeyAuth::api keyauth(getDecryptedName(), getDecryptedOwnerID(), getDecryptedVersion(), getDecryptedURL(), "");

    // Initialize KeyAuth
    keyauth.init();
    if (!keyauth.response.success) {
        char* initFailMsg = vmpstr(skstr("Failed to initialize KeyAuth"));
        std::cout << initFailMsg << std::endl;
        DECRYPT_FREE(initFailMsg);
        return false;
    }

    // Start menu
    for (;;) {
        system("cls");
        enableANSIColors();
        HWND chwnd = GetConsoleWindow();
        if (chwnd) {
            // Show the console window for the custom interface
            ShowWindow(chwnd, SW_SHOW);
            LONG ex = GetWindowLong(chwnd, GWL_EXSTYLE);
            SetWindowLong(chwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
            SetLayeredWindowAttributes(chwnd, 0, 220, LWA_ALPHA);
            // Disable resizing and maximize
            LONG style = GetWindowLong(chwnd, GWL_STYLE);
            style &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX);
            SetWindowLong(chwnd, GWL_STYLE, style);
            SetWindowPos(chwnd, nullptr, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
            // Center window on screen
            RECT r{}; GetWindowRect(chwnd, &r);
            int w = r.right - r.left, h = r.bottom - r.top;
            int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
            int cx = (sw - w) / 2, cy = (sh - h) / 2; if (cx < 0) cx = 0; if (cy < 0) cy = 0;
            MoveWindow(chwnd, cx, cy, w, h, TRUE);
        }

        // Hide cursor during fake loading
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cci{}; bool haveCCI = false; CONSOLE_CURSOR_INFO oldCCI{};
        if (hout != INVALID_HANDLE_VALUE && GetConsoleCursorInfo(hout, &oldCCI)) { haveCCI = true; cci = oldCCI; cci.bVisible = FALSE; SetConsoleCursorInfo(hout, &cci); }

        // Helpers to center output
        auto getCols = [&]() -> int {
            CONSOLE_SCREEN_BUFFER_INFO ci{}; if (hout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hout, &ci)) {
                return (ci.srWindow.Right - ci.srWindow.Left + 1);
            }
            return 100;
        };
        auto getRows = [&]() -> int {
            CONSOLE_SCREEN_BUFFER_INFO ci{}; if (hout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hout, &ci)) {
                return (ci.srWindow.Bottom - ci.srWindow.Top + 1);
            }
            return 30;
        };
        auto getTop = [&]() -> int {
            CONSOLE_SCREEN_BUFFER_INFO ci{}; if (hout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hout, &ci)) {
                return ci.srWindow.Top;
            }
            return 0;
        };
        auto setRow = [&](int row){ if (hout != INVALID_HANDLE_VALUE) { CONSOLE_SCREEN_BUFFER_INFO ci{}; if (GetConsoleScreenBufferInfo(hout,&ci)) { COORD pos{ ci.srWindow.Left, (SHORT)row }; SetConsoleCursorPosition(hout, pos); } } };
        auto centerPrint = [&](const std::string& s){ int cols = getCols(); int pad = (int)((cols - (int)s.size())/2); if (pad < 0) pad = 0; std::cout << std::string(pad, ' ') << s << "\n"; };
        auto centerType = [&](const std::string& s, int perCharMs){
            int cols = getCols(); int pad = (int)((cols - (int)s.size())/2); if (pad < 0) pad = 0;
            std::cout << std::string(pad, ' ');
            if (perCharMs <= 1) {
                // Windows Sleep(1) often ~15ms; print in chunks to appear fast while yielding
                const int chunk = 4; int i = 0;
                for (char ch : s) {
                    std::cout << ch << std::flush;
                    if ((++i % chunk) == 0) Sleep(1);
                }
            } else {
                for (char ch : s) { std::cout << ch << std::flush; Sleep(perCharMs); }
            }
            std::cout << "\n";
        };

        // Bigger ASCII banner for frizy (5 lines), animated reveal, centered vertically without scrolling
        const char* banner[] = {
            "  _        _     ___   _   _ ",
            " | |      / \\   |_ _| | \\ | |",
            " | |     / _ \\   | |  |  \\| |",
            " | |___ / ___ \\  | |  | |\\  |",
            " |_____/_/   \\_\\|___| |_| \\\u005f|"
        };
        int rows = getRows(); int top = getTop(); int bannerLines = 5; int startRow = top + (rows/2) - (bannerLines/2) - 5; if (startRow < top) startRow = top;
        setRow(startRow);
        for (int i = 0; i < bannerLines; ++i) { centerType(banner[i], 1); }

        // Longer fake loading with spinning indicator and padded text (avoid trailing artifacts)
        const char* spinner[] = {"|", "/", "-", "\\"};
        const char* steps[] = {
            "Starting up...",
            "Loading assets...",
            "Preparing UI...",
            "Connecting...",
            "Almost ready...",
            "Ready."
        };
        int spinIdx = 0;
        // Determine default console attributes to restore later
        WORD defAttr = 0; CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (hout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hout, &csbi)) defAttr = csbi.wAttributes; else defAttr = (FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
        const WORD spinnerAttr = (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);  // bright cyan
        const WORD stepAttr    = defAttr;                                                     // no color for steps
        // Show each step for a fixed number of frames, but show 'Ready.' only once
        const int framesPerStep = 10;
        const int stepsCount = (int)(sizeof(steps)/sizeof(steps[0]));
        const int totalFrames = framesPerStep * (stepsCount - 1) + 1; // single frame for last step
        for (int f = 0; f < totalFrames; ++f) {
            int idx = f / framesPerStep;
            if (idx >= stepsCount - 1) idx = stepsCount - 1; // pin to 'Ready.'
            const char* step = steps[idx];
            std::string content = std::string(" ") + spinner[spinIdx] + "  " + step;
            int cols = getCols(); int pad = (int)((cols - (int)content.size())/2); if (pad < 0) pad = 0;
            // position cursor at a fixed row under the banner to avoid scrolling
            setRow(startRow + bannerLines + 1);
            std::cout << std::string(pad, ' ');
            if (hout != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hout, spinnerAttr);
            std::cout << spinner[spinIdx] << "  ";
            if (hout != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hout, stepAttr);
            // Write step and pad remainder of the line to clear leftovers from longer previous text
            std::cout << step;
            int stepLen = (int)strlen(step);
            int printed = 1 /*spinner*/ + 2 /*spaces*/ + stepLen;
            int rem = cols - pad - printed; if (rem < 0) rem = 0;
            std::cout << std::string(rem, ' ') << std::flush;
            if (hout != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hout, defAttr);
            spinIdx = (spinIdx + 1) % 4;
            Sleep(75);
        }
        // hard clear spinner line and add a short pause before showing options
        int cols = getCols(); setRow(startRow + bannerLines + 1); std::cout << std::string(cols, ' ');
        Sleep(150);

        // Restore cursor visibility
        if (haveCCI) SetConsoleCursorInfo(hout, &oldCCI);

        // Show clean, numbered options in a centered bordered layout (typewriter), positioned close to banner
        auto makeBoxLine = [&](const std::string& inner){
            const int width = 30; // inner width for content
        int pad = width - (int)inner.size(); if (pad < 0) pad = 0;
        // Left-aligned inner text inside a fixed-width box
        return std::string("|") + "  " + inner + std::string(pad, ' ') + "  |";
        };
        setRow(startRow + bannerLines + 2);
        centerType("+--------------------------------+", 0);
        centerType(makeBoxLine("1) Login"), 0);
        centerType(makeBoxLine("2) Register"), 0);
        centerType(makeBoxLine("3) Exit"), 0);
        centerType("+--------------------------------+", 0);
        // Centered inline prompt; read a single key so the digit appears right after the prompt
        {
            std::string prompt = "Select [1-3]: ";
            int cols2 = getCols(); int pad2 = (int)((cols2 - (int)prompt.size())/2); if (pad2 < 0) pad2 = 0;
            std::cout << std::string(pad2, ' ') << prompt << std::flush;
            int ch = std::cin.get();
            if (ch == '\\r' || ch == '\\n') ch = std::cin.get();
            char c = (char)ch;
            if (c == '1') { system("cls"); break; }
            if (c == '2') { system("cls"); ShowChangelog(); continue; }
            if (c == '3') { system("cls"); exit(0); }
        }
    }

    // Auto-login if saved
    std::string saved = KeyStorage::load(); trimInPlace(saved); toUpperInPlace(saved);
    std::string key;
    if (!saved.empty()) {
        std::cout << "Found saved key. Signing you in..." << std::endl;
        keyauth.license(saved);
        if (keyauth.response.success) {
            key = saved;
        } else {
            std::cout << "Could not sign in." << std::endl;
            if (keyauth.response.message.find("revoked") != std::string::npos ||
                keyauth.response.message.find("not_found") != std::string::npos ||
                keyauth.response.message.find("expired") != std::string::npos ||
                keyauth.response.message.find("hwid_mismatch") != std::string::npos) {
                KeyStorage::clear();
            }
        }
    }

    while (key.empty()) {
        std::cout << "Enter key: ";
        std::getline(std::cin, key);
        trimInPlace(key); toUpperInPlace(key);
        if (key.empty()) { std::cout << "Please enter a key." << std::endl; continue; }
        std::cout << "Checking...\n";
        keyauth.license(key);
        if (!keyauth.response.success) {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::cout << "Invalid key: " << keyauth.response.message << std::endl;
            if (hOut != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            key.clear();
            continue;
        }
        // Success feedback and ask remember me
        HANDLE hOutOk = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOutOk != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hOutOk, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "Key accepted." << std::endl;
        if (hOutOk != INVALID_HANDLE_VALUE) SetConsoleTextAttribute(hOutOk, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        // Ask remember me with explicit [y/N] handling
        for (;;) {
            std::cout << "Save key for next time? [Y/N] ";
            std::string ans; std::getline(std::cin, ans);
            trimInPlace(ans);
            if (ans.empty() || ans[0]=='n' || ans[0]=='N') { break; }
            if (ans[0]=='y' || ans[0]=='Y') { KeyStorage::save(key); break; }
            // otherwise re-prompt until a valid response
        }
    }

    // Gate launch until user confirms
    std::cout << "Press 1 to start." << std::endl;
    for(;;){ int ch = std::cin.get(); if (ch=='1') break; }

    g_authValidated = true;
    g_authSuccess = true;
    g_serviceAuth = true;
    globals::firstreceived = true;
    globals::instances::username = keyauth.user_data.username;
    g_sessionToken = key + "_" + std::to_string(GetTickCount64());
    return true;
}
int jsdlfhjsdlkfhjssdfjsdlfjhlsdjfsdjflkjsdlfjslghdshga() {
    auto mainInitFunction = []() -> int {
        using namespace frizy::misc; // Bring frizy::misc into scope
        try {
            char* mainInitMsg = vmpstr(skstr("[DEBUG] Main initialization starting"));

            DECRYPT_FREE(mainInitMsg);

            enableANSIColors();
            HWND hwnd = GetConsoleWindow();
            // Set console title to the current EXE name
            wchar_t modulePath[MAX_PATH]{};
            GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
            wchar_t* base = wcsrchr(modulePath, L'\\');
            wchar_t* exeMutable = base ? (base + 1) : modulePath;
            // Strip .exe extension (case-insensitive)
            size_t len = wcslen(exeMutable);
            if (len >= 4) {
                wchar_t* ext = exeMutable + (len - 4);
                if (_wcsicmp(ext, L".exe") == 0) *ext = L'\0';
            }
            SetWindowTextW(hwnd, exeMutable);

            char* consoleSetupMsg = vmpstr(skstr("[DEBUG] Console setup complete"));

            DECRYPT_FREE(consoleSetupMsg);

            // Bypass authentication
            g_authValidated = true;
            g_authSuccess = true;
            g_serviceAuth = true;
            globals::firstreceived = true;
            globals::instances::username = "bypassed_user";
            g_sessionToken = "bypassed_token_" + std::to_string(GetTickCount64());
            
            char* authBypassMsg = vmpstr(skstr("[DEBUG] Authentication bypassed - proceeding to launch"));
            std::cout << authBypassMsg << std::endl;
            DECRYPT_FREE(authBypassMsg);

            // Flush any pending console input and detach console so engine can't receive ENTER to terminate
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            if (hIn != INVALID_HANDLE_VALUE) {
                FlushConsoleInputBuffer(hIn);
            }
            ShowWindow(hwnd, SW_HIDE);
            // Fade-out animation
            // LONG ex2 = GetWindowLong(hwnd, GWL_EXSTYLE);
            // SetWindowLong(hwnd, GWL_EXSTYLE, ex2 | WS_EX_LAYERED);
            // for (int a = 220; a >= 0; a -= 15) {
            //   SetLayeredWindowAttributes(hwnd, 0, (BYTE)a, LWA_ALPHA);
            //    Sleep(12);
            // }
            // FreeConsole();

            char* heartbeatMsg = vmpstr(skstr("[DEBUG] Starting heartbeat thread"));
            DECRYPT_FREE(heartbeatMsg);

            std::thread heartbeatThread(heartbeat);
            heartbeatThread.detach();

        frizy::misc::jumppower(); // Call the jumppower function

            char* engineMsg = vmpstr(skstr("[DEBUG] About to call engine::startup()"));
            std::cout << engineMsg << std::endl;
            DECRYPT_FREE(engineMsg);

            globals::misc::spectatebind.type = keybind::TOGGLE;
            globals::combat::locktargetkeybind.type = keybind::TOGGLE; // Initialize the new keybind


            engine::startup();

            char* engineCompleteMsg = vmpstr(skstr("[DEBUG] engine::startup() completed successfully"));
            std::cout << engineCompleteMsg << std::endl;
            DECRYPT_FREE(engineCompleteMsg);

            g_memoryProtector.scrambleMemory();
            return 0;
        }
        catch (const std::exception& e) {
            char* mainExceptionMsg = vmpstr(skstr("[DEBUG] Exception in main init: "));
            std::cout << mainExceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(mainExceptionMsg);
            Sleep(5000);
            exit(44444);
        }
        catch (...) {
            char* mainUnknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in main init"));
            std::cout << mainUnknownMsg << std::endl;
            DECRYPT_FREE(mainUnknownMsg);
            Sleep(5000);
            exit(99999);
        }
        vmend
        };

    functionHider.hideFunction(vmpstr(skstr("<")), mainInitFunction);

    std::vector<char> memoryBlock(1024, 0);
    functionEncryptor.encryptMemory(memoryBlock.data(), memoryBlock.size());

    g_memoryProtector.scrambleMemory();
    return functionHider.callHiddenFunction(vmpstr(skstr("<")));
}

int sdhflsdhflshdlfhjsdlfhsldhflsdhflsdhflhasd = jsdlfhjsdlkfhjssdfjsdlfjhlsdjfsdjflkjsdlfjslghdshga();

int main()
{
    vmultra_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    if (!VMPIsValidCRC() || VMPIsDebuggerPresent()) {
        exit(0xF0);
    }

    // Let the initialization in jsdlfhjsdlkfhjssdfjsdlfjhlsdjfsdjflkjsdlfjslghdshga() handle everything
    
    // Keep process alive so overlay threads continue running
    for (;;) { 
        Sleep(1000); 
    }
    
    return 0;
    vmend
}
