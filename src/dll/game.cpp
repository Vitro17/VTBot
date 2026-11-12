#include "game.h"
#include "memory.h"
#include "offsets.h"
#include "log.h"
#include <windows.h>
#include <vector>

namespace vt::game {

static uintptr_t g_player = 0;

static bool Plausible(uintptr_t obj, uintptr_t vtable, PlayerState* out = nullptr) {
    const auto& o = GetOffsets();
    uint64_t vt = 0;
    if (!mem::SafeRead(obj, &vt, 8) || vt != vtable) return false;
    uint32_t hp = mem::Read<uint32_t>(obj + o.hp), mhp = mem::Read<uint32_t>(obj + o.maxHp);
    uint32_t mp = mem::Read<uint32_t>(obj + o.mana), mmp = mem::Read<uint32_t>(obj + o.maxMana);
    if (mhp == 0 || mhp > 10'000'000 || hp > mhp) return false;
    if (mmp > 10'000'000 || mp > mmp) return false;
    if (out) { out->hp = hp; out->maxHp = mhp; out->mana = mp; out->maxMana = mmp; }
    return true;
}

uintptr_t FindPlayerObject(bool forceRescan) {
    const auto& o = GetOffsets();
    if (!o.valid) return 0;
    uintptr_t vtable = mem::ModuleBase() + o.playerVtable;

    if (!forceRescan && g_player && Plausible(g_player, vtable)) return g_player;
    g_player = 0;

    // Recorre el heap privado buscando qwords == vtable
    std::vector<uint8_t> buf;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    int candidates = 0;
    while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        addr = base + mbi.RegionSize;
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE) continue;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) continue;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) continue;

        buf.resize(mbi.RegionSize);
        if (!mem::SafeRead(base, buf.data(), mbi.RegionSize)) continue;
        const uint64_t* q = (const uint64_t*)buf.data();
        size_t n = mbi.RegionSize / 8;
        for (size_t i = 0; i < n; ++i) {
            if (q[i] != vtable) continue;
            ++candidates;
            uintptr_t obj = base + i * 8;
            if (Plausible(obj, vtable)) {
                g_player = obj;
                Log("objeto jugador encontrado: 0x%llx (candidatos vtable: %d)", (unsigned long long)obj, candidates);
                return g_player;
            }
        }
    }
    return 0;
}

bool ReadPlayer(PlayerState& out) {
    uintptr_t obj = FindPlayerObject();
    if (!obj) return false;
    return Plausible(obj, mem::ModuleBase() + GetOffsets().playerVtable, &out);
}
// TPlayerData es un QObject; se localiza por vtable y se valida como QObject vivo.
static uintptr_t g_playerData = 0;

static uintptr_t FindPlayerData() {
    const auto& o = GetOffsets();
    if (!o.valid || !o.playerDataVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.playerDataVtable;
    if (g_playerData && mem::IsLiveQObject(g_playerData, vt)) return g_playerData;
    g_playerData = 0;
    static DWORD lastScan = 0;
    if (lastScan && GetTickCount() - lastScan < 3000) return 0;   // no reescanear el heap en cada llamada
    lastScan = GetTickCount();
    auto hits = mem::FindByVtable(vt, 16);
    for (uintptr_t h : hits) {
        if (mem::LooksLikeStack(h) || !mem::IsLiveQObject(h, vt)) continue;
        g_playerData = h;
        break;
    }
    if (g_playerData) Log("TPlayerData encontrado: 0x%llx (candidatos: %zu)", (unsigned long long)g_playerData, hits.size());
    return g_playerData;
}

bool ReadConditions(uint32_t& out) {
    uintptr_t p = FindPlayerData();
    return p && mem::SafeRead(p + GetOffsets().conditionsOff, &out, 4);
}

bool ReadRegenSeconds(uint32_t& out) {
    uintptr_t p = FindPlayerData();
    return p && mem::SafeRead(p + GetOffsets().regenOff, &out, 4);
}
}