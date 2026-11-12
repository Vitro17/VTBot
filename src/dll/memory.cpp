#include "memory.h"
#include <windows.h>
#include <psapi.h>
#include <sstream>

namespace vt::mem {

uintptr_t ModuleBase(const wchar_t* name) { return (uintptr_t)GetModuleHandleW(name); }

size_t ModuleSize(const wchar_t* name) {
    MODULEINFO mi{};
    GetModuleInformation(GetCurrentProcess(), GetModuleHandleW(name), &mi, sizeof(mi));
    return mi.SizeOfImage;
}

bool SafeRead(uintptr_t addr, void* out, size_t size) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, out, size, &got) && got == size;
}

std::vector<uintptr_t> FindByVtable(uintptr_t vtable, size_t maxHits) {
    std::vector<uintptr_t> out;
    std::vector<uint8_t> buf;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    while (out.size() < maxHits && VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        addr = base + mbi.RegionSize;
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE) continue;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) continue;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) continue;
        buf.resize(mbi.RegionSize);
        if (!SafeRead(base, buf.data(), mbi.RegionSize)) continue;
        const uint64_t* q = (const uint64_t*)buf.data();
        size_t n = mbi.RegionSize / 8;
        for (size_t i = 0; i < n && out.size() < maxHits; ++i)
            if (q[i] == vtable) out.push_back(base + i * 8);
    }
    return out;
}

void ScanPrivate(const std::function<bool(uintptr_t, const uint8_t*, size_t)>& cb) {
    std::vector<uint8_t> buf;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        addr = base + mbi.RegionSize;
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE) continue;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) continue;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) continue;
        buf.resize(mbi.RegionSize);
        if (!SafeRead(base, buf.data(), mbi.RegionSize)) continue;
        if (!cb(base, buf.data(), mbi.RegionSize)) return;
    }
}

bool IsLiveQObject(uintptr_t obj, uintptr_t vtable) {
    if (!obj || (obj & 7)) return false;
    if (Read<uint64_t>(obj) != vtable) return false;
    uintptr_t d = (uintptr_t)Read<uint64_t>(obj + 8);
    if (!d || (d & 7)) return false;
    return (uintptr_t)Read<uint64_t>(d + 8) == obj;
}

bool LooksLikeStack(uintptr_t a) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((LPCVOID)a, &mbi, sizeof(mbi))) return true;
    MEMORY_BASIC_INFORMATION above;
    uintptr_t allocBase = (uintptr_t)mbi.AllocationBase;
    if (VirtualQuery((LPCVOID)allocBase, &above, sizeof(above)) && (above.Protect & PAGE_GUARD)) return true;
    return mbi.Type != MEM_PRIVATE;
}

uintptr_t PatternScan(const std::string& pattern, const wchar_t* module) {
    std::vector<int> bytes;
    std::istringstream ss(pattern);
    std::string tok;
    while (ss >> tok) bytes.push_back(tok == "?" || tok == "??" ? -1 : (int)strtol(tok.c_str(), nullptr, 16));

    uintptr_t base = ModuleBase(module);
    size_t size = ModuleSize(module);
    auto* p = (const uint8_t*)base;
    for (size_t i = 0; i + bytes.size() < size; ++i) {
        size_t j = 0;
        for (; j < bytes.size(); ++j)
            if (bytes[j] != -1 && p[i + j] != bytes[j]) break;
        if (j == bytes.size()) return base + i;
    }
    return 0;
}
}
