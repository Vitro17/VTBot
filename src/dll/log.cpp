#include "log.h"
#include <windows.h>
#include <cstdio>
#include <share.h>
#include <cstdarg>
#include <string>

namespace vt {
static FILE* g_file = nullptr;

void LogInit() {
    wchar_t path[MAX_PATH];
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&LogInit, &self);
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring p(path);
    p = p.substr(0, p.find_last_of(L'.')) + L".log";   // <nombre_dll>.log
    g_file = _wfsopen(p.c_str(), L"w", _SH_DENYNO);
}

void Log(const char* fmt, ...) {
    if (!g_file) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_file, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap; va_start(ap, fmt); vfprintf(g_file, fmt, ap); va_end(ap);
    fputc('\n', g_file); fflush(g_file);
}
}
