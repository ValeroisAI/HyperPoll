#include "v8k_common.h"

void UpdateM1HookLatency(const MSLLHOOKSTRUCT* ms);

LRESULT CALLBACK LowLevelMouseProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        if (wp == WM_LBUTTONDOWN) {
            UpdateM1HookLatency((MSLLHOOKSTRUCT*)lp);
        } else if (wp == WM_MOUSEMOVE &&
            InterlockedCompareExchange(&g_settings.enabled, 0, 0) &&
            InterlockedCompareExchange(&g_settings.suppressLegacyMove, 0, 0)) {
            MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lp;
            if (ms && !(ms->flags & LLMHF_INJECTED)) {
                return 1;
            }
        }
    }
    return CallNextHookEx(g_mouseHook, code, wp, lp);
}

DWORD WINAPI HookThreadProc(void*) {
    g_hookThreadId = GetCurrentThreadId();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    MSG msg;
    PeekMessageW(&msg, 0, WM_USER, WM_USER, PM_NOREMOVE);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_hInst, 0);
    InterlockedExchange(&g_hookOk, g_mouseHook ? 1 : 0);
    if (g_hookReady) SetEvent(g_hookReady);
    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = 0;
    }
    return 0;
}
