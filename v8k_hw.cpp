#include "v8k_common.h"

int GcdI(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a ? a : 1;
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR mon, HDC, LPRECT, LPARAM) {
    MONITORINFOEXW mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return TRUE;
    if (!(mi.dwFlags & MONITORINFOF_PRIMARY)) return TRUE;
    CopyW(g_hw.monitorDevice, 64, mi.szDevice);
    CopyW(g_hw.monitorName, 64, mi.szDevice);
    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsExW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm, 0)) {
        g_hw.width = (int)dm.dmPelsWidth;
        g_hw.height = (int)dm.dmPelsHeight;
        g_hw.bpp = (int)dm.dmBitsPerPel;
        g_hw.refreshHz = dm.dmDisplayFrequency ? (double)dm.dmDisplayFrequency : 60.0;
    }
    HDC dc = CreateDCW(mi.szDevice, 0, 0, 0);
    if (dc) {
        g_hw.physicalWidthMm = GetDeviceCaps(dc, HORZSIZE);
        g_hw.physicalHeightMm = GetDeviceCaps(dc, VERTSIZE);
        DeleteDC(dc);
    }
    return FALSE;
}

static void DetectDisplayConfigClock() {
    return;
}

static void DetectDxgiAndD3d() {
    static const GUID IID_IDXGIFactory1_V8K = {0x770aae78,0xf26f,0x4dba,{0xa8,0x29,0x25,0x3c,0x83,0xd1,0xb3,0x87}};
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    PFN_CreateDXGIFactory1 CreateDXGIFactory1_p = dxgi ? (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1") : 0;
    if (!CreateDXGIFactory1_p) {
        CopyW(g_hw.gpuName, 160, L"DXGI runtime unavailable");
        return;
    }
    IDXGIFactory1* factory = 0;
    HRESULT hr = CreateDXGIFactory1_p(IID_IDXGIFactory1_V8K, (void**)&factory);
    if (FAILED(hr) || !factory) {
        CopyW(g_hw.gpuName, 160, L"DXGI unavailable");
        return;
    }
    IDXGIAdapter1* adapter = 0;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        ZeroMemory(&desc, sizeof(desc));
        adapter->GetDesc1(&desc);
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            CopyW(g_hw.gpuName, 160, desc.Description);
            if (desc.VendorId == 0x10DE) CopyW(g_hw.gpuVendor, 40, L"NVIDIA");
            else if (desc.VendorId == 0x1002 || desc.VendorId == 0x1022) CopyW(g_hw.gpuVendor, 40, L"AMD");
            else if (desc.VendorId == 0x8086) CopyW(g_hw.gpuVendor, 40, L"Intel");
            else CopyW(g_hw.gpuVendor, 40, L"DXGI");
            IDXGIOutput* output = 0;
            for (UINT o = 0; adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
                DXGI_OUTPUT_DESC od;
                ZeroMemory(&od, sizeof(od));
                output->GetDesc(&od);
                if (lstrcmpiW(od.DeviceName, g_hw.monitorDevice) == 0) {
                    UINT count = 0;
                    output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &count, 0);
                    if (count > 0 && count <= 512) {
                        DXGI_MODE_DESC modes[512];
                        UINT cap = count;
                        if (SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &cap, modes))) {
                            double best = 0.0;
                            for (UINT k = 0; k < cap; ++k) {
                                if ((int)modes[k].Width == g_hw.width && (int)modes[k].Height == g_hw.height &&
                                    modes[k].RefreshRate.Denominator) {
                                    double r = (double)modes[k].RefreshRate.Numerator / (double)modes[k].RefreshRate.Denominator;
                                    if (fabs(r - g_hw.refreshHz) < fabs(best - g_hw.refreshHz) || best == 0.0) best = r;
                                }
                            }
                            if (best > 1.0) g_hw.refreshHz = best;
                        }
                    }
                }
                output->Release();
            }
            adapter->Release();
            break;
        }
        adapter->Release();
        adapter = 0;
    }
    factory->Release();

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    ID3D11Device* dev = 0;
    ID3D11DeviceContext* ctx = 0;
    D3D_FEATURE_LEVEL fl;
    HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
    PFN_D3D11CreateDevice D3D11CreateDevice_p = d3d11 ? (PFN_D3D11CreateDevice)GetProcAddress(d3d11, "D3D11CreateDevice") : 0;
    hr = D3D11CreateDevice_p ? D3D11CreateDevice_p(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, levels, 3, D3D11_SDK_VERSION, &dev, &fl, &ctx) : E_FAIL;
    g_hw.d3dReady = SUCCEEDED(hr);
    if (ctx) ctx->Release();
    if (dev) dev->Release();
}

void DetectRawMouse() {
    UINT count = 0;
    GetRawInputDeviceList(0, &count, sizeof(RAWINPUTDEVICELIST));
    if (!count || count > 64) return;
    RAWINPUTDEVICELIST list[64];
    UINT cap = count;
    if (GetRawInputDeviceList(list, &cap, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) return;
    int mice = 0;
    for (UINT i = 0; i < cap; ++i) {
        if (list[i].dwType != RIM_TYPEMOUSE) continue;
        ++mice;
        if (g_hw.mouseName[0] == 0) {
            UINT chars = 255;
            GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, g_hw.mouseName, &chars);
        }
    }
    g_hw.mouseCount = mice;
    if (!g_hw.mouseName[0]) CopyW(g_hw.mouseName, 256, L"No raw mouse enumerated");
}

void RefreshPointerInfo() {
    int params[3] = {0, 0, 0};
    int speed = 10;
    SystemParametersInfoW(SPI_GETMOUSE, 0, params, 0);
    SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &speed, 0);
    g_pointer.speed = speed;
    g_pointer.threshold1 = params[0];
    g_pointer.threshold2 = params[1];
    g_pointer.acceleration = params[2];
}

void DetectHardware() {
    ZeroMemory(&g_hw, sizeof(g_hw));
    g_hw.width = GetSystemMetrics(SM_CXSCREEN);
    g_hw.height = GetSystemMetrics(SM_CYSCREEN);
    g_hw.bpp = 32;
    g_hw.refreshHz = 60.0;
    CopyW(g_hw.monitorName, 64, L"Primary monitor");
    CopyW(g_hw.gpuName, 160, L"Unknown GPU");
    CopyW(g_hw.gpuVendor, 40, L"Unknown");
    EnumDisplayMonitors(0, 0, MonitorEnumProc, 0);
    DetectDisplayConfigClock();
    DetectDxgiAndD3d();
    DetectRawMouse();
    RefreshPointerInfo();
    if (g_hw.pixelClockMHz <= 0.0) {
        g_hw.pixelClockMHz = (double)g_hw.width * (double)g_hw.height * g_hw.refreshHz / 1000000.0;
    }
    if (g_hw.physicalWidthMm > 0 && g_hw.width > 0) {
        g_hw.metersPerPixel = ((double)g_hw.physicalWidthMm / 1000.0) / (double)g_hw.width;
    } else {
        g_hw.metersPerPixel = 0.000264583333;
    }
    if (g_dwm.DwmIsCompositionEnabled_p) {
        BOOL enabled = FALSE;
        if (SUCCEEDED(g_dwm.DwmIsCompositionEnabled_p(&enabled))) g_hw.dwmComposition = enabled;
    }
    swprintf(g_hw.syncStatus, 160, L"DWM %s, D3D11 %s", g_hw.dwmComposition ? L"composition active" : L"composition unavailable", g_hw.d3dReady ? L"ready" : L"fallback");
    CopyW(g_hw.mpoStatus, 96, L"Hardware composition: DXGI/DWM guarded");
}

void ApplyPerformanceMode() {
    BOOL engineOn = InterlockedCompareExchange(&g_settings.enabled, 0, 0) != 0;
    BOOL hp = InterlockedCompareExchange(&g_settings.highPerformance, 0, 0) != 0;

    if (!engineOn) {
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        if (g_engineThread) SetThreadPriority(g_engineThread, THREAD_PRIORITY_NORMAL);
        if (g_rawThread) SetThreadPriority(g_rawThread, THREAD_PRIORITY_NORMAL);
        if (g_nt.NtSetTimerResolution_p) {
            ULONG actual = 0;
            g_nt.NtSetTimerResolution_p(5000, FALSE, &actual);
        }
        return;
    }

    if (hp) {
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        if (g_engineThread) SetThreadPriority(g_engineThread, THREAD_PRIORITY_TIME_CRITICAL);
        if (g_rawThread) SetThreadPriority(g_rawThread, THREAD_PRIORITY_HIGHEST);
        if (g_nt.NtSetTimerResolution_p) {
            ULONG actual = 0;
            g_nt.NtSetTimerResolution_p(5000, TRUE, &actual);
        }
    } else {
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        if (g_engineThread) SetThreadPriority(g_engineThread, THREAD_PRIORITY_ABOVE_NORMAL);
        if (g_rawThread) SetThreadPriority(g_rawThread, THREAD_PRIORITY_NORMAL);
        if (g_nt.NtSetTimerResolution_p) {
            ULONG actual = 0;
            g_nt.NtSetTimerResolution_p(5000, FALSE, &actual);
        }
    }
}
