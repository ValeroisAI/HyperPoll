#include "v8k_common.h"
#include <math.h>

static int BuildUniformMicroSteps(LONG dx, LONG dy, int steps, int* sdx, int* sdy) {
    if (steps < 1) steps = 1;
    int n = 0;
    int prevIx = 0, prevIy = 0;
    for (int i = 1; i <= steps; ++i) {
        int ix = (i == steps) ? (int)dx : RoundI((double)dx * (double)i / (double)steps);
        int iy = (i == steps) ? (int)dy : RoundI((double)dy * (double)i / (double)steps);
        int stepDx = ix - prevIx;
        int stepDy = iy - prevIy;
        if (stepDx != 0 || stepDy != 0) {
            sdx[n] = stepDx;
            sdy[n] = stepDy;
            ++n;
        }
        prevIx = ix;
        prevIy = iy;
    }
    return n;
}

static void UpdateLatency(double latencyUs) {
    latencyUs = ClampD(latencyUs, 0.0, 50000.0);
    if (g_stats.latencyMinUs <= 0.0 || latencyUs < g_stats.latencyMinUs) g_stats.latencyMinUs = latencyUs;
    else g_stats.latencyMinUs = g_stats.latencyMinUs * 0.992 + latencyUs * 0.008;
    if (latencyUs > g_stats.latencyMaxUs) g_stats.latencyMaxUs = latencyUs;
    else g_stats.latencyMaxUs = g_stats.latencyMaxUs * 0.94 + latencyUs * 0.06;
    if (g_stats.latencyAvgUs <= 0.0) g_stats.latencyAvgUs = latencyUs;
    else g_stats.latencyAvgUs = g_stats.latencyAvgUs * 0.55 + latencyUs * 0.45;
}

static UINT InjectTurboBurst(const int* sdx, const int* sdy, int count, LARGE_INTEGER* firstEnd) {
    if (count <= 0) return 0;
    UINT sentTotal = 0;
    InterlockedExchange(&g_Injecting, 1);
    QueryPerformanceCounter(&g_lastInjectQpc);
    InterlockedExchangeAdd(&g_SkipNextRaw, (LONG)count);
    for (int i = 0; i < count; ++i) {
        INPUT input;
        ZeroMemory(&input, sizeof(input));
        input.type = INPUT_MOUSE;
        input.mi.dx = sdx[i];
        input.mi.dy = sdy[i];
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
        input.mi.dwExtraInfo = V8K_EXTRA_INFO;
        UINT sent = SendInput(1, &input, sizeof(INPUT));
        if (sent == 1) {
            ++sentTotal;
            InterlockedIncrement(&g_stats.injectedEvents);
            if (firstEnd && !firstEnd->QuadPart) QueryPerformanceCounter(firstEnd);
        }
    }
    InterlockedExchange(&g_Injecting, 0);
    return sentTotal;
}

static UINT InjectBatch(const int* sdx, const int* sdy, int count, BOOL noCoalesce) {
    if (count <= 0) return 0;
    if (count > V8K_MAX_MICRO_STEPS) count = V8K_MAX_MICRO_STEPS;
    INPUT inputs[V8K_MAX_MICRO_STEPS];
    int n = 0;
    for (int i = 0; i < count; ++i) {
        if (sdx[i] == 0 && sdy[i] == 0) continue;
        ZeroMemory(&inputs[n], sizeof(INPUT));
        inputs[n].type = INPUT_MOUSE;
        inputs[n].mi.dx = sdx[i];
        inputs[n].mi.dy = sdy[i];
        inputs[n].mi.dwFlags = MOUSEEVENTF_MOVE;
        if (noCoalesce) inputs[n].mi.dwFlags |= MOUSEEVENTF_MOVE_NOCOALESCE;
        inputs[n].mi.dwExtraInfo = V8K_EXTRA_INFO;
        ++n;
    }
    if (n <= 0) return 0;
    QueryPerformanceCounter(&g_lastInjectQpc);
    InterlockedExchangeAdd(&g_SkipNextRaw, (LONG)n);
    InterlockedExchange(&g_Injecting, 1);
    UINT sent = SendInput((UINT)n, inputs, sizeof(INPUT));
    InterlockedExchange(&g_Injecting, 0);
    if (sent == (UINT)n) {
        InterlockedExchangeAdd(&g_stats.injectedEvents, n);
        return n;
    }
    return 0;
}

BOOL InjectRelativeMove(int dx, int dy) {
    int sdx[1] = {dx};
    int sdy[1] = {dy};
    LARGE_INTEGER t;
    t.QuadPart = 0;
    return InjectTurboBurst(sdx, sdy, (dx || dy) ? 1 : 0, &t) > 0;
}

BOOL InjectRelativeBatch(const int* dx, const int* dy, int count) {
    BOOL nc = InterlockedCompareExchange(&g_settings.highPerformance, 0, 0) != 0;
    return InjectBatch(dx, dy, count, nc) > 0;
}

void UpdateM1HookLatency(const MSLLHOOKSTRUCT* ms) {
    if (!ms) return;
    DWORD nowMs = GetTickCount();
    DWORD ageMs = nowMs - ms->time;
    double us = (double)ageMs * 1000.0;
    if (us < 0.0 || us > 200000.0) us = 0.0;
    if (g_stats.m1HookLatencyUs <= 0.0) g_stats.m1HookLatencyUs = us;
    else g_stats.m1HookLatencyUs = g_stats.m1HookLatencyUs * 0.70 + us * 0.30;
    QueryPerformanceCounter(&g_lastM1HookQpc);
    InterlockedIncrement(&g_stats.m1Events);
}

void UpdateM1WindowLatency() {
    if (!g_lastM1HookQpc.QuadPart) return;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double us = QpcElapsedUs(g_lastM1HookQpc, now);
    if (us < 0.0 || us > 200000.0) return;
    if (g_stats.m1WindowLatencyUs <= 0.0) g_stats.m1WindowLatencyUs = us;
    else g_stats.m1WindowLatencyUs = g_stats.m1WindowLatencyUs * 0.70 + us * 0.30;
}

static void ProcessTurboMove(const RawMove& mv) {
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    double inputAgeUs = QpcElapsedUs(mv.qpc, t0);
    g_stats.engineAgeUs = g_stats.engineAgeUs <= 0.0 ? inputAgeUs : g_stats.engineAgeUs * 0.65 + inputAgeUs * 0.35;

    double peakHz = EstimatePhysicalHzPeak();
    int steps = ComputeMicroSteps(peakHz);
    g_stats.virtualHz = (double)steps * peakHz;
    if (g_stats.virtualHz > V8K_MAX_OUTPUT_HZ) g_stats.virtualHz = V8K_MAX_OUTPUT_HZ;

    int sdx[V8K_MAX_MICRO_STEPS];
    int sdy[V8K_MAX_MICRO_STEPS];
    int n = BuildUniformMicroSteps(mv.dx, mv.dy, steps, sdx, sdy);
    if (n <= 0) return;

    LARGE_INTEGER firstEnd;
    firstEnd.QuadPart = 0;
    if (steps > 1) {
        InjectTurboBurst(sdx, sdy, n, &firstEnd);
    } else {
        InjectTurboBurst(sdx, sdy, n, &firstEnd);
    }
    if (!firstEnd.QuadPart) QueryPerformanceCounter(&firstEnd);
    UpdateLatency(QpcElapsedUs(mv.qpc, firstEnd));
}

static double CatmullHermite(double p0, double p1, double p2, double p3, double t, double tension) {
    double s = ClampD(1.0 - tension, 0.0, 1.0);
    double m1 = (p2 - p0) * 0.5 * s;
    double m2 = (p3 - p1) * 0.5 * s;
    double t2 = t * t;
    double t3 = t2 * t;
    return (2.0 * t3 - 3.0 * t2 + 1.0) * p1 + (t3 - 2.0 * t2 + t) * m1 + (-2.0 * t3 + 3.0 * t2) * p2 + (t3 - t2) * m2;
}

static double KalmanUpdate(Kalman1D* k, double z, double q, double r) {
    if (!k->initialized) { k->x = z; k->p = 1.0; k->initialized = 1; return z; }
    k->p += q;
    double gain = k->p / (k->p + r);
    k->x += gain * (z - k->x);
    k->p *= (1.0 - gain);
    return k->x;
}

static void ProcessSmoothMove(const RawMove& mv) {
    static int seeded = 0;
    static double p0x, p0y, p1x, p1y;
    static Kalman1D kx = {0.0, 1.0, 0};
    static Kalman1D ky = {0.0, 1.0, 0};
    if (!seeded) { p0x = p1x = p0y = p1y = 0.0; kx.initialized = ky.initialized = 0; seeded = 1; }

    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    g_stats.engineAgeUs = QpcElapsedUs(mv.qpc, t0);

    int steps = ComputeMicroSteps(EstimatePhysicalHzPeak());
    if (steps <= 1) {
        InjectRelativeMove(mv.dx, mv.dy);
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        UpdateLatency(QpcElapsedUs(mv.qpc, end));
        return;
    }

    double p2x = p1x + (double)mv.dx;
    double p2y = p1y + (double)mv.dy;
    double q = ClampD(g_settings.kalmanQ, 0.00001, 1.0);
    double r = ClampD(g_settings.kalmanR, 0.00001, 10.0);
    double p3x = p2x + (p2x - p0x) * 0.5;
    double p3y = p2y + (p2y - p0y) * 0.5;
    int sdx[V8K_MAX_MICRO_STEPS];
    int sdy[V8K_MAX_MICRO_STEPS];
    int prevIx = 0, prevIy = 0, n = 0;
    double tension = ClampD(g_settings.splineTension, 0.0, 1.0);
    for (int i = 1; i <= steps; ++i) {
        double t = (double)i / (double)steps;
        double sx = CatmullHermite(p0x, p1x, p2x, p3x, t, tension);
        double sy = CatmullHermite(p0y, p1y, p2y, p3y, t, tension);
        sx = KalmanUpdate(&kx, sx, q, r);
        sy = KalmanUpdate(&ky, sy, q, r);
        int ix = (i == steps) ? (int)mv.dx : RoundI(sx - p1x);
        int iy = (i == steps) ? (int)mv.dy : RoundI(sy - p1y);
        int stepDx = ix - prevIx;
        int stepDy = iy - prevIy;
        if (stepDx || stepDy) { sdx[n] = stepDx; sdy[n] = stepDy; ++n; }
        prevIx = ix; prevIy = iy;
    }
    InjectBatch(sdx, sdy, n, FALSE);
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    UpdateLatency(QpcElapsedUs(mv.qpc, end));
    p0x = p1x; p0y = p1y; p1x = p2x; p1y = p2y;
}

void ProcessInterpolatedMove(const RawMove& mv) {
    if (!LicenseAllowsEngine()) return;
    if (InterlockedCompareExchange(&g_settings.highPerformance, 0, 0)) {
        ProcessTurboMove(mv);
    } else {
        ProcessSmoothMove(mv);
    }
}

static void EnableEngineThreadBoost() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    HMODULE av = LoadLibraryW(L"avrt.dll");
    if (av) {
        typedef HANDLE (WINAPI *PFN_AvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
        PFN_AvSetMmThreadCharacteristicsW fn = (PFN_AvSetMmThreadCharacteristicsW)GetProcAddress(av, "AvSetMmThreadCharacteristicsW");
        if (fn) { DWORD idx = 0; fn(L"Games", &idx); }
    }
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << 1);
}

DWORD WINAPI EngineThreadProc(void*) {
    QueryPerformanceCounter(&g_engineHeartbeatQpc);
    EnableEngineThreadBoost();
    for (;;) {
        if (InterlockedCompareExchange(&g_shutdown, 0, 0)) break;

        if (!InterlockedCompareExchange(&g_settings.enabled, 0, 0) || !LicenseAllowsEngine()) {
            WaitForSingleObject(g_rawEvent, 16);
            continue;
        }

        BOOL turbo = InterlockedCompareExchange(&g_settings.highPerformance, 0, 0) != 0;
        RawMove mv;
        int dropped = 0;
        BOOL got = FALSE;

        if (turbo && QueueDepth() > V8K_TURBO_BACKLOG_FLUSH) {
            got = QueueFlushToLatest(&mv, &dropped);
            if (dropped > 0) InterlockedExchangeAdd(&g_stats.coalescedEvents, dropped);
        } else if (turbo) {
            got = QueuePopSingle(&mv);
        } else {
            int merged = 0;
            got = QueueCoalesceLatest(&mv, &merged);
            if (merged > 1) InterlockedExchangeAdd(&g_stats.coalescedEvents, merged - 1);
        }

        if (!got) {
            if (turbo) {
                if (WaitForSingleObject(g_rawEvent, 0) == WAIT_OBJECT_0) continue;
                YieldProcessor();
            } else {
                WaitForSingleObject(g_rawEvent, 3);
            }
            continue;
        }

        if (!InterlockedCompareExchange(&g_stats.safeMode, 0, 0)) {
            ProcessInterpolatedMove(mv);
        }
        QueryPerformanceCounter(&g_engineHeartbeatQpc);

        if (turbo) {
            while (QueueDepth() > 0 && !InterlockedCompareExchange(&g_stats.safeMode, 0, 0)) {
                if (!QueuePopSingle(&mv)) break;
                ProcessInterpolatedMove(mv);
            }
        }
    }
    return 0;
}
