#include "v8k_common.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    FreeConsole();
    return RunApp(hInst, nCmdShow);
}

int main() {
    return wWinMain(GetModuleHandleW(0), 0, GetCommandLineW(), SW_SHOWNORMAL);
}
