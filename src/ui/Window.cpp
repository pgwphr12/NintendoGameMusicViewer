#include "Window.hpp"
#include "app/WindowsIntegration.hpp"
#include "ChannelLayout.hpp"
#include "visualizer/Envelope.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <commdlg.h>
#include <cstdio>
#include <fstream>
#include <map>
#include <shellapi.h>
#include <windowsx.h>
namespace ngmv {
std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
    UINT cp = CP_UTF8;
    if (!n) {
        cp = CP_ACP;
        n = MultiByteToWideChar(cp, 0, s.data(), int(s.size()), nullptr, 0);
    }
    std::wstring out(n, 0);
    MultiByteToWideChar(cp, 0, s.data(), int(s.size()), out.data(), n);
    return out;
}
// GDI objects are reused on the UI thread and released when the thread exits.
struct DrawingResources {
    std::map<COLORREF, HBRUSH> brushes;
    std::map<std::pair<COLORREF, int>, HPEN> pens;
    std::map<std::pair<int, bool>, HFONT> fonts;
    ~DrawingResources() {
        for (auto &entry : brushes)
            DeleteObject(entry.second);
        for (auto &entry : pens)
            DeleteObject(entry.second);
        for (auto &entry : fonts)
            DeleteObject(entry.second);
    }
    HBRUSH brush(COLORREF color) {
        auto &value = brushes[color];
        if (!value)
            value = CreateSolidBrush(color);
        return value;
    }
    HPEN pen(COLORREF color, int width) {
        auto &value = pens[{color, width}];
        if (!value)
            value = CreatePen(PS_SOLID, width, color);
        return value;
    }
    HFONT font(int size, bool bold) {
        auto &value = fonts[{size, bold}];
        if (!value)
            value = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        return value;
    }
};
static thread_local DrawingResources drawing;
static void fill(HDC dc, RECT r, COLORREF color) {
    FillRect(dc, &r, drawing.brush(color));
}
static void text(HDC dc, const std::wstring &s, int x, int y, int w, int h, int size,
                 COLORREF color, bool bold = false) {
    auto old = SelectObject(dc, drawing.font(size, bold));
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT r{x, y, x + w, y + h};
    DrawTextW(dc, s.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, old);
}
static void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    auto old = SelectObject(dc, drawing.pen(color, width));
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
}
static std::wstring channelLabel(const std::string &name, bool recording) {
    if (name == "FDS WAVE")
        return L"FDS";
    if (name.rfind("CHANNEL ", 0) == 0)
        return (recording ? L"CHANNEL " : L"채널 ") + wide(name.substr(8));
    if (name.rfind("VOICE ", 0) == 0)
        return (recording ? L"CHANNEL " : L"채널 ") + wide(name.substr(6));
    if (recording)
        return wide(name);
    if (name == "LEFT")
        return L"왼쪽 출력";
    if (name == "RIGHT")
        return L"오른쪽 출력";
    if (name == "MONO")
        return L"모노 출력";
    if (name == "EFFECTS / OTHER")
        return L"게임 잔향 / 기타";
    if (name.rfind("STREAM ", 0) == 0)
        return L"스트림 " + wide(name.substr(7));
    if (name == "TRIANGLE")
        return L"삼각파";
    if (name == "NOISE")
        return L"노이즈";
    if (name == "WAVE")
        return L"웨이브";
    if (name.rfind("PULSE ", 0) == 0)
        return L"펄스 " + wide(name.substr(6));
    if (name.rfind("SQUARE ", 0) == 0)
        return L"사각파 " + wide(name.substr(7));
    return wide(name);
}
static std::wstring time(double s) {
    int n = std::max(0, int(s));
    wchar_t b[40];
    swprintf_s(b, L"%02d:%02d", n / 60, n % 60);
    return b;
}
static void button(HDC dc, const wchar_t *s, int x, int y, int w, bool active = false) {
    fill(dc, {x, y, x + w, y + 50}, active ? RGB(31, 76, 76) : RGB(28, 34, 46));
    text(dc, s, x + 16, y, w - 28, 50, 20, active ? RGB(101, 237, 204) : RGB(214, 220, 235));
}
static void volumeButton(HDC dc, const wchar_t *label, int x, int y, int size = 32) {
    fill(dc, {x, y, x + size, y + size - 2}, RGB(28, 34, 46));
    text(dc, label, x + size / 4, y, size * 3 / 4, size - 2, size * 5 / 8, RGB(214, 220, 235));
}
void Window::report(const std::exception &e) {
    message_ = wide(e.what());
    MessageBoxW(hwnd_, message_.c_str(), L"Nintendo Game Music Viewer", MB_OK | MB_ICONERROR);
}
struct ChangingScope {
    bool &value;
    explicit ChangingScope(bool &flag) : value(flag) {
        value = true;
    }
    ~ChangingScope() {
        value = false;
    }
};
bool Window::open(const std::filesystem::path &p) {
    if (changing_)
        return false;
    try {
        ChangingScope guard(changing_);
        auto playlist = FolderPlaylist::scan(p, identify(readMusicFile(p)));
        player_.open(p);
        file_ = p;
        SetWindowTextW(
            hwnd_, (L"Nintendo Game Music Viewer · V1.3.1 — " + file_.filename().wstring()).c_str());
        playlist_ = std::move(playlist);

        drag_ = -1;
        message_.clear();

        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    } catch (const std::exception &e) {
        report(e);
        return false;
    }
}
void Window::selectFolder(size_t index, bool play) {
    if (changing_ || index >= playlist_.files.size())
        return;
    ChangingScope guard(changing_);
    const auto path = playlist_.files[index];
    player_.open(path);
    playlist_.current = index;
    file_ = path;
    SetWindowTextW(hwnd_,
                   (L"Nintendo Game Music Viewer · V1.3.1 — " + file_.filename().wstring()).c_str());

    drag_ = -1;
    message_.clear();
    if (play)
        player_.play();
    InvalidateRect(hwnd_, nullptr, FALSE);
}
void Window::nextTrack(int direction, bool automatic) {
    if (changing_ || !player_.loaded())
        return;
    if (playlist_.active()) {
        if (automatic && playlist_.current + 1 >= playlist_.files.size())
            return;
        auto size = playlist_.files.size();
        size_t next =
            direction > 0 ? (playlist_.current + 1) % size : (playlist_.current + size - 1) % size;
        selectFolder(next, automatic || player_.playing());
    } else if (!automatic && !player_.tracks().empty()) {
        int size = int(player_.tracks().size());
        player_.select((player_.track() + size + direction) % size);
    }
}
void Window::openDialog() {
    wchar_t path[32768] = {};
    OPENFILENAMEW d{};
    d.lStructSize = sizeof d;
    d.hwndOwner = hwnd_;
    d.lpstrFilter =
        L"게임 음악 "
        L"(*.nsf;*.nsfe;*.spc;*.gbs;*.gsf;*.minigsf;*.2sf;*.mini2sf;*.usf;*.miniusf;*."
        L"bcstm;*.bcwav;*.vgm;*.vgz)\0*.nsf;*.nsfe;*.spc;*.gbs;*.gsf;*.minigsf;*.2sf;*.mini2sf;*."
        L"usf;*.miniusf;*.bcstm;*.bcwav;*.vgm;*.vgz\0모든 파일\0*.*\0";
    d.lpstrFile = path;
    d.nMaxFile = 32768;
    d.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&d))
        open(path);
}
void Window::tracksMenu() {
    if (!player_.loaded())
        return;
    auto menu = CreatePopupMenu();
    int index = 0;
    if (playlist_.active()) {
        for (const auto &path : playlist_.files) {
            auto title = path.stem().wstring();
            AppendMenuW(menu, MF_STRING | (size_t(index) == playlist_.current ? MF_CHECKED : 0),
                        1000 + index, title.c_str());
            ++index;
        }
    } else
        for (const auto &t : player_.tracks()) {
            std::wstring title =
                std::to_wstring(index + 1) + L"  " +
                (t.title.empty() ? L"Track " + std::to_wstring(index + 1) : wide(t.title));
            AppendMenuW(menu, MF_STRING | (index == player_.track() ? MF_CHECKED : 0), 1000 + index,
                        title.c_str());
            index++;
        }
    POINT p;
    GetCursorPos(&p);
    auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    if (chosen >= 1000) {
        if (playlist_.active())
            selectFolder(chosen - 1000, player_.playing());
        else
            player_.select(chosen - 1000);
    }
}
void Window::rateMenu() {
    auto menu = CreatePopupMenu();
    const int rates[] = {16000, 22050, 32000, 44100, 48000, 96000};
    for (int i = 0; i < 6; i++)
        AppendMenuW(menu, MF_STRING | (rates[i] == player_.rate() ? MF_CHECKED : 0), i + 1,
                    (std::to_wstring(rates[i]) + L" Hz").c_str());
    POINT p;
    GetCursorPos(&p);
    auto n = TrackPopupMenu(menu, TPM_RETURNCMD, p.x, p.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    if (n >= 1 && n <= 6)
        player_.rate(rates[n - 1]);
}
void Window::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    if (fullscreen_) {
        GetWindowRect(hwnd_, &oldRect_);
        SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        MONITORINFO m{sizeof m};
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &m);
        SetWindowPos(hwnd_, HWND_TOP, m.rcMonitor.left, m.rcMonitor.top,
                     m.rcMonitor.right - m.rcMonitor.left, m.rcMonitor.bottom - m.rcMonitor.top,
                     SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(hwnd_, nullptr, oldRect_.left, oldRect_.top, oldRect_.right - oldRect_.left,
                     oldRect_.bottom - oldRect_.top, SWP_FRAMECHANGED | SWP_NOZORDER);
    }
}
std::vector<size_t> Window::visibleChannels() const {
    std::vector<size_t> indices;
    const auto &channels = player_.channels();
    const bool dmcUsed = player_.dmcUsed();
    for (size_t i = 0; i < channels.size(); ++i)
        if (channels[i].name != "DMC" || dmcUsed)
            indices.push_back(i);
    return indices;
}
void Window::paint(HDC dc, int width, int height) {
    segments_.clear();
    segments_.reserve(8 * 1576 * 2);
    fill(dc, {0, 0, width, height}, RGB(6, 9, 14));
    scale_ = std::min(width / 1920., height / 1080.);
    offsetX_ = int((width - 1920 * scale_) / 2);
    offsetY_ = int((height - 1080 * scale_) / 2);
    int saved = SaveDC(dc);
    SetGraphicsMode(dc, GM_ADVANCED);
    XFORM xf{FLOAT(scale_), 0, 0, FLOAT(scale_), FLOAT(offsetX_), FLOAT(offsetY_)};
    SetWorldTransform(dc, &xf);
    fill(dc, {0, 0, 1920, 1080}, RGB(12, 16, 24));
    if (changing_) {
        text(dc, recording_ ? L"Loading…" : L"불러오는 중…", 42, 22, 1810, 62, 46,
             RGB(235, 240, 249));
        RestoreDC(dc, saved);
        return;
    }
    const auto &tracks = player_.tracks();
    TrackInfo info;
    if (player_.loaded() && size_t(player_.track()) < tracks.size())
        info = tracks[player_.track()];
    std::wstring title =
        player_.loaded() ? (info.title.empty() ? L"Track " + std::to_wstring(player_.track() + 1)
                                               : wide(info.title))
                         : (recording_ ? L"Open a music file" : L"음악 파일을 열어 주세요");
    text(dc, title, 42, 22, 1810, 62, 46, RGB(235, 240, 249), true);
    std::wstring game =
        info.game.empty()
            ? (file_.empty()
                   ? L"NSF · NSFE · SPC · GBS · GSF · 2SF · USF · BCSTM · BCWAV · VGM · VGZ"
                   : file_.stem().wstring())
            : wide(info.game);
    text(dc,
         game + L"  /  " + wide(info.system) +
             (info.author.empty() ? L"" : L"  /  " + wide(info.author)),
         44, 89, 1600, 32, 22, RGB(150, 166, 189));
    text(dc,
         time(player_.seconds()) + L" / " +
             (info.lengthMs > 0 ? time(info.lengthMs / 1000.) : L"--:--"),
         1625, 91, 260, 30, 24, RGB(232, 206, 122));
    if (!recording_) {
        button(dc, L"파일 열기", 42, 141, 155);
        button(dc, L"Tracks", 212, 141, 155);
        button(dc, L"이전", 382, 141, 115);
        button(dc, player_.playing() ? L"일시정지" : L"재생", 512, 141, 150, player_.playing());
        button(dc, L"정지", 677, 141, 115);
        button(dc, L"다음", 807, 141, 115);
        button(dc, L"반복", 937, 141, 115, player_.repeat());
        button(dc, L"녹화뷰", 1067, 141, 115);
        button(dc, autoplay_ ? L"자동재생 켜짐" : L"자동재생 꺼짐", 1197, 141, 160, autoplay_);
        button(dc, (std::to_wstring(player_.rate()) + L" Hz").c_str(), 1372, 141, 147);
    }
    text(dc, recording_ ? L"Volume" : L"전체 음량", 1550, 131, 140, 28, 18, RGB(154, 168, 190));
    volumeButton(dc, L"−", 1550, 163);
    line(dc, 1594, 178, 1798, 178, RGB(48, 59, 77), 5);
    line(dc, 1594, 178, 1594 + int(204 * player_.master()), 178, RGB(80, 203, 180), 5);
    volumeButton(dc, L"+", 1810, 163);
    text(dc, std::to_wstring(int(std::round(player_.master() * 100))) + L"%", 1850, 159, 65, 35, 18,
         RGB(196, 208, 225));
    if (!recording_) {
        wchar_t speed[64];
        swprintf_s(speed, L"피치/속도  %.2f×", player_.speed());
        text(dc, speed, 42, 205, 265, 50, 22, RGB(196, 208, 225));
        button(dc, speedStep_ == 1 ? L"−0.01" : L"−0.10", 322, 205, 110);
        button(dc, speedStep_ == 1 ? L"+0.01" : L"+0.10", 447, 205, 110);
        button(dc, L"초기화", 572, 205, 135);
        button(dc, player_.reverb() ? L"반향 켜짐" : L"반향 꺼짐", 722, 205, 180, player_.reverb());
        button(dc, speedStep_ == 1 ? L"단위 0.01" : L"단위 0.10", 917, 205, 200);
        text(dc, L"반향 강도", 1140, 205, 130, 50, 20, RGB(154, 168, 190));
        volumeButton(dc, L"−", 1276, 215);
        line(dc, 1320, 230, 1450, 230, RGB(48, 59, 77), 5);
        line(dc, 1320, 230, 1320 + int(130 * player_.reverbAmount()), 230, RGB(80, 203, 180), 5);
        volumeButton(dc, L"+", 1462, 215);
        text(dc, std::to_wstring(int(std::round(player_.reverbAmount() * 100))) + L"%", 1505, 205,
             75, 50, 20, RGB(196, 208, 225));
        text(dc, L"잔향 시간", 1590, 205, 105, 50, 20, RGB(154, 168, 190));
        volumeButton(dc, L"−", 1700, 215);
        wchar_t decay[32];
        swprintf_s(decay, L"%.1f s", player_.reverbDecay());
        text(dc, decay, 1745, 205, 75, 50, 20, RGB(196, 208, 225));
        volumeButton(dc, L"+", 1830, 215);
    }
    const auto &channels = player_.channels();
    auto waves = player_.waveforms();
    auto states = player_.states();
    const COLORREF colors[] = {RGB(247, 205, 87),  RGB(126, 222, 190), RGB(137, 180, 249),
                               RGB(222, 146, 232), RGB(250, 155, 106), RGB(181, 211, 119),
                               RGB(126, 213, 228), RGB(247, 163, 181)};
    auto visible = visibleChannels();

    size_t count = visible.size();
    for (size_t i = 0; i < count; i++) {
        size_t c = visible[i];
        if (c >= waves.size() || c >= states.size() || c >= channels.size())
            continue;
        auto cell = channelCell(i, count, recording_);
        int y = cell.y, lane = cell.height;
        fill(dc, {cell.x, y, cell.x + cell.width, y + lane - 8},
             i % 2 ? RGB(16, 22, 32) : RGB(18, 24, 35));
        float peak = 0;
        for (float v : waves[c])
            peak = std::max(peak, std::abs(v));
        COLORREF color = states[c].mute ? RGB(96, 103, 119) : colors[c % 8];
        std::wstring label = channelLabel(channels[c].name, recording_);
        if (cell.compact && channels[c].name == "EFFECTS / OTHER")
            label = recording_ ? L"FX / OTHER" : L"잔향 / 기타";
        if (cell.compact && !recording_)
            label += L" " + std::to_wstring(int(std::round(states[c].volume * 100))) + L"%";
        if (cell.compact && states[c].mute)
            label += recording_ ? L" MUTED" : L" 음소거";
        text(dc, label, cell.labelX(),
             y + (cell.compact ? 2
                  : lane < 100 ? 5
                               : 15),
             cell.split ? 140 : 205,
             cell.compact ? 18
             : lane < 100 ? 30
                          : 36,
             cell.compact ? 13 : std::min(cell.split ? 20 : 24, lane / 4), color, true);
        if (!cell.compact) {
            std::wstring status;
            if (recording_) {
                if (states[c].mute)
                    status = L"MUTED";
            } else
                status = (states[c].mute ? L"음소거 · " : L"") +
                         std::to_wstring(int(std::round(states[c].volume * 100))) + L"%";
            if (!status.empty())
                text(dc, status, cell.labelX(), y + (lane < 100 ? 35 : 48), cell.split ? 140 : 178,
                     lane < 100 ? 14 : 22, lane < 100 ? 12 : 15, RGB(118, 136, 158));
        }
        if (!recording_) {
            volumeButton(dc, L"−", cell.labelX(), cell.volumeY(), cell.buttonSize());
            int sliderY = cell.volumeY() + cell.buttonSize() / 2;
            line(dc, cell.sliderLeft(), sliderY, cell.sliderRight(), sliderY, RGB(47, 59, 78), 3);
            line(dc, cell.sliderLeft(), sliderY,
                 cell.sliderLeft() +
                     int((cell.sliderRight() - cell.sliderLeft()) * states[c].volume / 2),
                 sliderY, color, 3);
            volumeButton(dc, L"+", cell.plusX(), cell.volumeY(), cell.buttonSize());
        }
        int x0 = cell.waveLeft(), x1 = cell.waveRight(), center = y + (lane - 8) / 2;
        float amp = (lane - 22) * .44f;
        line(dc, x0, center, x1, center, RGB(44, 56, 73));
        for (int x = x0; x < x1; x += int((x1 - x0) / 8))
            line(dc, x, y + 12, x, y + lane - 18, RGB(29, 39, 54));
        auto env = envelope(waves[c], size_t(x1 - x0), size_t(windowSamples_));

        float mean = 0;
        for (float v : waves[c])
            mean += v;
        mean /= float(std::max(size_t(1), waves[c].size()));
        float gain = peak > .0001f ? std::min(64.f, .65f / peak) : 1;
        int previous = center;
        for (size_t x = 0; x < env.size(); x++) {
            auto point = env[x];
            auto py = [&](float v) {
                return center - int(std::clamp((v - mean) * gain, -1.f, 1.f) * amp);
            };
            int low = py(point.minimum), high = py(point.maximum), mid = py(point.center);
            if (low != high)
                segments_.push_back({{x0 + LONG(x), low}, {x0 + LONG(x), high}, color});
            if (x)
                segments_.push_back({{x0 + LONG(x) - 1, previous}, {x0 + LONG(x), mid}, color});
            previous = mid;
        }
    }
    if (info.lengthMs > 0 && !recording_) {
        int x = 42 + int(std::clamp(player_.seconds() * 1000 / info.lengthMs, 0., 1.) * 1840);
        line(dc, 42, 1016, 1882, 1016, RGB(42, 52, 69), 5);
        line(dc, 42, 1016, x, 1016, RGB(74, 196, 173), 5);
    }
    if (!recording_ && player_.error().empty())
        text(dc,
             L"Ctrl+O 열기  ·  Space 재생/일시정지  ·  ←/→ Track 전환  ·  F9 녹화 화면  ·  "
             L"F10 기본 앱  ·  F11 전체 화면  ·  Esc 복원  ·  +/- 시간축  ·  A 자동재생",
             42, 1040, 1840, 30, 18, RGB(112, 130, 153));
    if (!player_.error().empty())
        text(dc, (recording_ ? L"Error: " : L"오류: ") + wide(player_.error()), 42, 1048, 1800, 28,
             18, RGB(245, 131, 131));
    RestoreDC(dc, saved);
}
void Window::mouseMove(int x, int y) {
    if (drag_ < 0)
        return;
    int lx = int((x - offsetX_) / scale_);
    if (drag_ == 100)
        player_.master(std::clamp((lx - 1594) / 204.f, 0.f, 1.f));
    else if (drag_ == 101)
        player_.reverbAmount(std::clamp((lx - 1320) / 130.f, 0.f, 1.f));
    else {
        auto states = player_.states();
        if (size_t(drag_) < states.size()) {
            auto visible = visibleChannels();
            auto found = std::find(visible.begin(), visible.end(), size_t(drag_));
            if (found == visible.end())
                return;
            auto cell = channelCell(size_t(found - visible.begin()), visible.size(), recording_);
            states[drag_].volume = std::clamp(2.f * (lx - cell.sliderLeft()) /
                                                  (cell.sliderRight() - cell.sliderLeft()),
                                              0.f, 2.f);
            player_.channel(drag_, states[drag_]);
        }
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}
void Window::click(int x, int y) {
    if (changing_)
        return;
    int lx = int((x - offsetX_) / scale_), ly = int((y - offsetY_) / scale_);
    try {
        if (ly >= 163 && ly < 193 && lx >= 1550 && lx < 1842) {
            if (lx < 1582)
                player_.master((std::lround(player_.master() * 100) - 5) / 100.f);
            else if (lx >= 1810)
                player_.master((std::lround(player_.master() * 100) + 5) / 100.f);
            else if (lx >= 1594 && lx < 1798) {
                drag_ = 100;
                SetCapture(hwnd_);
                mouseMove(x, y);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (!recording_ && ly >= 205 && ly < 255) {
            if (lx >= 322 && lx < 432)
                player_.speed((std::lround(player_.speed() * 100) - speedStep_) / 100.f);
            else if (lx >= 447 && lx < 557)
                player_.speed((std::lround(player_.speed() * 100) + speedStep_) / 100.f);
            else if (lx >= 572 && lx < 707)
                player_.speed(1);
            else if (lx >= 722 && lx < 902)
                player_.reverb(!player_.reverb());
            else if (lx >= 917 && lx < 1117)
                speedStep_ = speedStep_ == 1 ? 10 : 1;
            else if (ly >= 215 && ly < 245) {
                if (lx >= 1276 && lx < 1308)
                    player_.reverbAmount((std::lround(player_.reverbAmount() * 100) - 5) / 100.f);
                else if (lx >= 1462 && lx < 1494)
                    player_.reverbAmount((std::lround(player_.reverbAmount() * 100) + 5) / 100.f);
                else if (lx >= 1700 && lx < 1732)
                    player_.reverbDecay((std::lround(player_.reverbDecay() * 10) - 1) / 10.f);
                else if (lx >= 1830 && lx < 1862)
                    player_.reverbDecay((std::lround(player_.reverbDecay() * 10) + 1) / 10.f);
                else if (lx >= 1320 && lx <= 1450) {
                    drag_ = 101;
                    SetCapture(hwnd_);
                    mouseMove(x, y);
                }
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (!recording_ && ly >= 141 && ly < 191) {
            if (lx >= 42 && lx < 197)
                openDialog();
            else if (lx >= 212 && lx < 367)
                tracksMenu();
            else if (lx >= 382 && lx < 497 && player_.loaded())
                nextTrack(-1);
            else if (lx >= 512 && lx < 662) {
                if (player_.playing())
                    player_.pause();
                else
                    player_.play();
            } else if (lx >= 677 && lx < 792)
                player_.stop();
            else if (lx >= 807 && lx < 922 && player_.loaded())
                nextTrack(1);
            else if (lx >= 937 && lx < 1052)
                player_.repeat(!player_.repeat());
            else if (lx >= 1067 && lx < 1182)
                recording_ = true;
            else if (lx >= 1197 && lx < 1357)
                autoplay_ = !autoplay_;
            else if (lx >= 1372 && lx < 1519)
                rateMenu();
            return;
        }
        if (!recording_ && player_.loaded() && ly >= 1004 && ly <= 1029) {
            auto &t = player_.tracks()[player_.track()];
            if (t.lengthMs > 0)
                player_.seek(int(std::clamp((lx - 42) / 1840., 0., 1.) * t.lengthMs));
            return;
        }
        auto states = player_.states();
        auto visible = visibleChannels();
        for (size_t slot = 0; slot < visible.size(); ++slot) {
            auto cell = channelCell(slot, visible.size(), recording_);
            if (ly < cell.y || ly >= cell.y + cell.height - 8 || lx < cell.x ||
                lx >= cell.waveLeft() - 12)
                continue;
            size_t i = visible[slot];
            if (i >= states.size())
                return;
            if (!recording_ && ly >= cell.volumeY() && ly < cell.volumeY() + cell.buttonSize()) {
                if (lx >= cell.labelX() && lx < cell.labelX() + cell.buttonSize()) {
                    states[i].volume =
                        std::clamp((std::lround(states[i].volume * 100) - 5) / 100.f, 0.f, 2.f);
                    player_.channel(i, states[i]);
                } else if (lx >= cell.plusX() && lx < cell.plusX() + cell.buttonSize()) {
                    states[i].volume =
                        std::clamp((std::lround(states[i].volume * 100) + 5) / 100.f, 0.f, 2.f);
                    player_.channel(i, states[i]);
                } else if (lx >= cell.sliderLeft() && lx < cell.sliderRight()) {
                    drag_ = int(i);
                    SetCapture(hwnd_);
                    mouseMove(x, y);
                }
            } else if (ly - cell.y < (cell.compact ? 20 : cell.height < 100 ? 36 : 51)) {
                states[i].mute = !states[i].mute;
                player_.channel(i, states[i]);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

    } catch (const std::exception &e) {
        report(e);
    }
}
Window::~Window() {
    releaseBuffer();
}
void Window::releaseBuffer() {
    if (bufferDC_) {
        SelectObject(bufferDC_, bufferOld_);
        DeleteObject(bufferBitmap_);
        DeleteDC(bufferDC_);
    }
    bufferDC_ = nullptr;
    bufferBitmap_ = nullptr;
    bufferPixels_ = nullptr;
    bufferOld_ = nullptr;
    bufferWidth_ = bufferHeight_ = 0;
}
void Window::renderFrame(HDC dc, int width, int height) {
    if (!bufferDC_ || bufferWidth_ != width || bufferHeight_ != height) {
        releaseBuffer();
        bufferDC_ = CreateCompatibleDC(dc);
        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = width;
        bitmap.bmiHeader.biHeight = -height;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        void *pixels = nullptr;
        bufferBitmap_ = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
        bufferPixels_ = static_cast<unsigned long *>(pixels);
        if (!bufferDC_ || !bufferBitmap_) {
            if (bufferBitmap_)
                DeleteObject(bufferBitmap_);
            if (bufferDC_)
                DeleteDC(bufferDC_);
            bufferDC_ = nullptr;
            bufferBitmap_ = nullptr;
            paint(dc, width, height);
            for (const auto &segment : segments_)
                line(dc, int(segment.from.x * scale_) + offsetX_,
                     int(segment.from.y * scale_) + offsetY_, int(segment.to.x * scale_) + offsetX_,
                     int(segment.to.y * scale_) + offsetY_, segment.color,
                     std::max(1, int(2 * scale_)));
            return;
        }
        bufferOld_ = SelectObject(bufferDC_, bufferBitmap_);
        bufferWidth_ = width;
        bufferHeight_ = height;
    }
    paint(bufferDC_, width, height);
    // Complete background/text GDI work before touching the same DIB memory.
    GdiFlush();
    drawWaves();
    BitBlt(dc, 0, 0, width, height, bufferDC_, 0, 0, SRCCOPY);
}
void Window::drawWaves() {
    // Rasterize the actual min/max envelope directly: no per-segment kernel/GDI calls.
    const int thickness = std::max(1, int(std::round(2 * scale_)));
    for (const auto &segment : segments_) {
        int x = int(segment.from.x * scale_) + offsetX_;
        int y = int(segment.from.y * scale_) + offsetY_;
        const int endX = int(segment.to.x * scale_) + offsetX_;
        const int endY = int(segment.to.y * scale_) + offsetY_;
        const int dx = std::abs(endX - x), dy = -std::abs(endY - y);
        const int stepX = x < endX ? 1 : -1, stepY = y < endY ? 1 : -1;
        int error = dx + dy;
        const unsigned long color = (GetRValue(segment.color) << 16) |
                                    (GetGValue(segment.color) << 8) | GetBValue(segment.color);
        for (;;) {
            const int left = std::clamp(x - thickness / 2, 0, bufferWidth_);
            const int right = std::clamp(x - thickness / 2 + thickness, 0, bufferWidth_);
            const int top = std::clamp(y - thickness / 2, 0, bufferHeight_);
            const int bottom = std::clamp(y - thickness / 2 + thickness, 0, bufferHeight_);
            for (int row = top; row < bottom; ++row)
                std::fill(bufferPixels_ + size_t(row) * bufferWidth_ + left,
                          bufferPixels_ + size_t(row) * bufferWidth_ + right, color);
            if (x == endX && y == endY)
                break;
            const int twice = 2 * error;
            if (twice >= dy) {
                error += dy;
                x += stepX;
            }
            if (twice <= dx) {
                error += dx;
                y += stepY;
            }
        }
    }
}
void Window::capture() {
    RECT r;
    GetClientRect(hwnd_, &r);
    int w = r.right, h = r.bottom;
    auto dc = GetDC(hwnd_);
    auto mem = CreateCompatibleDC(dc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    auto bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    auto old = SelectObject(mem, bmp);
    wchar_t benchmark[2];
    if (GetEnvironmentVariableW(L"NGMV_RENDER_BENCHMARK", benchmark, 2)) {
        for (int i = 0; i < 10; ++i)
            renderFrame(mem, w, h);
        GdiFlush();
        auto resourcesBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        std::vector<double> durations;
        for (int i = 0; i < 120; ++i) {
            auto begin = std::chrono::steady_clock::now();
            renderFrame(mem, w, h);
            GdiFlush();
            durations.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
                    .count());
        }
        auto resourcesAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        double total = 0;
        for (double ms : durations)
            total += ms;
        std::sort(durations.begin(), durations.end());
        std::ofstream report(std::filesystem::path(screenshot_ + L".render.csv"));
        report << "width,height,frames,mean_ms,p95_ms,gdi_before,gdi_after\n"
               << w << ',' << h << ",120," << total / durations.size() << ',' << durations[113]
               << ',' << resourcesBefore << ',' << resourcesAfter << '\n';
    }
    renderFrame(mem, w, h);
    GdiFlush();
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + w * h * 4;
    std::ofstream f(std::filesystem::path(screenshot_), std::ios::binary);
    f.write(reinterpret_cast<char *>(&file), sizeof file);
    f.write(reinterpret_cast<char *>(&info.bmiHeader), sizeof(BITMAPINFOHEADER));
    f.write(static_cast<char *>(bits), w * h * 4);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd_, dc);
}
LRESULT CALLBACK Window::procedure(HWND w, UINT m, WPARAM a, LPARAM b) {
    auto *self = reinterpret_cast<Window *>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        self = static_cast<Window *>(reinterpret_cast<CREATESTRUCTW *>(b)->lpCreateParams);
        self->hwnd_ = w;
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    try {
        return self ? self->event(m, a, b) : DefWindowProcW(w, m, a, b);
    } catch (const std::exception &e) {
        if (self)
            self->report(e);
        return 0;
    }
}
LRESULT Window::event(UINT m, WPARAM a, LPARAM b) {
    switch (m) {
    case WM_COPYDATA: {
        std::wstring path;
        if (pendingFiles_.size() >= 32 || !desktop::decodeRequest(reinterpret_cast<COPYDATASTRUCT *>(b), path))
            return FALSE;
        pendingFiles_.push_back(std::move(path));
        PostMessageW(hwnd_, desktop::OpenQueuedFile, 0, 0);
        return TRUE;
    }
    case desktop::OpenQueuedFile:
        if (changing_) {
            SetTimer(hwnd_, 3, 100, nullptr);
            return 0;
        }
        if (!pendingFiles_.empty()) {
            auto path = std::move(pendingFiles_.front());
            pendingFiles_.pop_front();
            ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
            if (!SetForegroundWindow(hwnd_)) {
                FLASHWINFO flash{sizeof(FLASHWINFO), hwnd_, FLASHW_TRAY, 3, 0};
                FlashWindowEx(&flash);
            }
            try {
                if (!path.empty() && open(path)) player_.play();
            } catch (const std::exception &e) { report(e); }
            if (!pendingFiles_.empty()) PostMessageW(hwnd_, desktop::OpenQueuedFile, 0, 0);
        }
        return 0;
    case desktop::AskDefaultApps:
        try { desktop::defaultAppsPrompt(hwnd_, false, recording_); }
        catch (const std::exception &e) { report(e); }
        return 0;
    case WM_SYSCOMMAND:
        if ((a & 0xfff0) == desktop::DefaultAppsCommand) {
            try { desktop::defaultAppsPrompt(hwnd_, true, recording_); }
            catch (const std::exception &e) { report(e); }
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT r;
        GetClientRect(hwnd_, &r);
        if (r.right && r.bottom) {
            try {
                renderFrame(dc, r.right, r.bottom);
            } catch (...) {
                EndPaint(hwnd_, &ps);
                throw;
            }
        }
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_TIMER:
        if (a == 3) {
            KillTimer(hwnd_, 3);
            PostMessageW(hwnd_, desktop::OpenQueuedFile, 0, 0);
            return 0;
        }
        try {
            if (!changing_ && player_.playing() && player_.ended()) {
                bool repeat = player_.repeat();
                player_.pause();
                if (repeat) {
                    player_.stop();
                    player_.play();
                } else if (autoplay_ && playlist_.active() &&
                           playlist_.current + 1 < playlist_.files.size()) {
                    nextTrack(1, true);
                } else
                    message_ = L"Track ended";
            }
            if (a == 2) {
                KillTimer(hwnd_, 2);
                capture();
                DestroyWindow(hwnd_);
                return 0;
            }
        } catch (const std::exception &e) {
            report(e);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        click(GET_X_LPARAM(b), GET_Y_LPARAM(b));
        return 0;
    case WM_MOUSEMOVE:
        mouseMove(GET_X_LPARAM(b), GET_Y_LPARAM(b));
        return 0;
    case WM_LBUTTONUP:
        drag_ = -1;
        ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        drag_ = -1;
        return 0;
    case WM_DROPFILES: {
        auto drop = reinterpret_cast<HDROP>(a);
        wchar_t p[32768];
        DragQueryFileW(drop, 0, p, 32768);
        DragFinish(drop);
        open(p);
        return 0;
    }
    case WM_KEYDOWN:
        if (changing_)
            return 0;
        try {
            if (a == 'O' && (GetKeyState(VK_CONTROL) & 0x8000))
                openDialog();
            else if (a == VK_SPACE) {
                if (player_.playing())
                    player_.pause();
                else
                    player_.play();
            } else if (a == 'A')
                autoplay_ = !autoplay_;
            else if (a == VK_F9) {
                recording_ = !recording_;
                ModifyMenuW(GetSystemMenu(hwnd_, FALSE), desktop::DefaultAppsCommand,
                    MF_BYCOMMAND | MF_STRING, desktop::DefaultAppsCommand,
                    recording_ ? L"Default apps... (F10)" : L"기본 앱 설정... (F10)");
            }
            else if (a == VK_F10)
                desktop::defaultAppsPrompt(hwnd_, true, recording_);
            else if (a == VK_F11)
                toggleFullscreen();
            else if (a == VK_ESCAPE && fullscreen_)
                toggleFullscreen();
            else if (a == VK_OEM_PLUS)
                windowSamples_ = std::max(256, windowSamples_ / 2);
            else if (a == VK_OEM_MINUS)
                windowSamples_ = std::min(4096, windowSamples_ * 2);
            else if (a == VK_LEFT && player_.loaded())
                nextTrack(-1);
            else if (a == VK_RIGHT && player_.loaded())
                nextTrack(1);
        } catch (const std::exception &e) {
            report(e);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        auto p = reinterpret_cast<MINMAXINFO *>(b);
        p->ptMinTrackSize = {800, 490};
        p->ptMaxTrackSize = {8192, 8192};
        return 0;
    }
    case WM_DESTROY:
        RemovePropW(hwnd_, desktop::InstanceProperty);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, m, a, b);
}
int Window::run(HINSTANCE instance, const std::filesystem::path &initial,
                const std::filesystem::path &screenshot, int width, int height, bool recording, bool playInitial) {
    SetProcessDPIAware();
    recording_ = recording;
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpszClassName = desktop::WindowClass;
    wc.lpfnWndProc = procedure;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    RegisterClassW(&wc);
    RECT r{0, 0, width, height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindowW(wc.lpszClassName, L"Nintendo Game Music Viewer · V1.3.1",
                          WS_OVERLAPPEDWINDOW, screenshot.empty() ? CW_USEDEFAULT : -20000,
                          screenshot.empty() ? CW_USEDEFAULT : -20000, r.right - r.left,
                          r.bottom - r.top, nullptr, nullptr, instance, this);
    if (!hwnd_)
        throw std::runtime_error("Cannot create window.");
    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
    auto smallIcon = LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    if (screenshot.empty()) {
        if (!SetPropW(hwnd_, desktop::InstanceProperty, reinterpret_cast<HANDLE>(desktop::instanceToken())))
            throw std::runtime_error("Cannot initialize file handoff.");
        auto menu = GetSystemMenu(hwnd_, FALSE);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, desktop::DefaultAppsCommand,
                    recording_ ? L"Default apps... (F10)" : L"기본 앱 설정... (F10)");
    }
    DragAcceptFiles(hwnd_, TRUE);
    ShowWindow(hwnd_, SW_SHOW);
    if (!screenshot.empty()) {
        SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
                     SWP_NOZORDER | SWP_NOMOVE);
    }
    SetTimer(hwnd_, 1, 16, nullptr);
    if (!initial.empty() && open(initial) && playInitial)
        player_.play();
    if (screenshot.empty()) PostMessageW(hwnd_, desktop::AskDefaultApps, 0, 0);
    if (!screenshot.empty()) {
        if (player_.loaded())
            player_.play();
        screenshot_ = screenshot.wstring();
        SetTimer(hwnd_, 2, 1200, nullptr);
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return int(msg.wParam);
}
} // namespace ngmv
