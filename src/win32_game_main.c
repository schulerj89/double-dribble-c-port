#define UNICODE
#define _UNICODE

#include "dd_asset_pack.h"
#include "dd_audio.h"
#include "dd_renderer.h"

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

static DDAssetPack g_pack;
static uint32_t *g_pixels;
static uint8_t *g_wav;
static size_t g_wav_size;
static BITMAPINFO g_bitmap_info;

static LRESULT CALLBACK dd_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)wparam;
    (void)lparam;
    switch (message) {
        case WM_TIMER:
            KillTimer(window, 1u);
            if (g_wav != NULL) {
                PlaySoundA((LPCSTR)g_wav, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT client;
            HDC dc = BeginPaint(window, &paint);
            GetClientRect(window, &client);
            SetStretchBltMode(dc, COLORONCOLOR);
            StretchDIBits(dc, 0, 0, client.right, client.bottom,
                          0, 0, (int)g_pack.meta.width, (int)g_pack.meta.height,
                          g_pixels, &g_bitmap_info, DIB_RGB_COLORS, SRCCOPY);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            PlaySoundW(NULL, NULL, 0);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command) {
    int argc;
    LPWSTR *argv;
    char pack_path[MAX_PATH];
    WNDCLASSW window_class;
    HWND window;
    MSG message;
    RECT rect = {0, 0, 768, 720};
    (void)previous;
    (void)command_line;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc != 2 || WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, pack_path, MAX_PATH, NULL, NULL) == 0) {
        MessageBoxW(NULL, L"Usage: double_dribble_game.exe <title.assetpack>", L"Double Dribble", MB_ICONERROR);
        LocalFree(argv);
        return 2;
    }
    LocalFree(argv);
    if (!dd_asset_pack_load(pack_path, &g_pack)) {
        MessageBoxW(NULL, L"The asset pack is missing, corrupt, or from an unsupported ROM.", L"Double Dribble", MB_ICONERROR);
        return 1;
    }
    g_pixels = (uint32_t *)malloc((size_t)g_pack.meta.width * g_pack.meta.height * sizeof(uint32_t));
    if (g_pixels == NULL || !dd_render_title(&g_pack, g_pixels, g_pack.meta.width, g_pack.meta.height) ||
        !dd_build_title_wav(&g_pack, &g_wav, &g_wav_size)) {
        dd_asset_pack_unload(&g_pack);
        free(g_pixels);
        free(g_wav);
        return 1;
    }
    ZeroMemory(&g_bitmap_info, sizeof(g_bitmap_info));
    g_bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bitmap_info.bmiHeader.biWidth = (LONG)g_pack.meta.width;
    g_bitmap_info.bmiHeader.biHeight = -(LONG)g_pack.meta.height;
    g_bitmap_info.bmiHeader.biPlanes = 1;
    g_bitmap_info.bmiHeader.biBitCount = 32;
    g_bitmap_info.bmiHeader.biCompression = BI_RGB;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = dd_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"DoubleDribbleNativePort";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&window_class);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    window = CreateWindowW(window_class.lpszClassName, L"Double Dribble - Native C Port",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           rect.right - rect.left, rect.bottom - rect.top,
                           NULL, NULL, instance, NULL);
    if (window == NULL) return 1;
    ShowWindow(window, show_command);
    SetTimer(window, 1u, (UINT)((1000u * g_pack.meta.spoken_frame) / 60u), NULL);
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    free(g_pixels);
    free(g_wav);
    dd_asset_pack_unload(&g_pack);
    return 0;
}
