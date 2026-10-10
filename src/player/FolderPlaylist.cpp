#include "FolderPlaylist.hpp"
#include <algorithm>
#include <cwctype>
#include <shlwapi.h>
#include <windows.h>
namespace ngmv {
FolderPlaylist FolderPlaylist::scan(const std::filesystem::path &file, Format format) {
    FolderPlaylist result;
    if (format == Format::Nsf || format == Format::Nsfe || format == Format::Gbs)
        return result;
    auto matches = [format](const std::filesystem::path &path) {
        auto ext = path.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](wchar_t c) { return std::towlower(c); });
        switch (format) {
        case Format::Vgm:
            return ext == L".vgm" || ext == L".vgz";
        case Format::Spc:
            return ext == L".spc";
        case Format::Gsf:
            return ext == L".gsf" || ext == L".minigsf";
        case Format::TwoSf:
            return ext == L".2sf" || ext == L".mini2sf";
        case Format::Usf:
            return ext == L".usf" || ext == L".miniusf";
        case Format::Bcstm:
        case Format::Bcwav:
            return ext == L".bcstm" || ext == L".bcwav";
        default:
            return false;
        }
    };
    std::error_code error;
    auto absolute = std::filesystem::absolute(file, error).lexically_normal();
    if (error)
        return result;
    auto directory = std::filesystem::directory_iterator(absolute.parent_path(), error);
    for (auto end = std::filesystem::directory_iterator(); !error && directory != end;
         directory.increment(error)) {
        auto path = directory->path();
        if (directory->is_regular_file(error) && matches(path) &&
            CompareStringOrdinal(path.filename().c_str(), -1, absolute.filename().c_str(), -1,
                                 TRUE) != CSTR_EQUAL)
            result.files.push_back(path);
        error.clear();
    }
    // Keep the selected file even if enumeration fails or its extension was renamed.
    result.files.push_back(absolute);
    std::sort(result.files.begin(), result.files.end(), [](const auto &a, const auto &b) {
        int order = StrCmpLogicalW(a.filename().c_str(), b.filename().c_str());
        return order ? order < 0 : a.native() < b.native();
    });
    result.current = size_t(std::find(result.files.begin(), result.files.end(), absolute) -
                            result.files.begin());
    return result;
}
} // namespace ngmv
