#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <mfapi.h>
#include <mfplay.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <iterator>
#include <string>

namespace {
constexpr wchar_t kWindowClass[] = L"KaviroPlayerWindow";
constexpr int kOpenId = 1001;
constexpr int kPlayId = 1002;
constexpr int kStopId = 1003;
constexpr int kSeekId = 1004;
constexpr int kStatusId = 1005;
constexpr UINT_PTR kTimerId = 1;
HWND g_window = nullptr;
HWND g_video = nullptr;
HWND g_status = nullptr;
HWND g_seek = nullptr;
IMFPMediaPlayer* g_player = nullptr;
bool g_dragging = false;
std::wstring g_currentPath;

void setStatus(const std::wstring& text) {
    if (g_status) SetWindowTextW(g_status, text.c_str());
}

void layoutControls(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    constexpr int row = 38;
    constexpr int margin = 8;
    const int controlsY = std::max(0, height - row);
    if (g_video) MoveWindow(g_video, 0, 0, width, controlsY, TRUE);
    if (GetDlgItem(hwnd, kOpenId)) MoveWindow(GetDlgItem(hwnd, kOpenId), margin, controlsY + 5, 78, 28, TRUE);
    if (GetDlgItem(hwnd, kPlayId)) MoveWindow(GetDlgItem(hwnd, kPlayId), 92, controlsY + 5, 78, 28, TRUE);
    if (GetDlgItem(hwnd, kStopId)) MoveWindow(GetDlgItem(hwnd, kStopId), 176, controlsY + 5, 64, 28, TRUE);
    if (g_seek) MoveWindow(g_seek, 248, controlsY + 7, std::max(40, width - 420), 24, TRUE);
    if (g_status) MoveWindow(g_status, std::max(250, width - 164), controlsY + 5, 156, 28, TRUE);
}

void releasePlayer() {
    KillTimer(g_window, kTimerId);
    if (g_player) {
        g_player->Stop();
        g_player->Shutdown();
        g_player->Release();
        g_player = nullptr;
    }
}

void openMedia(HWND hwnd) {
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.lpstrFilter = L"Media files\0*.mp4;*.mkv;*.mov;*.avi;*.webm;*.mp3;*.m4a;*.wav;*.flac;*.aac;*.ogg;*.wma\0All files\0*.*\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrTitle = L"Open media in KAVIRO Player";
    if (!GetOpenFileNameW(&dialog)) return;

    wchar_t url[32768]{};
    DWORD urlLength = static_cast<DWORD>(std::size(url));
    if (UrlCreateFromPathW(path, url, &urlLength, 0) != S_OK) {
        setStatus(L"Could not prepare file URL");
        return;
    }
    IMFPMediaPlayer* candidate = nullptr;
    const HRESULT hr = MFPCreateMediaPlayer(url, TRUE, 0, nullptr, g_video, &candidate);
    if (FAILED(hr) || !candidate) {
        setStatus(L"Open failed (codec or file)");
        return;
    }
    releasePlayer();
    g_player = candidate;
    g_currentPath = path;
    const auto slash = g_currentPath.find_last_of(L"\\/");
    const auto title = slash == std::wstring::npos ? g_currentPath : g_currentPath.substr(slash + 1);
    SetWindowTextW(hwnd, (L"KAVIRO Player — " + title).c_str());
    setStatus(L"Playing");
    layoutControls(hwnd);
    PROPVARIANT duration;
    PropVariantInit(&duration);
    if (SUCCEEDED(g_player->GetDuration(&duration)) && duration.vt == VT_I8 && duration.hVal.QuadPart > 0) {
        const auto seconds = static_cast<int>(std::min<LONGLONG>(duration.hVal.QuadPart / 10000000LL, 2147483647LL));
        SendMessageW(g_seek, TBM_SETRANGE, TRUE, MAKELONG(0, std::max(1, seconds)));
        SendMessageW(g_seek, TBM_SETPOS, TRUE, 0);
    }
    PropVariantClear(&duration);
    SetTimer(hwnd, kTimerId, 250, nullptr);
}

void seekToSlider() {
    if (!g_player || !g_seek) return;
    PROPVARIANT position;
    PropVariantInit(&position);
    position.vt = VT_I8;
    position.hVal.QuadPart = static_cast<LONGLONG>(SendMessageW(g_seek, TBM_GETPOS, 0, 0)) * 10000000LL;
    const HRESULT hr = g_player->SetPosition(GUID_NULL, &position);
    PropVariantClear(&position);
    if (FAILED(hr)) setStatus(L"Seek failed");
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_window = hwnd;
        g_video = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 0, 0, 640, 420, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Open file", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 8, 430, 78, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpenId)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Play / Pause", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 92, 430, 78, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlayId)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 176, 430, 64, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStopId)), GetModuleHandleW(nullptr), nullptr);
        g_seek = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 248, 432, 280, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSeekId)), GetModuleHandleW(nullptr), nullptr);
        g_status = CreateWindowExW(0, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 540, 430, 156, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusId)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_seek, TBM_SETRANGE, TRUE, MAKELONG(0, 1));
        layoutControls(hwnd);
        return 0;
    case WM_SIZE:
        layoutControls(hwnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kOpenId: openMedia(hwnd); return 0;
        case kPlayId:
            if (g_player) {
                MFP_MEDIAPLAYER_STATE state{};
                if (SUCCEEDED(g_player->GetState(&state))) {
                    if (state == MFP_MEDIAPLAYER_STATE_PLAYING) { g_player->Pause(); setStatus(L"Paused"); }
                    else { g_player->Play(); setStatus(L"Playing"); }
                }
            } else openMedia(hwnd);
            return 0;
        case kStopId:
            if (g_player) { g_player->Stop(); setStatus(L"Stopped"); SendMessageW(g_seek, TBM_SETPOS, TRUE, 0); }
            return 0;
        default: break;
        }
        break;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == g_seek) {
            const int code = LOWORD(wParam);
            if (code == TB_THUMBTRACK) g_dragging = true;
            if (code == TB_ENDTRACK || code == TB_THUMBPOSITION) { g_dragging = false; seekToSlider(); }
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId && g_player && !g_dragging) {
            PROPVARIANT position;
            PropVariantInit(&position);
            if (SUCCEEDED(g_player->GetPosition(&position)) && position.vt == VT_I8 && position.hVal.QuadPart >= 0)
                SendMessageW(g_seek, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::min<LONGLONG>(position.hVal.QuadPart / 10000000LL, 2147483647LL)));
            PropVariantClear(&position);
        }
        return 0;
    case WM_DESTROY:
        releasePlayer();
        MFShutdown();
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return 1;
    const HRESULT mf = MFStartup(MF_VERSION);
    if (FAILED(mf)) { if (SUCCEEDED(com)) CoUninitialize(); return 2; }
    if (commandLine && std::wcsstr(commandLine, L"--smoke-test")) {
        MFShutdown();
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = windowProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) { MFShutdown(); if (SUCCEEDED(com)) CoUninitialize(); return 3; }
    HWND hwnd = CreateWindowExW(0, kWindowClass, L"KAVIRO Player", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 640, nullptr, nullptr, instance, nullptr);
    if (!hwnd) { MFShutdown(); if (SUCCEEDED(com)) CoUninitialize(); return 4; }
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    if (SUCCEEDED(com)) CoUninitialize();
    return static_cast<int>(message.wParam);
}
