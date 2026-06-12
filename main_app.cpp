#include "v8k_common.h"

int RunApp(HINSTANCE hInst, int nCmdShow) {
    g_hInst = hInst;
    QueryPerformanceFrequency(&g_qpcFreq);
    QueryPerformanceCounter(&g_engineHeartbeatQpc);
    LoadDynamicApis();
    LoadLicenseState();
    DetectHardware();
    ZeroMemory(&g_stats, sizeof(g_stats));
    g_stats.latencyMinUs = 0.0;
    g_stats.latencyMaxUs = 0.0;
    g_stats.latencyAvgUs = 0.0;
    SetWarning(L"Engine ready");

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(0, MAKEINTRESOURCEW(32512));
    wc.hIcon = LoadIconW(0, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = V8K_CLASS_NAME;
    RegisterClassExW(&wc);

    g_rawEvent = CreateEventW(0, FALSE, FALSE, 0);
    g_rawReady = CreateEventW(0, TRUE, FALSE, 0);
    g_rawThread = CreateThread(0, 0, RawThreadProc, 0, 0, 0);
    if (g_rawThread && g_rawReady) WaitForSingleObject(g_rawReady, 1000);
    g_engineThread = CreateThread(0, 0, EngineThreadProc, 0, 0, 0);

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, V8K_CLASS_NAME, L"Virtual 8K Pro", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 680, 0, 0, hInst, 0);
    g_hWnd = hwnd;
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

