#include "v8k_common.h"

ULONGLONG FtToU64(FILETIME ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

void UpdateCpuUsage() {
    static LARGE_INTEGER lastOutputQpc = {0};
    static LONG lastInjected = 0;
    LARGE_INTEGER nowQpc;
    QueryPerformanceCounter(&nowQpc);
    if (lastOutputQpc.QuadPart) {
        double dt = QpcElapsedUs(lastOutputQpc, nowQpc);
        LONG curInjected = InterlockedCompareExchange(&g_stats.injectedEvents, 0, 0);
        LONG diff = curInjected - lastInjected;
        if (dt > 1000.0) {
            double hz = (double)diff * 1000000.0 / dt;
            if (g_stats.outputHzMeasured <= 0.0) g_stats.outputHzMeasured = hz;
            else g_stats.outputHzMeasured = g_stats.outputHzMeasured * 0.70 + hz * 0.30;
        }
        lastInjected = curInjected;
    }
    lastOutputQpc = nowQpc;

    FILETIME idle, kernel, user, c, e, pk, pu;
    if (!GetSystemTimes(&idle, &kernel, &user)) return;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &pk, &pu)) return;
    CpuSample cur;
    cur.sysIdle = FtToU64(idle);
    cur.sysKernel = FtToU64(kernel);
    cur.sysUser = FtToU64(user);
    cur.procKernel = FtToU64(pk);
    cur.procUser = FtToU64(pu);
    cur.valid = TRUE;
    if (g_cpuLast.valid) {
        ULONGLONG sys = (cur.sysKernel - g_cpuLast.sysKernel) + (cur.sysUser - g_cpuLast.sysUser);
        ULONGLONG proc = (cur.procKernel - g_cpuLast.procKernel) + (cur.procUser - g_cpuLast.procUser);
        if (sys > 0) {
            g_stats.cpuPercent = ClampD((double)proc * 100.0 / (double)sys, 0.0, 100.0);
        }
    }
    g_cpuLast = cur;
}

void DrawFilledRect(HDC dc, RECT rc, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &rc, b);
    DeleteObject(b);
}

void DrawBorder(HDC dc, RECT rc, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, rc.left, rc.top, 0);
    LineTo(dc, rc.right - 1, rc.top);
    LineTo(dc, rc.right - 1, rc.bottom - 1);
    LineTo(dc, rc.left, rc.bottom - 1);
    LineTo(dc, rc.left, rc.top);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawTextIn(HDC dc, RECT rc, const WCHAR* text, COLORREF color, UINT flags) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rc, flags);
}

void DrawMetric(HDC dc, int x, int y, int w, int h, const WCHAR* label, const WCHAR* value, COLORREF accent) {
    RECT rc = {x, y, x + w, y + h};
    DrawFilledRect(dc, rc, RGB(31, 34, 39));
    DrawBorder(dc, rc, RGB(54, 59, 67));
    RECT lab = {x + 12, y + 8, x + w - 12, y + 28};
    RECT val = {x + 12, y + 30, x + w - 12, y + h - 8};
    SelectObject(dc, g_font);
    DrawTextIn(dc, lab, label, RGB(160, 168, 178), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, val, value, accent, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void DrawButton(HDC dc, RECT rc, const WCHAR* text, BOOL active) {
    DrawFilledRect(dc, rc, active ? RGB(38, 104, 92) : RGB(42, 47, 55));
    DrawBorder(dc, rc, active ? RGB(73, 210, 168) : RGB(77, 85, 96));
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, rc, text, active ? RGB(230, 255, 248) : RGB(230, 234, 240), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void DrawTabs(HDC dc, RECT client) {
    const WCHAR* names[] = {L"Genel", L"Gelismis", L"Donanim (Diag)", L"Testler", L"Lisans"};
    int x = 18;
    for (int i = 0; i < 5; ++i) {
        RECT r = {x, 18, x + 124, 52};
        DrawFilledRect(dc, r, i == g_activeTab ? RGB(41, 47, 56) : RGB(22, 24, 29));
        if (i == g_activeTab) {
            RECT line = {r.left, r.bottom - 3, r.right, r.bottom};
            DrawFilledRect(dc, line, RGB(226, 40, 200));
        }
        SelectObject(dc, g_fontBold);
        DrawTextIn(dc, r, names[i], i == g_activeTab ? RGB(245, 248, 250) : RGB(148, 156, 168), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        x += 130;
    }
    RECT sep = {0, 58, client.right, 59};
    DrawFilledRect(dc, sep, RGB(42, 46, 53));
}

void DrawGraph(HDC dc, RECT rc) {
    DrawFilledRect(dc, rc, RGB(24, 27, 32));
    DrawBorder(dc, rc, RGB(54, 59, 67));
    HPEN grid = CreatePen(PS_SOLID, 1, RGB(35, 39, 46));
    HGDIOBJ old = SelectObject(dc, grid);
    for (int i = 1; i < 4; ++i) {
        int y = rc.top + (rc.bottom - rc.top) * i / 4;
        MoveToEx(dc, rc.left, y, 0);
        LineTo(dc, rc.right, y);
    }
    SelectObject(dc, old);
    DeleteObject(grid);
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(79, 209, 197));
    old = SelectObject(dc, pen);
    int lastX = rc.left;
    int lastY = rc.bottom - 6;
    for (int i = 0; i < V8K_GRAPH_SAMPLES; ++i) {
        int idx = (g_stats.graphHead + i) % V8K_GRAPH_SAMPLES;
        double v = ClampD(g_stats.graph[idx] / 5.0, 0.0, 1.0);
        int x = rc.left + 6 + (rc.right - rc.left - 12) * i / (V8K_GRAPH_SAMPLES - 1);
        int y = rc.bottom - 6 - (int)((rc.bottom - rc.top - 12) * v);
        if (i == 0) MoveToEx(dc, x, y, 0);
        else LineTo(dc, x, y);
        lastX = x;
        lastY = y;
    }
    MoveToEx(dc, lastX, lastY, 0);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawDashboard(HDC dc, RECT client) {
    WCHAR buf[128];
    RECT top = {24, 76, client.right - 24, 122};
    g_startButton = {client.right - 176, 78, client.right - 24, 116};
    g_modeButton = {client.right - 340, 78, client.right - 188, 116};
    g_syncButton = {client.right - 504, 78, client.right - 352, 116};
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, top, V8K_APP_NAME, RGB(242, 245, 248), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    DrawButton(dc, g_startButton, InterlockedCompareExchange(&g_settings.enabled, 0, 0) ? L"STOP" : L"START", InterlockedCompareExchange(&g_settings.enabled, 0, 0));
    DrawButton(dc, g_modeButton, InterlockedCompareExchange(&g_settings.highPerformance, 0, 0) ? L"TURBO 8K" : L"SMOOTH", InterlockedCompareExchange(&g_settings.highPerformance, 0, 0));
    DrawButton(dc, g_syncButton, InterlockedCompareExchange(&g_settings.syncToDwm, 0, 0) ? L"VSYNC ON" : L"VSYNC OFF", InterlockedCompareExchange(&g_settings.syncToDwm, 0, 0));
    RECT hint = {24, 118, client.right - 24, 138};
    SelectObject(dc, g_font);
    DrawTextIn(dc, hint, L"START + TURBO 8K: dusuk gecikme, 8 adim burst | TEK HAREKET: oyunda ac | SMOOTH: spline (masaustu)", RGB(145, 155, 168), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    int y = 142;
    int w = (client.right - 72) / 3;
    swprintf(buf, 128, L"%.0f / %.0f Hz", g_stats.physicalHz, g_stats.physicalHzPeak);
    DrawMetric(dc, 24, y, w, 74, L"Physical Avg / Peak", buf, RGB(240, 246, 255));
    double outHz = g_stats.outputHzMeasured > 1.0 ? g_stats.outputHzMeasured :
        (g_stats.virtualHz > 1.0 ? g_stats.virtualHz : ComputeVirtualHz(g_stats.physicalHzPeak));
    swprintf(buf, 128, L"%.0f Hz", outHz);
    DrawMetric(dc, 36 + w, y, w, 74, L"Virtual Output Hz", buf, RGB(79, 209, 197));
    swprintf(buf, 128, L"%.1f / %.1f / %.1f us", g_stats.latencyMinUs, g_stats.latencyAvgUs, g_stats.latencyMaxUs);
    DrawMetric(dc, 48 + w * 2, y, w, 74, L"Latency Min/Avg/Max", buf, RGB(255, 205, 112));
    y += 90;
    swprintf(buf, 128, L"%.1f%%", g_stats.cpuPercent);
    DrawMetric(dc, 24, y, w, 74, L"CPU", buf, RGB(190, 225, 255));
    swprintf(buf, 128, L"%dx microstep", ComputeMicroSteps(g_stats.physicalHz));
    DrawMetric(dc, 36 + w, y, w, 74, L"Interpolation Ratio", buf, RGB(210, 190, 255));
    swprintf(buf, 128, L"%.3f m/s", g_stats.speedMetersPerSec);
    DrawMetric(dc, 48 + w * 2, y, w, 74, L"Cursor Speed", buf, RGB(255, 238, 180));
    RECT graph = {24, y + 96, client.right - 24, client.bottom - 74};
    DrawGraph(dc, graph);
    RECT status = {24, client.bottom - 54, client.right - 24, client.bottom - 24};
    WCHAR statusText[256];
    if (!LicenseAllowsEngine()) swprintf(statusText, 256, L"%s", g_license.status);
    else if (InterlockedCompareExchange(&g_stats.safeMode, 0, 0)) swprintf(statusText, 256, L"%s", g_stats.warning);
    else swprintf(statusText, 256, L"Engine ready | %s | %s", g_hw.gpuVendor, g_hw.syncStatus);
    DrawFilledRect(dc, status, RGB(24, 27, 32));
    DrawBorder(dc, status, RGB(54, 59, 67));
    DrawTextIn(dc, status, statusText, LicenseAllowsEngine() ? RGB(184, 228, 215) : RGB(255, 170, 140), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void DrawSlider(HDC dc, int idx, int x, int y, int w, const WCHAR* label, const WCHAR* desc, double value, double minV, double maxV) {
    RECT row = {x, y, x + w, y + 70};
    RECT lab = {x, y, x + (w * 42) / 100, y + 24};
    RECT help = {x + (w * 44) / 100, y, x + w, y + 24};
    WCHAR text[128];
    swprintf(text, 128, L"%s  %.4f", label, value);
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, lab, text, RGB(230, 235, 240), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, g_font);
    DrawTextIn(dc, help, desc, RGB(150, 160, 174), DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    RECT track = {x, y + 42, x + w, y + 50};
    g_sliderRects[idx] = track;
    DrawFilledRect(dc, track, RGB(45, 51, 60));
    double t = (value - minV) / (maxV - minV);
    t = ClampD(t, 0.0, 1.0);
    RECT fill = track;
    fill.right = fill.left + (int)((fill.right - fill.left) * t);
    DrawFilledRect(dc, fill, RGB(79, 209, 197));
    int kx = track.left + (int)((track.right - track.left) * t);
    RECT knob = {kx - 7, track.top - 7, kx + 7, track.bottom + 7};
    DrawFilledRect(dc, knob, RGB(230, 245, 242));
    DrawBorder(dc, row, RGB(32, 36, 43));
}

void DrawAdvanced(HDC dc, RECT client) {
    RECT head = {24, 78, client.right - 24, 116};
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, head, L"Gelismis Ayarlar", RGB(242, 245, 248), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    int x = 36;
    int y = 138;
    int w = client.right - 72;
    DrawSlider(dc, 0, x, y, w, L"Kalman Q", L"Tahmin gurultusu: yuksek deger daha hizli tepki verir", g_settings.kalmanQ, 0.0001, 0.20);
    DrawSlider(dc, 1, x, y + 86, w, L"Kalman R", L"Sensor gurultusu: yuksek deger daha cok yumusatir", g_settings.kalmanR, 0.0010, 2.00);
    DrawSlider(dc, 2, x, y + 172, w, L"Spline Tension", L"Egri gerginligi: dusuk deger daha yumusak hareket verir", g_settings.splineTension, 0.0, 1.0);
    DrawSlider(dc, 3, x, y + 258, w, L"VSync Offset us", L"Zamanlama ince ayari; DWM beklemesi yapmaz", g_settings.syncOffsetUs, -500.0, 500.0);
    DrawSlider(dc, 4, x, y + 344, w, L"Sanal Hz Limiti (Oyun/Drop Uyumu)", L"Roblox/Eski motorlarda FPS dusmemesi icin 2K/4K yapin", g_settings.virtualHzLimit, 1000.0, 8000.0);
    
    RECT autoBtn = {x, y + 420, x + 180, y + 452};
    DrawButton(dc, autoBtn, L"OTO-OPTI MIZE ET", FALSE);
    RECT testBtn = {x + 192, y + 420, client.right - 36, y + 452};
    DrawButton(dc, testBtn, InterlockedCompareExchange(&g_settings.testMode, 0, 0) ? L"TEST MODU ACIK (Kasma Yapabilir)" : L"TEST MODU KAPALI (Onerilen)", InterlockedCompareExchange(&g_settings.testMode, 0, 0));
    RECT supBtn = {x, y + 458, client.right - 36, y + 490};
    DrawButton(dc, supBtn, InterlockedCompareExchange(&g_settings.suppressLegacyMove, 0, 0) ? L"TEK HAREKET (OYUN): ACIK" : L"TEK HAREKET (OYUN): KAPALI", InterlockedCompareExchange(&g_settings.suppressLegacyMove, 0, 0));

    RECT info = {36, y + 504, client.right - 36, y + 576};
    DrawFilledRect(dc, info, RGB(24, 27, 32));
    DrawBorder(dc, info, RGB(54, 59, 67));
    WCHAR text[256];
    swprintf(text, 256, L"Hedef limit: %.0f Hz | Dinamik oran: %dx | Windows hiz: %d/20 | Accel: %d", g_settings.virtualHzLimit, ComputeMicroSteps(g_stats.physicalHz), g_pointer.speed, g_pointer.acceleration);
    DrawTextIn(dc, info, text, RGB(178, 190, 202), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void DrawDiagnostics(HDC dc, RECT client) {
    RECT rc = {30, 86, client.right - 30, client.bottom - 30};
    DrawFilledRect(dc, rc, RGB(24, 27, 32));
    DrawBorder(dc, rc, RGB(54, 59, 67));
    int y = rc.top + 18;
    WCHAR line[512];
    SelectObject(dc, g_fontBold);
    RECT r = {rc.left + 18, y, rc.right - 18, y + 28};
    DrawTextIn(dc, r, L"Hardware Diagnostics", RGB(242, 245, 248), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    y += 44;
    SelectObject(dc, g_font);
    int g = GcdI(g_hw.width, g_hw.height);
    swprintf(line, 512, L"Monitor: %s  |  %dx%d  %.3f Hz  %d bpp  aspect %d:%d", g_hw.monitorDevice, g_hw.width, g_hw.height, g_hw.refreshHz, g_hw.bpp, g_hw.width / g, g_hw.height / g);
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 30;
    swprintf(line, 512, L"Physical size: %d x %d mm  |  Pixel clock: %.3f MHz", g_hw.physicalWidthMm, g_hw.physicalHeightMm, g_hw.pixelClockMHz);
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 30;
    swprintf(line, 512, L"GPU: %s  |  Vendor: %s", g_hw.gpuName, g_hw.gpuVendor);
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 30;
    swprintf(line, 512, L"Sync: %s  |  %s", g_hw.syncStatus, g_hw.mpoStatus);
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 30;
    swprintf(line, 512, L"Mouse devices: %d  |  Active raw path: %s", g_hw.mouseCount, g_hw.mouseName);
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 30;
    swprintf(line, 512, L"Windows pointer: speed %d/20  |  thresholds %d/%d  |  acceleration %d", g_pointer.speed, g_pointer.threshold1, g_pointer.threshold2, g_pointer.acceleration);
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 30;
    swprintf(line, 512, L"Gercek olaylar: %ld  |  Sanal olaylar: %ld  |  Kuyruk: %d  |  Is parcacigi: %s", g_stats.rawEvents, g_stats.injectedEvents, QueueDepth(), g_rawThread ? L"aktif" : L"kapali");
    r = {rc.left + 18, y, rc.right - 18, y + 24};
    DrawTextIn(dc, r, line, RGB(218, 224, 232), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawTests(HDC dc, RECT client) {
    RECT head = {24, 78, client.right - 24, 116};
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, head, L"Latency Testleri", RGB(242, 245, 248), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    int w = (client.right - 72) / 3;
    WCHAR buf[160];
    int y = 136;
    swprintf(buf, 160, L"%.1f / %.1f / %.1f us", g_stats.rawPeriodMinUs, g_stats.rawPeriodAvgUs, g_stats.rawPeriodMaxUs);
    DrawMetric(dc, 24, y, w, 78, L"Polling Period Min/Avg/Max", buf, RGB(226, 40, 200));
    swprintf(buf, 160, L"%.0f Hz", g_stats.rawPeriodAvgUs > 0.0 ? 1000000.0 / g_stats.rawPeriodAvgUs : 0.0);
    DrawMetric(dc, 36 + w, y, w, 78, L"Measured Polling Rate", buf, RGB(240, 246, 255));
    swprintf(buf, 160, L"%.1f us", g_stats.engineAgeUs);
    DrawMetric(dc, 48 + w * 2, y, w, 78, L"Raw -> Engine Age", buf, RGB(255, 205, 112));
    y += 96;
    swprintf(buf, 160, L"%.0f Hz", g_stats.outputHzMeasured);
    DrawMetric(dc, 24, y, w, 78, L"Measured Output Rate", buf, RGB(210, 190, 255));
    swprintf(buf, 160, L"%ld / %ld", g_stats.coalescedEvents, g_stats.droppedEvents);
    DrawMetric(dc, 36 + w, y, w, 78, L"Coalesced / Dropped", buf, RGB(255, 238, 180));
    swprintf(buf, 160, L"%d", QueueDepth());
    DrawMetric(dc, 48 + w * 2, y, w, 78, L"Queue Depth", buf, RGB(190, 225, 255));
    y += 96;
    swprintf(buf, 160, L"%.1f us", g_stats.m1HookLatencyUs);
    DrawMetric(dc, 24, y, w, 78, L"M1 Hook Age", buf, RGB(210, 190, 255));
    swprintf(buf, 160, L"%.1f us", g_stats.m1WindowLatencyUs);
    DrawMetric(dc, 36 + w, y, w, 78, L"M1 Hook -> Window", buf, RGB(255, 238, 180));
    swprintf(buf, 160, L"%ld clicks", g_stats.m1Events);
    DrawMetric(dc, 48 + w * 2, y, w, 78, L"M1 Samples", buf, RGB(190, 225, 255));
    y += 104;
    RECT note = {24, y, client.right - 24, y + 76};
    DrawFilledRect(dc, note, RGB(24, 27, 32));
    DrawBorder(dc, note, RGB(54, 59, 67));
    DrawTextIn(dc, note, L"8K icin: START + HIGH PERF + Sanal Hz 8000. TEK HAREKET oyunlarda cift hizi onler. Raw oncelik guvenlidir; NOLEGACY kaldirildi. Performans sadece START sonrasi aktif.", RGB(178, 190, 202), DT_LEFT | DT_WORDBREAK | DT_VCENTER);
    RECT graph = {24, y + 98, client.right - 24, client.bottom - 34};
    DrawGraph(dc, graph);
}

void LayoutLicenseControls(RECT client) {
    MoveWindow(g_hEmailEdit, 188, 168, client.right - 240, 28, TRUE);
    MoveWindow(g_hKeyEdit, 188, 214, client.right - 240, 28, TRUE);
    BOOL show = (g_activeTab == 4);
    ShowWindow(g_hEmailEdit, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hKeyEdit, show ? SW_SHOW : SW_HIDE);
}

void DrawLicense(HDC dc, RECT client) {
    LayoutLicenseControls(client);
    RECT rc = {30, 86, client.right - 30, client.bottom - 30};
    DrawFilledRect(dc, rc, RGB(24, 27, 32));
    DrawBorder(dc, rc, RGB(54, 59, 67));
    RECT title = {rc.left + 18, rc.top + 20, rc.right - 18, rc.top + 52};
    SelectObject(dc, g_fontBold);
    DrawTextIn(dc, title, L"License", RGB(242, 245, 248), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    RECT status = {rc.left + 18, rc.top + 64, rc.right - 18, rc.top + 96};
    DrawTextIn(dc, status, g_license.status, g_license.pro ? RGB(126, 238, 206) : RGB(255, 205, 112), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    RECT l1 = {rc.left + 18, 168, 176, 196};
    RECT l2 = {rc.left + 18, 214, 176, 242};
    SelectObject(dc, g_font);
    DrawTextIn(dc, l1, L"Email", RGB(190, 199, 210), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    DrawTextIn(dc, l2, L"License Key", RGB(190, 199, 210), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    g_licenseButton = {188, 268, 352, 306};
    DrawButton(dc, g_licenseButton, L"ACTIVATE", g_license.pro);
}

void PaintUi(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    DrawFilledRect(mem, client, RGB(10, 10, 12));
    DrawTabs(mem, client);
    if (g_activeTab != 4) LayoutLicenseControls(client);
    if (g_activeTab == 0) DrawDashboard(mem, client);
    else if (g_activeTab == 1) DrawAdvanced(mem, client);
    else if (g_activeTab == 2) DrawDiagnostics(mem, client);
    else if (g_activeTab == 3) DrawTests(mem, client);
    else DrawLicense(mem, client);
    BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

BOOL EnsureMouseHook(BOOL on) {
    if (on) {
        if (g_mouseHook || g_hookThread) return TRUE;
        if (g_hookReady) {
            CloseHandle(g_hookReady);
            g_hookReady = 0;
        }
        InterlockedExchange(&g_hookOk, 0);
        g_hookReady = CreateEventW(0, TRUE, FALSE, 0);
        g_hookThread = CreateThread(0, 0, HookThreadProc, 0, 0, 0);
        if (!g_hookThread || WaitForSingleObject(g_hookReady, 700) != WAIT_OBJECT_0 ||
            !InterlockedCompareExchange(&g_hookOk, 0, 0)) {
            if (g_hookThread) {
                PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
                WaitForSingleObject(g_hookThread, 500);
                CloseHandle(g_hookThread);
                g_hookThread = 0;
            }
            if (g_hookReady) {
                CloseHandle(g_hookReady);
                g_hookReady = 0;
            }
            InterlockedExchange(&g_stats.safeMode, 1);
            SetWarning(L"Safe mode: low-level mouse hook unavailable");
            return FALSE;
        }
        return TRUE;
    }
    if (g_hookThread) {
        PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hookThread, 1000);
        CloseHandle(g_hookThread);
        g_hookThread = 0;
        g_hookThreadId = 0;
    } else if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = 0;
    }
    if (g_hookReady) {
        CloseHandle(g_hookReady);
        g_hookReady = 0;
    }
    InterlockedExchange(&g_hookOk, 0);
    return TRUE;
}

void ToggleEngine() {
    if (!LicenseAllowsEngine()) {
        SetWarning(L"License required");
        InterlockedExchange(&g_settings.enabled, 0);
        EnsureMouseHook(FALSE);
        return;
    }
    LONG now = InterlockedCompareExchange(&g_settings.enabled, 0, 0);
    if (!now) {
        if (!EnsureMouseHook(TRUE)) {
            InterlockedExchange(&g_settings.enabled, 0);
            return;
        }
        InterlockedExchange(&g_settings.enabled, 1);
        if (InterlockedCompareExchange(&g_settings.highPerformance, 0, 0)) {
            InterlockedExchange(&g_settings.suppressLegacyMove, 1);
            EnsureMouseHook(TRUE);
        }
        ApplyPerformanceMode();
        InterlockedExchange(&g_stats.safeMode, 0);
        InterlockedExchange(&g_physDtRingFill, 0);
        g_stats.physicalHzPeak = 0.0;
        QueueClear();
        g_stats.latencyMinUs = 0.0;
        g_stats.latencyMaxUs = 0.0;
        g_stats.latencyAvgUs = 0.0;
        g_stats.rawPeriodMinUs = 0.0;
        g_stats.rawPeriodMaxUs = 0.0;
        g_stats.rawPeriodAvgUs = 0.0;
        g_stats.engineAgeUs = 0.0;
        g_stats.outputHzMeasured = 0.0;
        SetWarning(L"TURBO: burst enjeksiyon aktif (8K hedef)");
        QueryPerformanceCounter(&g_lastPhysicalQpc);
    } else {
        InterlockedExchange(&g_settings.enabled, 0);
        ApplyPerformanceMode();
        EnsureMouseHook(FALSE);
        QueueClear();
        SetWarning(L"Engine stopped");
    }
}

void SliderSetValue(int idx, int x) {
    RECT r = g_sliderRects[idx];
    double t = (double)(x - r.left) / (double)(r.right - r.left);
    t = ClampD(t, 0.0, 1.0);
    if (idx == 0) g_settings.kalmanQ = 0.0001 + t * (0.20 - 0.0001);
    else if (idx == 1) g_settings.kalmanR = 0.001 + t * (2.00 - 0.001);
    else if (idx == 2) g_settings.splineTension = t;
    else if (idx == 3) g_settings.syncOffsetUs = -500.0 + t * 1000.0;
    else if (idx == 4) g_settings.virtualHzLimit = 1000.0 + t * 7000.0;
}

BOOL PtInRectI(RECT r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

void HandleClickDown(HWND hwnd, int x, int y) {
    if (y >= 18 && y <= 52) {
        int tab = (x - 18) / 130;
        if (tab >= 0 && tab < 5) {
            int left = 18 + tab * 130;
            if (x >= left && x < left + 124) {
                g_activeTab = tab;
                RECT c;
                GetClientRect(hwnd, &c);
                LayoutLicenseControls(c);
                InvalidateRect(hwnd, 0, FALSE);
                return;
            }
        }
    }
    if (g_activeTab == 0) {
        if (PtInRectI(g_startButton, x, y)) {
            ToggleEngine();
            ApplyPerformanceMode();
        }
        else if (PtInRectI(g_modeButton, x, y)) {
            LONG hp = InterlockedCompareExchange(&g_settings.highPerformance, 0, 0);
            InterlockedExchange(&g_settings.highPerformance, hp ? 0 : 1);
            ApplyPerformanceMode();
        } else if (PtInRectI(g_syncButton, x, y)) {
            LONG s = InterlockedCompareExchange(&g_settings.syncToDwm, 0, 0);
            InterlockedExchange(&g_settings.syncToDwm, s ? 0 : 1);
        }
    } else if (g_activeTab == 1) {
        RECT client;
        GetClientRect(hwnd, &client);
        for (int i = 0; i < 5; ++i) {
            RECT hot = g_sliderRects[i];
            hot.top -= 12; hot.bottom += 12;
            if (PtInRectI(hot, x, y)) {
                g_dragSlider = i;
                SliderSetValue(i, x);
                InvalidateRect(hwnd, 0, FALSE);
                return;
            }
        }
        RECT autoBtn = {36, 138 + 420, 36 + 180, 138 + 452};
        if (PtInRectI(autoBtn, x, y)) {
            g_settings.kalmanQ = 0.20;
            g_settings.kalmanR = 0.001;
            g_settings.splineTension = 0.1;
            g_settings.virtualHzLimit = 8000.0;
            InterlockedExchange(&g_settings.highPerformance, 1);
            if (InterlockedCompareExchange(&g_settings.enabled, 0, 0)) ApplyPerformanceMode();
            InvalidateRect(hwnd, 0, FALSE);
            return;
        }
        RECT testBtn = {36 + 192, 138 + 420, 36 + 500, 138 + 452};
        if (PtInRectI(testBtn, x, y)) {
            LONG tm = InterlockedCompareExchange(&g_settings.testMode, 0, 0);
            InterlockedExchange(&g_settings.testMode, tm ? 0 : 1);
            InvalidateRect(hwnd, 0, FALSE);
            return;
        }
        RECT supBtn = {36, 138 + 458, client.right - 36, 138 + 490};
        if (PtInRectI(supBtn, x, y)) {
            LONG sup = InterlockedCompareExchange(&g_settings.suppressLegacyMove, 0, 0);
            InterlockedExchange(&g_settings.suppressLegacyMove, sup ? 0 : 1);
            if (InterlockedCompareExchange(&g_settings.enabled, 0, 0) && !EnsureMouseHook(TRUE)) {
                InterlockedExchange(&g_settings.suppressLegacyMove, 0);
                SetWarning(L"Hook gerekli: motoru START ile acin");
            } else {
                SetWarning(InterlockedCompareExchange(&g_settings.suppressLegacyMove, 0, 0) ?
                    L"Tek hareket: WM_MOUSEMOVE engellenir (oyun icin)" :
                    L"Tek hareket kapali: masaustu icin onerilen");
            }
            InvalidateRect(hwnd, 0, FALSE);
            return;
        }
    } else if (g_activeTab == 4) {
        if (PtInRectI(g_licenseButton, x, y)) {
            ActivateLicenseFromUi();
        }
    }
    InvalidateRect(hwnd, 0, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_editBrush = CreateSolidBrush(RGB(31, 34, 39));
        g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_fontBold = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hEmailEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_license.email, WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 24, hwnd, 0, g_hInst, 0);
        g_hKeyEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_license.key, WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 24, hwnd, 0, g_hInst, 0);
        SendMessageW(g_hEmailEdit, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_hKeyEdit, WM_SETFONT, (WPARAM)g_font, TRUE);
        if (g_dwm.DwmSetWindowAttribute_p) {
            BOOL on = TRUE;
            g_dwm.DwmSetWindowAttribute_p(hwnd, 20, &on, sizeof(on));
            g_dwm.DwmSetWindowAttribute_p(hwnd, 19, &on, sizeof(on));
        }
        SetTimer(hwnd, 1, 250, 0);
        break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, RGB(236, 240, 245));
        SetBkColor(dc, RGB(31, 34, 39));
        return (LRESULT)g_editBrush;
    }
    case WM_LBUTTONDOWN:
        UpdateM1WindowLatency();
        SetCapture(hwnd);
        HandleClickDown(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (g_dragSlider >= 0 && (wp & MK_LBUTTON)) {
            SliderSetValue(g_dragSlider, GET_X_LPARAM(lp));
            InvalidateRect(hwnd, 0, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        g_dragSlider = -1;
        ReleaseCapture();
        return 0;
    case WM_SIZE: {
        RECT c;
        GetClientRect(hwnd, &c);
        LayoutLicenseControls(c);
        return 0;
    }
    case WM_TIMER:
        RefreshPointerInfo();
        UpdateCpuUsage();
        if (GetForegroundWindow() == hwnd) {
            InvalidateRect(hwnd, 0, FALSE);
        }
        return 0;
    case WM_PAINT:
        PaintUi(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        InterlockedExchange(&g_shutdown, 1);
        if (g_rawEvent) SetEvent(g_rawEvent);
        if (g_engineThread) {
            WaitForSingleObject(g_engineThread, 1000);
            CloseHandle(g_engineThread);
            g_engineThread = 0;
        }
        EnsureMouseHook(FALSE);
        if (g_rawThread) {
            PostThreadMessageW(g_rawThreadId, WM_QUIT, 0, 0);
            WaitForSingleObject(g_rawThread, 1000);
            CloseHandle(g_rawThread);
            g_rawThread = 0;
            g_rawThreadId = 0;
        } else {
            UnregisterRawMouse();
        }
        if (g_rawReady) {
            CloseHandle(g_rawReady);
            g_rawReady = 0;
        }
        if (g_font) DeleteObject(g_font);
        if (g_fontBold) DeleteObject(g_fontBold);
        if (g_editBrush) DeleteObject(g_editBrush);
        if (g_rawEvent) CloseHandle(g_rawEvent);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
