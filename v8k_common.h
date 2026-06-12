#ifndef V8K_COMMON_H
#define V8K_COMMON_H

#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include <math.h>

#ifdef __MINGW32__
#define swprintf _snwprintf
#endif

#ifndef LSTATUS
typedef LONG LSTATUS;
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE __stdcall
#endif
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#ifndef FW_SEMIBOLD
#define FW_SEMIBOLD 600
#endif
#ifndef RIDEV_DEVNOTIFY
#define RIDEV_DEVNOTIFY 0x00002000
#endif
#ifndef RIDEV_NOLEGACY
#define RIDEV_NOLEGACY 0x00000030
#endif
#ifndef LLMHF_INJECTED
#define LLMHF_INJECTED 0x00000001
#endif
#ifndef WM_INPUT_DEVICE_CHANGE
#define WM_INPUT_DEVICE_CHANGE 0x00FE
#endif
#ifndef YieldProcessor
#if defined(__MINGW32__) || defined(__GNUC__)
static __inline__ void _v8k_yield(void) { __asm__ __volatile__("pause":::"memory"); }
#define YieldProcessor() _v8k_yield()
#elif defined(_MSC_VER)
#include <intrin.h>
#define YieldProcessor() _mm_pause()
#else
#define YieldProcessor() Sleep(0)
#endif
#endif

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(const GUID&, void**) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
    virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
};

typedef enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN = 0,
    DXGI_FORMAT_R8G8B8A8_UNORM = 28
} DXGI_FORMAT;

typedef struct DXGI_RATIONAL {
    UINT Numerator;
    UINT Denominator;
} DXGI_RATIONAL;

typedef enum DXGI_MODE_SCANLINE_ORDER {
    DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED = 0
} DXGI_MODE_SCANLINE_ORDER;

typedef enum DXGI_MODE_SCALING {
    DXGI_MODE_SCALING_UNSPECIFIED = 0
} DXGI_MODE_SCALING;

typedef enum DXGI_MODE_ROTATION {
    DXGI_MODE_ROTATION_UNSPECIFIED = 0
} DXGI_MODE_ROTATION;

typedef struct DXGI_MODE_DESC {
    UINT Width;
    UINT Height;
    DXGI_RATIONAL RefreshRate;
    DXGI_FORMAT Format;
    DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
    DXGI_MODE_SCALING Scaling;
} DXGI_MODE_DESC;

typedef struct DXGI_OUTPUT_DESC {
    WCHAR DeviceName[32];
    RECT DesktopCoordinates;
    BOOL AttachedToDesktop;
    DXGI_MODE_ROTATION Rotation;
    HMONITOR Monitor;
} DXGI_OUTPUT_DESC;

typedef struct DXGI_ADAPTER_DESC {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
} DXGI_ADAPTER_DESC;

typedef struct DXGI_ADAPTER_DESC1 {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
    UINT Flags;
} DXGI_ADAPTER_DESC1;

typedef struct DXGI_GAMMA_CONTROL_CAPABILITIES { BOOL ScaleAndOffsetSupported; float MaxConvertedValue; float MinConvertedValue; UINT NumGammaControlPoints; float ControlPointPositions[1025]; } DXGI_GAMMA_CONTROL_CAPABILITIES;
typedef struct DXGI_GAMMA_CONTROL { struct { float Red; float Green; float Blue; } Scale; struct { float Red; float Green; float Blue; } Offset; struct { float Red; float Green; float Blue; } GammaCurve[1025]; } DXGI_GAMMA_CONTROL;
typedef struct DXGI_FRAME_STATISTICS { UINT PresentCount; UINT PresentRefreshCount; UINT SyncRefreshCount; LARGE_INTEGER SyncQPCTime; LARGE_INTEGER SyncGPUTime; } DXGI_FRAME_STATISTICS;

#ifndef DXGI_ERROR_NOT_FOUND
#define DXGI_ERROR_NOT_FOUND ((HRESULT)0x887A0002L)
#endif
#ifndef DXGI_ADAPTER_FLAG_SOFTWARE
#define DXGI_ADAPTER_FLAG_SOFTWARE 2
#endif

struct IDXGISurface;

struct IDXGIObject : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(const GUID&, UINT, const void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(const GUID&, const IUnknown*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(const GUID&, UINT*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetParent(const GUID&, void**) = 0;
};

struct IDXGIOutput : public IDXGIObject {
    virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT, UINT, UINT*, DXGI_MODE_DESC*) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC*, DXGI_MODE_DESC*, IUnknown*) = 0;
    virtual HRESULT STDMETHODCALLTYPE WaitForVBlank(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE TakeOwnership(IUnknown*, BOOL) = 0;
    virtual void STDMETHODCALLTYPE ReleaseOwnership(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetGammaControl(const DXGI_GAMMA_CONTROL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGammaControl(DXGI_GAMMA_CONTROL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDisplaySurface(IDXGISurface*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(IDXGISurface*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS*) = 0;
};

struct IDXGIAdapter : public IDXGIObject {
    virtual HRESULT STDMETHODCALLTYPE EnumOutputs(UINT, IDXGIOutput**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(const GUID&, LARGE_INTEGER*) = 0;
};

struct IDXGIAdapter1 : public IDXGIAdapter {
    virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1*) = 0;
};

struct IDXGISwapChain;

struct IDXGIFactory : public IDXGIObject {
    virtual HRESULT STDMETHODCALLTYPE EnumAdapters(UINT, IDXGIAdapter**) = 0;
    virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND, UINT) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown*, void*, IDXGISwapChain**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE, IDXGIAdapter**) = 0;
};

struct IDXGIFactory1 : public IDXGIFactory {
    virtual HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT, IDXGIAdapter1**) = 0;
    virtual BOOL STDMETHODCALLTYPE IsCurrent(void) = 0;
};

typedef enum D3D_DRIVER_TYPE {
    D3D_DRIVER_TYPE_UNKNOWN = 0,
    D3D_DRIVER_TYPE_HARDWARE = 1
} D3D_DRIVER_TYPE;

typedef enum D3D_FEATURE_LEVEL {
    D3D_FEATURE_LEVEL_10_0 = 0xa000,
    D3D_FEATURE_LEVEL_10_1 = 0xa100,
    D3D_FEATURE_LEVEL_11_0 = 0xb000
} D3D_FEATURE_LEVEL;

#ifndef D3D11_SDK_VERSION
#define D3D11_SDK_VERSION 7
#endif

struct ID3D11Device : public IUnknown {};
struct ID3D11DeviceContext : public IUnknown {};

typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(const GUID& riid, void** ppFactory);
typedef HRESULT (WINAPI *PFN_D3D11CreateDevice)(IUnknown* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

#ifndef MOUSEEVENTF_MOVE_NOCOALESCE
#define MOUSEEVENTF_MOVE_NOCOALESCE 0x2000
#endif
#ifndef MOUSEEVENTF_VIRTUALDESK
#define MOUSEEVENTF_VIRTUALDESK 0x4000
#endif
#ifndef QDC_ONLY_ACTIVE_PATHS
#define QDC_ONLY_ACTIVE_PATHS 0x00000002
#endif

#define V8K_APP_NAME              L"Virtual 8K"
#define V8K_CLASS_NAME            L"Virtual8KProWindow"
#define V8K_RAW_CLASS_NAME        L"Virtual8KRawInputWindow"
#define V8K_REG_PATH              L"Software\\Virtual8K"
#define V8K_TRIAL_VALUE           L"TrialDpapi"
#define V8K_LICENSE_VALUE         L"LicenseDpapi"
#define V8K_EXTRA_INFO            ((ULONG_PTR)0x56384B50524F2026ULL)
#define V8K_QUEUE_SIZE            4096
#define V8K_QUEUE_MASK            (V8K_QUEUE_SIZE - 1)
#define V8K_MAX_MICRO_STEPS       8
#define V8K_MAX_OUTPUT_HZ         8000.0
#define V8K_STEP_US               (1000000.0 / V8K_MAX_OUTPUT_HZ)
#define V8K_IDLE_SAFE_US          1000000.0
#define V8K_THREAD_STALE_US       500000.0
#define V8K_STALE_INPUT_US        1200.0
#define V8K_BACKLOG_CATCHUP_DEPTH 8
#define V8K_TURBO_BACKLOG_FLUSH   16
#define V8K_PHYS_DT_SAMPLES       32
#define V8K_SKIP_RAW_WINDOW_US    8000.0
#define V8K_WM_REREG_RAW         (WM_APP + 42)
#define V8K_TRIAL_DAYS            30
#define V8K_GRAPH_SAMPLES         160
#define V8K_MAX_TEXT              256
#define V8K_DPAPI_ENTROPY_BYTES   14
#define V8K_LICENSE_SECRET_BYTES  32

struct RawMove {
    LONG dx;
    LONG dy;
    LARGE_INTEGER qpc;
    HANDLE device;
};

struct Kalman1D {
    double x;
    double p;
    int initialized;
};

struct MotionState {
    double velX;
    double velY;
    double speed;
    double accel;
    int flick;
};

struct Settings {
    volatile LONG enabled;
    volatile LONG highPerformance;
    volatile LONG syncToDwm;
    volatile LONG testMode;
    volatile LONG aggressiveRaw;
    volatile LONG suppressLegacyMove;
    double kalmanQ;
    double kalmanR;
    double splineTension;
    double syncOffsetUs;
    double virtualHzLimit;
};

struct Stats {
    double physicalHz;
    double physicalHzPeak;
    double virtualHz;
    double latencyMinUs;
    double latencyMaxUs;
    double latencyAvgUs;
    double cpuPercent;
    double gpuPercent;
    double speedMetersPerSec;
    double outputHzMeasured;
    double rawPeriodMinUs;
    double rawPeriodAvgUs;
    double rawPeriodMaxUs;
    double engineAgeUs;
    double m1HookLatencyUs;
    double m1WindowLatencyUs;
    double graph[V8K_GRAPH_SAMPLES];
    int graphHead;
    volatile LONG rawEvents;
    volatile LONG injectedEvents;
    volatile LONG coalescedEvents;
    volatile LONG droppedEvents;
    volatile LONG m1Events;
    volatile LONG safeMode;
    WCHAR warning[V8K_MAX_TEXT];
};

struct HardwareInfo {
    WCHAR monitorName[64];
    WCHAR monitorDevice[64];
    WCHAR gpuName[160];
    WCHAR gpuVendor[40];
    WCHAR mouseName[256];
    WCHAR syncStatus[160];
    WCHAR mpoStatus[96];
    int width;
    int height;
    int bpp;
    int physicalWidthMm;
    int physicalHeightMm;
    int mouseCount;
    double refreshHz;
    double pixelClockMHz;
    double metersPerPixel;
    BOOL d3dReady;
    BOOL dwmComposition;
};

struct PointerInfo {
    int speed;
    int threshold1;
    int threshold2;
    int acceleration;
};

struct TrialRecord {
    DWORD magic;
    DWORD version;
    FILETIME firstRunUtc;
    FILETIME lastRunUtc;
    DWORD nonce;
};

struct LicenseRecord {
    DWORD magic;
    DWORD version;
    WCHAR email[128];
    WCHAR key[128];
};

struct LicenseState {
    BOOL pro;
    BOOL trialOk;
    BOOL tampered;
    int daysLeft;
    WCHAR email[128];
    WCHAR key[128];
    WCHAR status[160];
};

struct RegApi {
    HMODULE module;
    LSTATUS (WINAPI *RegCreateKeyExW_p)(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, const SECURITY_ATTRIBUTES*, PHKEY, LPDWORD);
    LSTATUS (WINAPI *RegSetValueExW_p)(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
    LSTATUS (WINAPI *RegQueryValueExW_p)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    LSTATUS (WINAPI *RegCloseKey_p)(HKEY);
};

struct Blob {
    DWORD cbData;
    BYTE* pbData;
};

struct CryptoApi {
    HMODULE module;
    BOOL (WINAPI *CryptProtectData_p)(Blob*, LPCWSTR, Blob*, PVOID, PVOID, DWORD, Blob*);
    BOOL (WINAPI *CryptUnprotectData_p)(Blob*, LPWSTR*, Blob*, PVOID, PVOID, DWORD, Blob*);
};

struct DwmApi {
    HMODULE module;
    HRESULT (WINAPI *DwmFlush_p)(void);
    HRESULT (WINAPI *DwmIsCompositionEnabled_p)(BOOL*);
    HRESULT (WINAPI *DwmSetWindowAttribute_p)(HWND, DWORD, LPCVOID, DWORD);
};

struct NtApi {
    HMODULE module;
    LONG (WINAPI *NtSetTimerResolution_p)(ULONG, BOOLEAN, PULONG);
};

struct CpuSample {
    ULONGLONG sysIdle;
    ULONGLONG sysKernel;
    ULONGLONG sysUser;
    ULONGLONG procKernel;
    ULONGLONG procUser;
    BOOL valid;
};

/* globals (v8k_globals.cpp) */
extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hEmailEdit;
extern HWND g_hKeyEdit;
extern HHOOK g_mouseHook;
extern HANDLE g_hookThread;
extern HANDLE g_hookReady;
extern DWORD g_hookThreadId;
extern volatile LONG g_hookOk;
extern HANDLE g_rawThread;
extern HANDLE g_rawReady;
extern DWORD g_rawThreadId;
extern HWND g_rawHwnd;
extern HANDLE g_engineThread;
extern HANDLE g_rawEvent;
extern volatile LONG g_shutdown;
extern volatile LONG g_rawRegistered;
extern volatile LONG g_queueHead;
extern volatile LONG g_queueTail;
extern RawMove g_queue[V8K_QUEUE_SIZE];
extern LARGE_INTEGER g_qpcFreq;
extern LARGE_INTEGER g_lastPhysicalQpc;
extern LARGE_INTEGER g_lastInjectQpc;
extern LARGE_INTEGER g_engineHeartbeatQpc;
extern Settings g_settings;
extern MotionState g_motion;
extern Stats g_stats;
extern HardwareInfo g_hw;
extern PointerInfo g_pointer;
extern LicenseState g_license;
extern RegApi g_reg;
extern CryptoApi g_crypto;
extern DwmApi g_dwm;
extern NtApi g_nt;
extern CpuSample g_cpuLast;
extern HFONT g_font;
extern HFONT g_fontBold;
extern HBRUSH g_editBrush;
extern int g_activeTab;
extern int g_dragSlider;
extern RECT g_startButton;
extern RECT g_modeButton;
extern RECT g_syncButton;
extern RECT g_licenseButton;
extern RECT g_sliderRects[5];
extern LARGE_INTEGER g_lastM1HookQpc;
extern HANDLE g_lastNamedDevice;
extern volatile LONG g_Injecting;
extern volatile LONG g_SkipNextRaw;
extern double g_physDtUsRing[V8K_PHYS_DT_SAMPLES];
extern volatile LONG g_physDtRingFill;

extern const BYTE g_dpapiEntropy[];
extern const BYTE g_licenseSecret[];

void CopyW(WCHAR* dst, int cap, const WCHAR* src);
void AppendW(WCHAR* dst, int cap, const WCHAR* src);
double ClampD(double v, double lo, double hi);
int ClampI(int v, int lo, int hi);
int ComputeMicroSteps(double physicalHz);
double ComputeVirtualHz(double physicalHz);
int RoundI(double v);
ULONGLONG FileTimeToU64(const FILETIME& ft);
ULONGLONG FtNow();
double QpcElapsedUs(const LARGE_INTEGER& a, const LARGE_INTEGER& b);
LONGLONG UsToQpc(double us);
void SetWarning(const WCHAR* text);
BOOL LoadDynamicApis(void);
BOOL LicenseAllowsEngine(void);
BOOL ActivateLicenseFromUi(void);
void LoadLicenseState(void);
void DetectHardware(void);
void DetectRawMouse(void);
void ApplyPerformanceMode(void);
int GcdI(int a, int b);
ULONGLONG FtToU64(FILETIME ft);
void UpdateM1WindowLatency(void);
void LayoutLicenseControls(RECT client);
BOOL PtInRectI(RECT r, int x, int y);
void SliderSetValue(int idx, int x);
BOOL RegisterRawMouse(HWND hwnd);
void UnregisterRawMouse(void);
void ReregisterRawMouse(void);
BOOL QueuePush(const RawMove& mv);
BOOL QueuePop(RawMove* mv);
int QueueDepth(void);
void QueueClear(void);
BOOL QueueCoalesceLatest(RawMove* mv, int* mergedCount);
BOOL QueuePopSingle(RawMove* mv);
BOOL QueueFlushToLatest(RawMove* mv, int* dropped);
double EstimatePhysicalHzPeak(void);
BOOL ShouldSkipRaw(HANDLE device, const LARGE_INTEGER& now);
void ProcessRawInput(LPARAM lParam);
LRESULT CALLBACK RawWndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI RawThreadProc(LPVOID);
DWORD WINAPI HookThreadProc(LPVOID);
DWORD WINAPI EngineThreadProc(LPVOID);
BOOL InjectRelativeMove(int dx, int dy);
BOOL InjectRelativeBatch(const int* dx, const int* dy, int count);
void ProcessInterpolatedMove(const RawMove& mv);
BOOL EnsureMouseHook(BOOL on);
void ToggleEngine(void);
void PaintUi(HWND hwnd);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int RunApp(HINSTANCE hInst, int nCmdShow);
void RefreshPointerInfo(void);
void UpdateCpuUsage(void);
void HandleClickDown(HWND hwnd, int x, int y);

#endif
