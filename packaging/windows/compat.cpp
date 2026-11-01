// Cutline.exe aus Installationen vor der Umbenennung: startet nur noch Vidline.exe mit denselben Argumenten.
// Der alte Updater, "Öffnen mit"-Einträge und angeheftete Verknüpfungen zeigen weiter auf diesen Namen.
#include <windows.h>

#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR args, int) {
    wchar_t path[32768];
    const DWORD n = GetModuleFileNameW(nullptr, path, 32768);
    std::wstring exe(path, n);
    exe.resize(exe.find_last_of(L"\\/") + 1);
    exe += L"Vidline.exe";
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
