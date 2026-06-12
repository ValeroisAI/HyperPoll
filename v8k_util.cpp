#include "v8k_common.h"
#include <math.h>

void CopyW(WCHAR* dst, int cap, const WCHAR* src) {
    if (!dst || cap <= 0) return;
    int i = 0;
    if (src) {
        for (; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    }
    dst[i] = 0;
}

void AppendW(WCHAR* dst, int cap, const WCHAR* src) {
    if (!dst || cap <= 0 || !src) return;
    int n = 0;
    while (n < cap - 1 && dst[n]) ++n;
    int i = 0;
    while (n < cap - 1 && src[i]) dst[n++] = src[i++];
    dst[n] = 0;
}

double ClampD(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

int ClampI(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

double EstimatePhysicalHzPeak(void) {
    LONG n = InterlockedCompareExchange(&g_physDtRingFill, 0, 0);
    if (n <= 0) return g_stats.physicalHz > 1.0 ? g_stats.physicalHz : 1000.0;
    if (n > V8K_PHYS_DT_SAMPLES) n = V8K_PHYS_DT_SAMPLES;
    double minDt = 0.0;
    for (LONG i = 0; i < n; ++i) {
        double dt = g_physDtUsRing[i];
        if (dt < 20.0 || dt > 50000.0) continue;
        if (minDt <= 0.0 || dt < minDt) minDt = dt;
    }
    if (minDt <= 0.0) return g_stats.physicalHz > 1.0 ? g_stats.physicalHz : 1000.0;
    double peak = 1000000.0 / minDt;
    if (peak < 125.0) peak = 125.0;
    if (peak > 24000.0) peak = 24000.0;
    return peak;
}

int ComputeMicroSteps(double physicalHz) {
    double peak = EstimatePhysicalHzPeak();
    if (peak > physicalHz) physicalHz = peak;
    if (physicalHz < 125.0) physicalHz = 1000.0;
    if (physicalHz > 24000.0) physicalHz = 24000.0;
    double target = ClampD(g_settings.virtualHzLimit, 1000.0, 8000.0);
    double ratio = target / physicalHz;
    int steps = ClampI((int)ceil(ratio), 1, V8K_MAX_MICRO_STEPS);
    if (target >= 7500.0 && steps < V8K_MAX_MICRO_STEPS && physicalHz < 3200.0) {
        steps = V8K_MAX_MICRO_STEPS;
    }
    return steps;
}

double ComputeVirtualHz(double physicalHz) {
    double peak = EstimatePhysicalHzPeak();
    if (peak > physicalHz) physicalHz = peak;
    if (physicalHz < 125.0) physicalHz = 1000.0;
    double out = physicalHz * (double)ComputeMicroSteps(physicalHz);
    return out > V8K_MAX_OUTPUT_HZ ? V8K_MAX_OUTPUT_HZ : out;
}

int RoundI(double v) {
    return (int)(v >= 0.0 ? v + 0.5 : v - 0.5);
}

ULONGLONG FileTimeToU64(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

ULONGLONG FtNow() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return FileTimeToU64(ft);
}

double QpcElapsedUs(const LARGE_INTEGER& a, const LARGE_INTEGER& b) {
    return (double)(b.QuadPart - a.QuadPart) * 1000000.0 / (double)g_qpcFreq.QuadPart;
}

LONGLONG UsToQpc(double us) {
    return (LONGLONG)(us * (double)g_qpcFreq.QuadPart / 1000000.0);
}

void FormatDouble(WCHAR* out, int cap, const WCHAR* label, double v, const WCHAR* suffix, int decimals) {
    WCHAR fmt[32];
    wsprintfW(fmt, L"%%s%%.%df%%s", decimals);
    swprintf(out, cap, fmt, label, v, suffix ? suffix : L"");
}

void SetWarning(const WCHAR* text) {
    CopyW(g_stats.warning, V8K_MAX_TEXT, text);
}
