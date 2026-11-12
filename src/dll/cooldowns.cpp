#include "cooldowns.h"
#include "memory.h"
#include "offsets.h"
#include "log.h"
#include <windows.h>
#include <unordered_set>

namespace vt::cooldowns {

// Layout 15.03 (verificado por observacion): TCooldownStorage+0x20 = cabeza del std::map, +0x28 = tamano.
// Nodo MSVC: +0x19 isnil, +0x20 clave (uint16, el resto del qword es relleno sin inicializar), +0x28 fin en
// milisegundos desde epoch (QDateTime::currentMSecsSinceEpoch), igual que System time.
constexpr uint32_t kMapHead = 0x20, kMapSize = 0x28;
constexpr uint32_t kNodeIsNil = 0x19, kNodeKey = 0x20, kNodeEnd = 0x28;

static uintptr_t g_storage = 0;

static int64_t NowEpochMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;   // 100 ns desde 1601
    return (int64_t)((t - 116444736000000000ULL) / 10000ULL);
}

static bool StorageSane(uintptr_t st, uintptr_t vt) {
    if (!mem::IsLiveQObject(st, vt)) return false;
    return mem::Read<uint64_t>(st + kMapSize) < 10000 && mem::Read<uint64_t>(st + kMapHead) != 0;
}

static uintptr_t FindStorage() {
    const auto& o = GetOffsets();
    if (!o.cooldownVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.cooldownVtable;
    if (g_storage && StorageSane(g_storage, vt)) return g_storage;
    g_storage = 0;
    static DWORD lastTry = 0;
    if (GetTickCount() - lastTry < 3000) return 0;
    lastTry = GetTickCount();
    auto hits = mem::FindByVtable(vt, 8);
    for (uintptr_t h : hits) {
        if (mem::LooksLikeStack(h) || !StorageSane(h, vt)) continue;
        g_storage = h;
        break;
    }
    Log("TCooldownStorage: candidatos=%zu elegido=0x%llx", hits.size(), (unsigned long long)g_storage);
    return g_storage;
}

bool Active(std::vector<Entry>& out) {
    out.clear();
    uintptr_t st = FindStorage();
    if (!st) return false;
    int64_t now = NowEpochMs();
    uintptr_t head = (uintptr_t)mem::Read<uint64_t>(st + kMapHead);
    uintptr_t root = head ? (uintptr_t)mem::Read<uint64_t>(head + 8) : 0;
    std::vector<uintptr_t> stack;
    std::unordered_set<uintptr_t> seen;
    if (root) stack.push_back(root);
    while (!stack.empty() && seen.size() < 256) {
        uintptr_t n = stack.back(); stack.pop_back();
        if (!n || (n & 7) || !seen.insert(n).second) continue;
        if (mem::Read<uint8_t>(n + kNodeIsNil, 1) != 0) continue;
        stack.push_back((uintptr_t)mem::Read<uint64_t>(n));
        stack.push_back((uintptr_t)mem::Read<uint64_t>(n + 0x10));
        int id = (int)(mem::Read<uint64_t>(n + kNodeKey) & 0xFFFF);
        int64_t end = mem::Read<int64_t>(n + kNodeEnd);
        if (end > now) out.push_back({id, end - now});
    }
    return true;
}

int64_t Remaining(const std::vector<Entry>& list, int id) {
    for (const auto& e : list) if (e.id == id) return e.remainingMs;
    return 0;
}
}
