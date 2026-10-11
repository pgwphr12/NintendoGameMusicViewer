#pragma once
#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <shlobj.h>
#include <windows.h>
namespace ngmv::desktop {
// Integer storage avoids locale-dependent floating point and non-finite values.
class Settings {
  public:
    std::map<std::string, int> values;
    static std::filesystem::path path() {
        wchar_t isolated[32768]{};
        DWORD length = GetEnvironmentVariableW(L"NGMV_SETTINGS_TEST_FILE", isolated, 32768);
        if (length && length < 32768) return isolated;
        wchar_t folder[MAX_PATH]{};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, folder)))
            return {};
        return std::filesystem::path(folder) / L"NintendoGameMusicViewer" / L"settings.ini";
    }
    void load(const std::filesystem::path &file) {
        values.clear();
        std::error_code ec;
        if (std::filesystem::file_size(file, ec) > 65536 || ec) return;
        std::ifstream input(file);
        std::string line;
        while (std::getline(input, line)) {
            auto split = line.find('=');
            if (split == std::string::npos || split == 0 || split > 512) continue;
            auto text = line.substr(split + 1);
            if (!text.empty() && text.back() == '\r') text.pop_back();
            int value = 0;
            auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec == std::errc{} && result.ptr == text.data() + text.size())
                values[line.substr(0, split)] = value;
        }
    }
    int get(const std::string &key, int fallback, int minimum, int maximum) const {
        auto i = values.find(key);
        return i != values.end() && i->second >= minimum && i->second <= maximum ? i->second : fallback;
    }
    std::string text() const {
        std::ostringstream out;
        for (const auto &entry : values) out << entry.first << '=' << entry.second << '\n';
        return out.str();
    }
    bool save(const std::filesystem::path &file) const {
        if (file.empty()) return false;
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        if (ec) return false;
        auto temporary = file;
        temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << text();
        output.flush();
        bool ok = bool(output);
        output.close();
        if (ok && MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;
        std::filesystem::remove(temporary, ec);
        return false;
    }
    static std::string channelKey(const std::string &system, const std::string &channel) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string key = "channel.";
        for (unsigned char c : system + "\n" + channel) {
            key += digits[c >> 4]; key += digits[c & 15];
        }
        return key;
    }
};
}
