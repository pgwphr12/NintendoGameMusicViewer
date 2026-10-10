#pragma once
#include "player/FolderPlaylist.hpp"
#include "player/Player.hpp"
#include <filesystem>
#include <deque>
#include <windows.h>
namespace ngmv {
class Window {
    HWND hwnd_ = nullptr;
    HDC bufferDC_ = nullptr;
    HBITMAP bufferBitmap_ = nullptr;
    HGDIOBJ bufferOld_ = nullptr;
    int bufferWidth_ = 0, bufferHeight_ = 0;
    struct WaveSegment {
        POINT from, to;
        COLORREF color;
    };
    std::vector<WaveSegment> segments_;
    unsigned long *bufferPixels_ = nullptr;
    void drawWaves();
    void releaseBuffer();
    Player player_;
    std::filesystem::path file_;
    FolderPlaylist playlist_;
    bool changing_ = false;
    std::deque<std::wstring> pendingFiles_;
    void selectFolder(size_t index, bool play);
    void nextTrack(int direction, bool automatic = false);
    bool recording_ = false, fullscreen_ = false;
    bool autoplay_ = true;
    int windowSamples_ = 2048;
    int speedStep_ = 10;
    RECT oldRect_{};
    std::wstring message_ = L"";
    double scale_ = 1;
    int offsetX_ = 0, offsetY_ = 0;
    int drag_ = -1;
    std::wstring screenshot_;
    static LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT event(UINT, WPARAM, LPARAM);
    std::vector<size_t> visibleChannels() const;
    void paint(HDC, int, int);
    void renderFrame(HDC, int, int);
    void click(int, int);
    void mouseMove(int, int);
    void openDialog();
    bool open(const std::filesystem::path &);
    void tracksMenu();
    void rateMenu();
    void toggleFullscreen();
    void report(const std::exception &e);
    void capture();

  public:
    ~Window();
    int run(HINSTANCE, const std::filesystem::path &initial = {},
            const std::filesystem::path &screenshot = {}, int width = 1280, int height = 720,
            bool recording = false, bool playInitial = false);
};
std::wstring wide(const std::string &s);
} // namespace ngmv
