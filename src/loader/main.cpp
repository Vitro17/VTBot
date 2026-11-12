// vtloader: inyecta vtbot.dll en un proceso x64 (client.exe) via CreateRemoteThread + LoadLibraryW
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <filesystem>

static DWORD FindProcess(const wchar_t* name, const wchar_t* skipWithModule = nullptr) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{sizeof(pe)};
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static bool Inject(DWORD pid, const std::wstring& dllPath) {
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                              PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { printf("[-] OpenProcess fallo (%lu). Ejecuta como administrador?\n", GetLastError()); return false; }

    size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(proc, remote, dllPath.c_str(), bytes, nullptr)) {
        printf("[-] No se pudo escribir en el proceso (%lu)\n", GetLastError());
        CloseHandle(proc); return false;
    }
    auto loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE th = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
    if (!th) { printf("[-] CreateRemoteThread fallo (%lu)\n", GetLastError()); CloseHandle(proc); return false; }
    WaitForSingleObject(th, 10000);
    DWORD ret = 0; GetExitCodeThread(th, &ret);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(th); CloseHandle(proc);
    printf(ret ? "[+] vtbot.dll cargada.\n" : "[-] LoadLibrary devolvio NULL (dependencias o arquitectura).\n");
    return ret != 0;
}

// Uso: vtloader.exe [pid]          inyecta una copia unica de vtbot.dll
//      vtloader.exe eject [pid]    pide a los vtbot cargados que se descarguen solos
int wmain(int argc, wchar_t** argv) {
    bool eject = argc > 1 && _wcsicmp(argv[1], L"eject") == 0;
    int pidArg = eject ? 2 : 1;
    DWORD pid = argc > pidArg ? (DWORD)_wtoi(argv[pidArg]) : FindProcess(L"client.exe");
    if (!pid) { printf("[-] No se encontro client.exe. Uso: vtloader.exe [eject] [pid]\n"); return 1; }

    if (eject) {
        wchar_t name[64]; swprintf(name, 64, L"Local\\VTBot.Unload.%lu", pid);
        HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
        if (!ev) { printf("[-] No hay ningun VTBot cargado en PID %lu\n", pid); return 1; }
        SetEvent(ev); CloseHandle(ev);
        printf("[+] Senal de descarga enviada.\n");
        return 0;
    }

    {   // evitar dos bots a la vez: el bot cargado mantiene este evento abierto mientras vive
        wchar_t name[64]; swprintf(name, 64, L"Local\\VTBot.Unload.%lu", pid);
        if (HANDLE ev = OpenEventW(SYNCHRONIZE, FALSE, name)) {
            CloseHandle(ev);
            printf("[!] VTBot ya esta cargado en este cliente (PID %lu). Si no lo ves, usa el boton VT del cliente.\n"
                   "    Para recargarlo: vtloader.exe eject  (espera unos segundos) y vuelve a inyectar.\n", pid);
            return 2;
        }
    }
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    auto dir = std::filesystem::path(self).parent_path();
    auto dll = dir / L"vtcore.dll";
    if (!std::filesystem::exists(dll)) { printf("[-] vtcore.dll no esta junto al loader.\n"); return 1; }

    // copia con nombre unico: permite recompilar vtbot.dll aunque haya una version cargada
    auto copy = dir / (L"vtbot_" + std::to_wstring(GetTickCount64()) + L".dll");
    std::filesystem::copy_file(dll, copy, std::filesystem::copy_options::overwrite_existing);

    printf("[*] Inyectando %ls en PID %lu\n", copy.filename().c_str(), pid);
    return Inject(pid, copy.wstring()) ? 0 : 1;
}
