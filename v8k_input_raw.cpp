#include "v8k_common.h"
#include <math.h>

static void RecordPhysicalInterval(double dtUs) {
    if (dtUs < 20.0 || dtUs > 50000.0) return;
    LONG idx = InterlockedIncrement(&g_physDtRingFill) - 1;
    if (idx >= V8K_PHYS_DT_SAMPLES) {
        InterlockedExchange(&g_physDtRingFill, V8K_PHYS_DT_SAMPLES);
        idx = V8K_PHYS_DT_SAMPLES - 1;
    }
    g_physDtUsRing[idx % V8K_PHYS_DT_SAMPLES] = dtUs;
    double hz = 1000000.0 / dtUs;
    if (g_stats.physicalHz <= 0.0) g_stats.physicalHz = hz;
    else g_stats.physicalHz = g_stats.physicalHz * 0.35 + hz * 0.65;
    if (hz > g_stats.physicalHzPeak) g_stats.physicalHzPeak = hz;
    else if (g_stats.physicalHzPeak <= 0.0) g_stats.physicalHzPeak = hz;
    else g_stats.physicalHzPeak = g_stats.physicalHzPeak * 0.995 + hz * 0.005;
    g_stats.virtualHz = ComputeVirtualHz(g_stats.physicalHz);
}

BOOL RegisterRawMouse(HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid.hwndTarget = hwnd;
    BOOL ok = RegisterRawInputDevices(&rid, 1, sizeof(rid));
    InterlockedExchange(&g_rawRegistered, ok ? 1 : 0);
    if (!ok) SetWarning(L"Raw kayit basarisiz");
    return ok;
}

void ReregisterRawMouse() {
    if (g_rawHwnd) PostMessageW(g_rawHwnd, V8K_WM_REREG_RAW, 0, 0);
}

void UnregisterRawMouse() {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = 0;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    InterlockedExchange(&g_rawRegistered, 0);
}

BOOL QueuePush(const RawMove& mv) {
    LONG head = InterlockedCompareExchange(&g_queueHead, 0, 0);
    LONG tail = InterlockedCompareExchange(&g_queueTail, 0, 0);
    LONG next = (head + 1) & V8K_QUEUE_MASK;
    if (next == tail) {
        InterlockedExchange(&g_queueTail, (tail + 1) & V8K_QUEUE_MASK);
        InterlockedIncrement(&g_stats.droppedEvents);
    }
    g_queue[head] = mv;
    MemoryBarrier();
    InterlockedExchange(&g_queueHead, next);
    SetEvent(g_rawEvent);
    return TRUE;
}

BOOL QueuePop(RawMove* mv) {
    LONG tail = InterlockedCompareExchange(&g_queueTail, 0, 0);
    if (tail == InterlockedCompareExchange(&g_queueHead, 0, 0)) return FALSE;
    *mv = g_queue[tail];
    MemoryBarrier();
    InterlockedExchange(&g_queueTail, (tail + 1) & V8K_QUEUE_MASK);
    return TRUE;
}

BOOL QueuePopSingle(RawMove* mv) {
    return QueuePop(mv);
}

BOOL QueueFlushToLatest(RawMove* mv, int* dropped) {
    RawMove cur;
    if (!QueuePop(&cur)) return FALSE;
    int skip = 0;
    RawMove newer;
    while (QueuePop(&newer)) {
        cur = newer;
        ++skip;
    }
    *mv = cur;
    if (dropped) *dropped = skip;
    if (skip > 0) InterlockedExchangeAdd(&g_stats.droppedEvents, skip);
    return TRUE;
}

int QueueDepth() {
    LONG head = InterlockedCompareExchange(&g_queueHead, 0, 0);
    LONG tail = InterlockedCompareExchange(&g_queueTail, 0, 0);
    return (head - tail) & V8K_QUEUE_MASK;
}

void QueueClear() {
    InterlockedExchange(&g_queueTail, InterlockedCompareExchange(&g_queueHead, 0, 0));
}

BOOL QueueCoalesceLatest(RawMove* mv, int* mergedCount) {
    if (!QueuePopSingle(mv)) return FALSE;
    if (mergedCount) *mergedCount = 1;
    return TRUE;
}

BOOL ShouldSkipRaw(HANDLE device, const LARGE_INTEGER& now) {
    (void)device;
    (void)now;
    return InterlockedCompareExchange(&g_Injecting, 0, 0) != 0;
}

static BOOL ShouldSkipRawPacket(const RAWMOUSE* mouse, const LARGE_INTEGER& now) {
    if (!mouse) return TRUE;
    if (mouse->ulExtraInformation == V8K_EXTRA_INFO) return TRUE;
    if (g_lastInjectQpc.QuadPart && QpcElapsedUs(g_lastInjectQpc, now) > V8K_SKIP_RAW_WINDOW_US) {
        InterlockedExchange(&g_SkipNextRaw, 0);
    }
    LONG skip = InterlockedCompareExchange(&g_SkipNextRaw, 0, 0);
    if (skip > 0 && g_lastInjectQpc.QuadPart) {
        double age = QpcElapsedUs(g_lastInjectQpc, now);
        if (age >= 0.0 && age < V8K_SKIP_RAW_WINDOW_US) {
            InterlockedDecrement(&g_SkipNextRaw);
            return TRUE;
        }
        InterlockedExchange(&g_SkipNextRaw, 0);
    }
    return InterlockedCompareExchange(&g_Injecting, 0, 0) != 0;
}

void UpdateRawStats(LONG dx, LONG dy, const LARGE_INTEGER& now) {
    InterlockedIncrement(&g_stats.rawEvents);
    if (g_lastPhysicalQpc.QuadPart != 0) {
        double dtUs = QpcElapsedUs(g_lastPhysicalQpc, now);
        RecordPhysicalInterval(dtUs);
        if (g_stats.rawPeriodMinUs <= 0.0 || dtUs < g_stats.rawPeriodMinUs) g_stats.rawPeriodMinUs = dtUs;
        else g_stats.rawPeriodMinUs = g_stats.rawPeriodMinUs * 0.95 + dtUs * 0.05;
        if (dtUs > g_stats.rawPeriodMaxUs) g_stats.rawPeriodMaxUs = dtUs;
        else g_stats.rawPeriodMaxUs = g_stats.rawPeriodMaxUs * 0.85 + dtUs * 0.15;
        if (g_stats.rawPeriodAvgUs <= 0.0) g_stats.rawPeriodAvgUs = dtUs;
        else g_stats.rawPeriodAvgUs = g_stats.rawPeriodAvgUs * 0.55 + dtUs * 0.45;
        int mag = abs((int)dx) + abs((int)dy);
        if (mag > 0) {
            double pixels = (mag > 3) ? sqrt((double)dx * (double)dx + (double)dy * (double)dy) : (double)mag;
            double meters = pixels * g_hw.metersPerPixel;
            double speed = meters / (dtUs / 1000000.0);
            g_stats.speedMetersPerSec = g_stats.speedMetersPerSec * 0.65 + speed * 0.35;
            if ((g_stats.rawEvents & 3) == 0) {
                g_stats.graph[g_stats.graphHead] = ClampD(g_stats.speedMetersPerSec, 0.0, 5.0);
                g_stats.graphHead = (g_stats.graphHead + 1) % V8K_GRAPH_SAMPLES;
            }
        }
    }
    g_lastPhysicalQpc = now;
}

void UpdateMouseDeviceName(HANDLE device) {
    if (!device) return;
    UINT chars = 255;
    WCHAR name[256];
    name[0] = 0;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name, &chars) != (UINT)-1 && name[0]) {
        CopyW(g_hw.mouseName, 256, name);
    }
}

void UpdateMotionState(LONG dx, LONG dy, double dtUs) {
    if (dtUs < 1.0 || dtUs > 100000.0) return;
    double invSec = 1000000.0 / dtUs;
    double vx = (double)dx * invSec;
    double vy = (double)dy * invSec;
    g_motion.velX = g_motion.velX * 0.55 + vx * 0.45;
    g_motion.velY = g_motion.velY * 0.55 + vy * 0.45;
    double newSpeed = sqrt(g_motion.velX * g_motion.velX + g_motion.velY * g_motion.velY);
    g_motion.speed = g_motion.speed * 0.65 + newSpeed * 0.35;
    int mag = abs((int)dx) + abs((int)dy);
    g_motion.flick = (mag >= 28) ? 1 : 0;
}

void ProcessRawInput(LPARAM lParam) {
    UINT size = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
    if (!size || size > 512) return;
    BYTE buffer[512];
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) != size) return;
    RAWINPUT* raw = (RAWINPUT*)buffer;
    if (raw->header.dwType != RIM_TYPEMOUSE) return;
    RAWMOUSE* mouse = &raw->data.mouse;
    if (mouse->usFlags & MOUSE_MOVE_ABSOLUTE) return;
    LONG dx = mouse->lLastX;
    LONG dy = mouse->lLastY;
    if (dx == 0 && dy == 0) return;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (ShouldSkipRawPacket(mouse, now)) return;
    double dtUs = 0.0;
    if (g_lastPhysicalQpc.QuadPart) dtUs = QpcElapsedUs(g_lastPhysicalQpc, now);
    UpdateRawStats(dx, dy, now);
    UpdateMotionState(dx, dy, dtUs);
    if (raw->header.hDevice != g_lastNamedDevice) {
        g_lastNamedDevice = raw->header.hDevice;
        UpdateMouseDeviceName(raw->header.hDevice);
    }
    if (InterlockedCompareExchange(&g_stats.safeMode, 0, 0) && LicenseAllowsEngine()) {
        InterlockedExchange(&g_stats.safeMode, 0);
    }
    if (!InterlockedCompareExchange(&g_settings.enabled, 0, 0) || !LicenseAllowsEngine()) return;
    if (InterlockedCompareExchange(&g_stats.safeMode, 0, 0)) return;
    RawMove mv;
    mv.dx = dx;
    mv.dy = dy;
    mv.qpc = now;
    mv.device = raw->header.hDevice;
    QueuePush(mv);
}

static void EnableCaptureThreadBoost() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    HMODULE av = LoadLibraryW(L"avrt.dll");
    if (av) {
        typedef HANDLE (WINAPI *PFN_AvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
        PFN_AvSetMmThreadCharacteristicsW fn = (PFN_AvSetMmThreadCharacteristicsW)GetProcAddress(av, "AvSetMmThreadCharacteristicsW");
        if (fn) {
            DWORD idx = 0;
            fn(L"Pro Audio", &idx);
        }
    }
}

LRESULT CALLBACK RawWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        EnableCaptureThreadBoost();
        if (!RegisterRawMouse(hwnd)) SetWarning(L"Raw kayit basarisiz");
        if (g_rawReady) SetEvent(g_rawReady);
        return 0;
    case WM_INPUT:
        ProcessRawInput(lp);
        return 0;
    case WM_INPUT_DEVICE_CHANGE:
        DetectRawMouse();
        return 0;
    case V8K_WM_REREG_RAW:
        UnregisterRawMouse();
        RegisterRawMouse(hwnd);
        return 0;
    case WM_DESTROY:
        UnregisterRawMouse();
        g_rawHwnd = 0;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

DWORD WINAPI RawThreadProc(void*) {
    g_rawThreadId = GetCurrentThreadId();
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = RawWndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = V8K_RAW_CLASS_NAME;
    RegisterClassExW(&wc);
    g_rawHwnd = CreateWindowExW(0, V8K_RAW_CLASS_NAME, L"Virtual8KRaw", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, g_hInst, 0);
    if (!g_rawHwnd && g_rawReady) SetEvent(g_rawReady);
    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_rawHwnd) {
        DestroyWindow(g_rawHwnd);
        g_rawHwnd = 0;
    }
    return 0;
}
