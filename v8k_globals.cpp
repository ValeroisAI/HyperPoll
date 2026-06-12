#include "v8k_common.h"

HINSTANCE g_hInst = 0;
volatile LONG g_Injecting = 0;
volatile LONG g_SkipNextRaw = 0;
double g_physDtUsRing[V8K_PHYS_DT_SAMPLES];
volatile LONG g_physDtRingFill = 0;
HWND g_hWnd = 0;
HWND g_hEmailEdit = 0;
HWND g_hKeyEdit = 0;
HHOOK g_mouseHook = 0;
HANDLE g_hookThread = 0;
HANDLE g_hookReady = 0;
DWORD g_hookThreadId = 0;
volatile LONG g_hookOk = 0;
HANDLE g_rawThread = 0;
HANDLE g_rawReady = 0;
DWORD g_rawThreadId = 0;
HWND g_rawHwnd = 0;
HANDLE g_engineThread = 0;
HANDLE g_rawEvent = 0;
volatile LONG g_shutdown = 0;
volatile LONG g_rawRegistered = 0;
volatile LONG g_queueHead = 0;
volatile LONG g_queueTail = 0;
RawMove g_queue[V8K_QUEUE_SIZE];
LARGE_INTEGER g_qpcFreq;
LARGE_INTEGER g_lastPhysicalQpc;
LARGE_INTEGER g_lastInjectQpc;
LARGE_INTEGER g_engineHeartbeatQpc;
Settings g_settings = {0, 0, 1, 0, 0, 0, 0.015, 0.18, 0.25, 0.0, 8000.0};
MotionState g_motion = {0.0, 0.0, 0.0, 0.0, 0};
Stats g_stats;
HardwareInfo g_hw;
PointerInfo g_pointer;
LicenseState g_license;
RegApi g_reg;
CryptoApi g_crypto;
DwmApi g_dwm;
NtApi g_nt;
CpuSample g_cpuLast;
HFONT g_font = 0;
HFONT g_fontBold = 0;
HBRUSH g_editBrush = 0;
int g_activeTab = 0;
int g_dragSlider = -1;
RECT g_startButton;
RECT g_modeButton;
RECT g_syncButton;
RECT g_licenseButton;
RECT g_sliderRects[5];
LARGE_INTEGER g_lastM1HookQpc;
HANDLE g_lastNamedDevice = 0;

const BYTE g_dpapiEntropy[] = {
    0x56,0x38,0x4B,0x2D,0x44,0x50,0x41,0x50,0x49,0x2D,0x32,0x30,0x32,0x36
};
const BYTE g_licenseSecret[] = {
    0x22,0x81,0x11,0x9A,0xB0,0x4D,0x7C,0xFA,0x6E,0x3B,0x43,0x91,0xC4,0x55,0x0D,0x70,
    0x19,0xE0,0xA4,0xA5,0x31,0x6D,0x3E,0x02,0x97,0x13,0xC8,0x45,0xB7,0x28,0x5F,0xD1
};
