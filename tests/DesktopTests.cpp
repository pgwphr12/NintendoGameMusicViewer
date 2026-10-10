#include "app/WindowsIntegration.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace ngmv::desktop;
static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static std::wstring read(HKEY root, const std::wstring &key, const std::wstring &name) {
    wchar_t value[2048]{}; DWORD size = sizeof value;
    check(RegGetValueW(root, key.c_str(), name.c_str(), RRF_RT_REG_SZ, nullptr, value, &size) == ERROR_SUCCESS, "Registry value missing");
    return value;
}
int main(int argc, char **argv) {
    HKEY isolated = nullptr;
    const std::wstring testKey = L"Software\\NGMVDesktopTests\\" + std::to_wstring(GetCurrentProcessId());
    try {
        const std::filesystem::path exe = L"C:\\음악 프로그램\\NintendoGameMusicViewer.exe";
        auto extensions = supportedExtensions();
        check(extensions.size() == 14 && std::set<std::wstring>(extensions.begin(), extensions.end()).size() == 14, "14 unique extensions required");
        auto entries = registrationPlan(exe);
        for (const auto &entry : entries) {
            check(entry.key.find(L"UserChoice") == std::wstring::npos, "Never overwrite UserChoice");
            for (const auto &ext : extensions)
                check(!(entry.key == L"Software\\Classes\\" + ext && entry.name.empty()), "Never overwrite extension defaults");
            if (entry.key.find(L"\\command") != std::wstring::npos)
                check(entry.value == L"\"" + exe.native() + L"\" --play \"%1\"", "Quote executable and file paths");
        }
        check(RegCreateKeyExW(HKEY_CURRENT_USER, testKey.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &isolated, nullptr) == ERROR_SUCCESS, "Test registry root");
        if (argc > 1 && std::string(argv[1]) == "--prompt-test") {
            // Redirect HKCU only in this process. Real preferences and default
            // registrations remain untouched by the dialog test.
            check(RegOverridePredefKey(HKEY_CURRENT_USER, isolated) == ERROR_SUCCESS, "Override test HKCU");
            defaultAppsPrompt(nullptr, false, false);
            DWORD suppressed = 0, size = sizeof suppressed;
            check(RegGetValueW(HKEY_CURRENT_USER, L"Software\\NintendoGameMusicViewer\\Preferences", L"DontAskDefaultApps", RRF_RT_REG_DWORD, nullptr, &suppressed, &size) == ERROR_SUCCESS && suppressed == 1, "Remember don't ask again");
            defaultAppsPrompt(nullptr, false, false); // Must skip, without a dialog.
            defaultAppsPrompt(nullptr, true, true); // F10 must still work.
            RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
            RegCloseKey(isolated); isolated = nullptr;
            RegDeleteTreeW(HKEY_CURRENT_USER, testKey.c_str());
            std::cout << "Korean/English prompts, decline, preference persistence and forced reopen: PASS\n";
            return 0;
        }
        HKEY previous = nullptr;
        check(RegCreateKeyExW(isolated, L"Software\\Classes\\.nsf", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &previous, nullptr) == ERROR_SUCCESS, "Seed previous association");
        const wchar_t old[] = L"Previous.Player";
        RegSetValueExW(previous, L"", 0, REG_SZ, reinterpret_cast<const BYTE *>(old), sizeof old); RegCloseKey(previous);
        registerFileTypes(isolated, exe, false);
        for (const auto &entry : entries) check(read(isolated, entry.key, entry.name) == entry.value, "Registered value differs");
        check(read(isolated, L"Software\\Classes\\.nsf", L"") == old, "Previous default must survive");
        RegCloseKey(isolated); isolated = nullptr;
        check(RegDeleteTreeW(HKEY_CURRENT_USER, testKey.c_str()) == ERROR_SUCCESS, "Clean test registry");
        std::wstring out, path = L"E:\\노래\\Test & Track.mini2sf";
        COPYDATASTRUCT request{CopyDataTag, DWORD((path.size()+1)*sizeof(wchar_t)), path.data()};
        check(decodeRequest(&request, out) && out == path, "Unicode handoff");
        request.dwData++; check(!decodeRequest(&request,out), "Reject wrong protocol"); request.dwData--;
        request.cbData--; check(!decodeRequest(&request,out), "Reject partial characters");
        request.cbData = DWORD(path.size()*sizeof(wchar_t)); check(!decodeRequest(&request,out), "Reject missing terminator");
        request.cbData = 65538; check(!decodeRequest(&request,out), "Reject oversized payload");
        wchar_t relative[] = L"test.nsf"; request.cbData = sizeof relative; request.lpData = relative;
        check(!decodeRequest(&request,out), "Reject relative path");
        wchar_t embedded[] = {L'C',L':',L'\\',0,L'a',0}; request.cbData = sizeof embedded; request.lpData = embedded;
        check(!decodeRequest(&request,out), "Reject embedded terminator");
        wchar_t empty[] = L""; request.cbData = sizeof empty; request.lpData = empty;
        check(decodeRequest(&request,out) && out.empty(), "Activation-only request");
        check(!decodeRequest(nullptr,out), "Reject absent request");
        std::cout << "14 file types, quoted Unicode commands, isolated registry, preservation of defaults, IPC validation: PASS\n";
        return 0;
    } catch (const std::exception &e) {
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        if (isolated) RegCloseKey(isolated);
        RegDeleteTreeW(HKEY_CURRENT_USER, testKey.c_str());
        std::cerr << e.what() << '\n'; return 1;
    }
}
