#include "WindowsIntegration.hpp"
#include <algorithm>
#include <commctrl.h>
#include <cstring>
#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdexcept>
namespace ngmv::desktop {
static constexpr wchar_t Preferences[] = L"Software\\NintendoGameMusicViewer\\Preferences";
static std::wstring testSession() {
    wchar_t text[256]{};
    DWORD size = GetEnvironmentVariableW(L"NGMV_TEST_SESSION", text, 256);
    return size && size < 256 ? text : L"";
}
bool automatedSession() { return !testSession().empty(); }
static std::wstring userIdentity() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        throw std::runtime_error("Cannot determine the Windows user.");
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> bytes(size);
    bool ok = GetTokenInformation(token, TokenUser, bytes.data(), size, &size) != FALSE;
    CloseHandle(token);
    LPWSTR sid = nullptr;
    if (!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(bytes.data())->User.Sid, &sid))
        throw std::runtime_error("Cannot determine the Windows user.");
    std::wstring result(sid);
    LocalFree(sid);
    return result + L"." + testSession();
}
ULONG_PTR instanceToken() {
    static const ULONG_PTR value = [] {
        ULONG_PTR hash = 2166136261u;
        for (wchar_t c : userIdentity()) hash = (hash ^ c) * 16777619u;
        return hash ? hash : ULONG_PTR(1);
    }();
    return value;
}
std::filesystem::path executablePath() {
    std::vector<wchar_t> buffer(32768);
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (!size || size >= buffer.size()) throw std::runtime_error("Cannot determine the executable path.");
    return std::filesystem::path(std::wstring(buffer.data(), size));
}
std::vector<std::wstring> supportedExtensions() {
    return {L".nsf", L".nsfe", L".spc", L".gbs", L".gsf", L".minigsf", L".2sf",
            L".mini2sf", L".usf", L".miniusf", L".bcstm", L".bcwav", L".vgm", L".vgz"};
}
bool decodeRequest(const COPYDATASTRUCT *data, std::wstring &result) {
    if (!data || data->dwData != CopyDataTag || !data->lpData ||
        data->cbData < sizeof(wchar_t) || data->cbData > 32768 * sizeof(wchar_t) ||
        data->cbData % sizeof(wchar_t)) return false;
    std::wstring text(data->cbData / sizeof(wchar_t), L'\0');
    memcpy(text.data(), data->lpData, data->cbData);
    if (text.back() || text.find(L'\0') != text.size() - 1) return false;
    text.pop_back();
    if (!text.empty() && !std::filesystem::path(text).is_absolute()) return false;
    result = std::move(text);
    return true;
}
std::vector<RegistryValue> registrationPlan(const std::filesystem::path &exe) {
    if (!exe.is_absolute() || exe.native().find(L'"') != std::wstring::npos)
        throw std::runtime_error("File registration requires an absolute executable path.");
    const auto quoted = L"\"" + exe.native() + L"\"";
    const auto command = quoted + L" --play \"%1\"";
    const std::wstring caps = L"Software\\NintendoGameMusicViewer\\Capabilities";
    const std::wstring apps = L"Software\\Classes\\Applications\\NintendoGameMusicViewer.exe";
    std::vector<RegistryValue> values{
        {caps, L"ApplicationName", L"Nintendo Game Music Viewer"},
        {caps, L"ApplicationDescription", L"NES, SNES, Game Boy, GBA, DS, Nintendo 64, 3DS, SMS and Mega Drive game music"},
        {caps, L"ApplicationIcon", quoted + L",0"},
        {L"Software\\RegisteredApplications", L"NintendoGameMusicViewer", caps},
        {apps, L"FriendlyAppName", L"Nintendo Game Music Viewer"},
        {apps + L"\\DefaultIcon", L"", quoted + L",0"},
        {apps + L"\\shell\\open\\command", L"", command},
        {L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\NintendoGameMusicViewer.exe", L"", exe.native()}
    };
    for (const auto &extension : supportedExtensions()) {
        const std::wstring progId = L"NintendoGameMusicViewer" + extension;
        const std::wstring key = L"Software\\Classes\\" + progId;
        values.push_back({key, L"", L"Nintendo Game Music (" + extension + L")"});
        values.push_back({key + L"\\DefaultIcon", L"", quoted + L",0"});
        values.push_back({key + L"\\shell\\open\\command", L"", command});
        values.push_back({caps + L"\\FileAssociations", extension, progId});
        values.push_back({L"Software\\Classes\\" + extension + L"\\OpenWithProgids", progId, L""});
        values.push_back({apps + L"\\SupportedTypes", extension, L""});
    }
    return values;
}
void registerFileTypes(HKEY root, const std::filesystem::path &exe, bool notifyShell) {
    for (const auto &entry : registrationPlan(exe)) {
        HKEY key = nullptr;
        LONG error = RegCreateKeyExW(root, entry.key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
        if (error == ERROR_SUCCESS) {
            error = RegSetValueExW(key, entry.name.c_str(), 0, REG_SZ,
                                  reinterpret_cast<const BYTE *>(entry.value.c_str()),
                                  DWORD((entry.value.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(key);
        }
        if (error != ERROR_SUCCESS) throw std::runtime_error("Cannot register supported file types for this Windows user.");
    }
    if (notifyShell) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
static DWORD preference(const wchar_t *name) {
    DWORD value = 0, size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, Preferences, name, RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value;
}
static void savePreference(const wchar_t *name, DWORD value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, Preferences, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot save the default-app prompt preference.");
    LONG result = RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<BYTE *>(&value), sizeof(value));
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) throw std::runtime_error("Cannot save the default-app prompt preference.");
}
void defaultAppsPrompt(HWND parent, bool force, bool english) {
    if (automatedSession()) return; // Automated tests never change the user's associations.
    if (!force && preference(L"FileRegistrationConsent")) {
        // Portable packages can move. Prior consent covers refreshing our own
        // registration, but never changing the user's Windows default choice.
        wchar_t command[32768]{}; DWORD bytes = sizeof command;
        RegGetValueW(HKEY_CURRENT_USER,
            L"Software\\Classes\\Applications\\NintendoGameMusicViewer.exe\\shell\\open\\command",
            L"", RRF_RT_REG_SZ, nullptr, command, &bytes);
        auto exe = executablePath();
        if (command != L"\"" + exe.native() + L"\" --play \"%1\"")
            registerFileTypes(HKEY_CURRENT_USER, exe, true);
        return;
    }
    if (!force && preference(L"DontAskDefaultApps")) return;
    TASKDIALOG_BUTTON buttons[] = {
        {100, english ? L"Register and open Windows settings" : L"등록하고 Windows 설정 열기"},
        {101, english ? L"Not now" : L"나중에"}
    };
    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof dialog;
    dialog.hwndParent = parent;
    dialog.hInstance = GetModuleHandleW(nullptr);
    dialog.pszMainIcon = MAKEINTRESOURCEW(101);
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    dialog.pszWindowTitle = L"Nintendo Game Music Viewer";
    dialog.pszMainInstruction = english ? L"Use this viewer to open supported game music files?" : L"지원하는 게임 음악 파일을 이 프로그램으로 열까요?";
    dialog.pszContent = english
        ? L"NSF, NSFE, SPC, GBS, GSF, MINIGSF, 2SF, MINI2SF, USF, MINIUSF, BCSTM, BCWAV, VGM, VGZ\n\nRegister these 14 types, then choose this viewer in Windows Default apps. Existing defaults stay in place until you select it."
        : L"NSF, NSFE, SPC, GBS, GSF, MINIGSF, 2SF, MINI2SF, USF, MINIUSF, BCSTM, BCWAV, VGM, VGZ\n\n14개 확장자를 등록한 뒤 Windows 기본 앱 설정에서 이 프로그램을 선택하세요. 기본 앱 변경은 Windows 설정에서 직접 완료해야 합니다.";
    dialog.pszVerificationText = english ? L"Don't ask again" : L"다시 묻지 않기";
    dialog.cButtons = 2;
    dialog.pButtons = buttons;
    dialog.nDefaultButton = 101;
    int choice = 0;
    BOOL neverAsk = FALSE;
    HRESULT result = TaskDialogIndirect(&dialog, &choice, nullptr, &neverAsk);
    if (FAILED(result)) throw std::runtime_error("Cannot display the default-app prompt.");
    if (choice == 100) {
        registerFileTypes(HKEY_CURRENT_USER, executablePath(), true);
        savePreference(L"FileRegistrationConsent", 1);
        savePreference(L"DontAskDefaultApps", 1);
        auto launched = ShellExecuteW(parent, L"open", L"ms-settings:defaultapps?registeredAppUser=NintendoGameMusicViewer", nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(launched) <= 32)
            throw std::runtime_error("Open Windows Settings > Apps > Default apps to choose Nintendo Game Music Viewer.");
    } else if (neverAsk) savePreference(L"DontAskDefaultApps", 1);
}
struct Search { ULONG_PTR token; HWND window = nullptr; };
static BOOL CALLBACK findPrimary(HWND window, LPARAM parameter) {
    auto &search = *reinterpret_cast<Search *>(parameter);
    if (reinterpret_cast<ULONG_PTR>(GetPropW(window, InstanceProperty)) == search.token) {
        wchar_t className[128]{};
        GetClassNameW(window, className, 128);
        if (!wcscmp(className, WindowClass)) { search.window = window; return FALSE; }
    }
    return TRUE;
}
InstanceGate::~InstanceGate() {
    if (owns_) ReleaseMutex(mutex_);
    if (mutex_) CloseHandle(mutex_);
}
bool InstanceGate::forwardOrBecomePrimary(const std::filesystem::path &input) {
    std::wstring name = L"Local\\NintendoGameMusicViewer.V1." + userIdentity();
    mutex_ = CreateMutexW(nullptr, TRUE, name.c_str());
    DWORD error = GetLastError();
    if (!mutex_) throw std::runtime_error("Cannot initialize the single-instance player.");
    if (error != ERROR_ALREADY_EXISTS) { owns_ = true; return false; }
    auto text = input.empty() ? L"" : std::filesystem::absolute(input).lexically_normal().wstring();
    if (text.size() >= 32768) throw std::runtime_error("The music file path is too long.");
    auto deadline = GetTickCount64() + 15000;
    while (GetTickCount64() < deadline) {
        DWORD wait = WaitForSingleObject(mutex_, 0);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) { owns_ = true; return false; }
        Search search{instanceToken()};
        EnumWindows(findPrimary, reinterpret_cast<LPARAM>(&search));
        if (search.window) {
            DWORD pid = 0;
            GetWindowThreadProcessId(search.window, &pid);
            AllowSetForegroundWindow(pid);
            COPYDATASTRUCT request{CopyDataTag, DWORD((text.size() + 1) * sizeof(wchar_t)), const_cast<wchar_t *>(text.c_str())};
            DWORD_PTR accepted = 0;
            if (SendMessageTimeoutW(search.window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&request),
                                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &accepted) && accepted)
                return true;
        }
        Sleep(50);
    }
    throw std::runtime_error("The existing viewer is not responding. Close it and try opening the file again.");
}
}
