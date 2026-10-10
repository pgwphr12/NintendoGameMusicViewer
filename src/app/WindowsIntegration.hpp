#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
namespace ngmv::desktop {
inline constexpr wchar_t WindowClass[] = L"NintendoGameMusicViewerWindow";
inline constexpr wchar_t InstanceProperty[] = L"NGMV.SingleInstance.V1";
inline constexpr ULONG_PTR CopyDataTag = 0x4e474d31;
inline constexpr UINT OpenQueuedFile = WM_APP + 21;
inline constexpr UINT AskDefaultApps = WM_APP + 22;
inline constexpr UINT DefaultAppsCommand = 0xb100;
bool automatedSession();
ULONG_PTR instanceToken();
std::filesystem::path executablePath();
std::vector<std::wstring> supportedExtensions();
bool decodeRequest(const COPYDATASTRUCT *, std::wstring &);
struct RegistryValue { std::wstring key, name, value; };
std::vector<RegistryValue> registrationPlan(const std::filesystem::path &);
void registerFileTypes(HKEY root, const std::filesystem::path &, bool notifyShell);
void defaultAppsPrompt(HWND, bool force, bool english = false);
class InstanceGate {
    HANDLE mutex_ = nullptr;
    bool owns_ = false;
  public:
    ~InstanceGate();
    // True means the primary accepted the request; this process should exit.
    bool forwardOrBecomePrimary(const std::filesystem::path &);
};
}
