#include "containers.h"
#include "memory.h"
#include "offsets.h"
#include "log.h"
#include <windows.h>
#include <set>

namespace vt::containers {

using mem::Read;

static uintptr_t g_storage = 0;

// Nodo de std::map de MSVC: +0 left, +8 parent, +0x10 right, +0x18 color, +0x19 _Isnil, +0x20 clave, +0x28 valor.
// Recorre el arbol y llama a cb(clave u8, puntero al TContainer, ctrl). Devuelve el numero de entradas visitadas
// o -1 si la estructura no cuadra.
template <typename F>
static int WalkMap(uintptr_t storage, uint32_t mapOff, F&& cb) {
    uintptr_t head = (uintptr_t)Read<uint64_t>(storage + mapOff);
    uint64_t size = Read<uint64_t>(storage + mapOff + 8);
    if (!head || (head & 7) || size > 64) return -1;
    if (Read<uint8_t>(head + 0x19) != 1) return -1;   // el nodo cabeza tiene _Isnil
    std::vector<uintptr_t> todo{(uintptr_t)Read<uint64_t>(head + 8)};
    std::set<uintptr_t> seen;
    int n = 0;
    while (!todo.empty() && n <= 64) {
        uintptr_t node = todo.back();
        todo.pop_back();
        if (!node || node == head || (node & 7) || !seen.insert(node).second) continue;
        if (Read<uint8_t>(node + 0x19)) continue;   // nil
        cb(Read<uint8_t>(node + 0x20), (uintptr_t)Read<uint64_t>(node + 0x28));
        ++n;
        todo.push_back((uintptr_t)Read<uint64_t>(node));
        todo.push_back((uintptr_t)Read<uint64_t>(node + 0x10));
    }
    return n;
}

static bool StorageOk(uintptr_t s, const Offsets& o, uintptr_t base) {
    bool good = true;
    int n = WalkMap(s, o.containerMapOff, [&](uint8_t, uintptr_t c) {
        if (!c || (uintptr_t)Read<uint64_t>(c) != base + o.containerVtable) good = false;
    });
    return n >= 0 && good;
}

static uintptr_t FindStorage() {
    const Offsets& o = GetOffsets();
    if (!o.containerStorageVtable || !o.containerVtable) return 0;
    const uintptr_t base = mem::ModuleBase();
    if (g_storage && (uintptr_t)Read<uint64_t>(g_storage) == base + o.containerStorageVtable &&
        StorageOk(g_storage, o, base))
        return g_storage;
    g_storage = 0;
    auto hits = mem::FindByVtable(base + o.containerStorageVtable, 16);
    for (uintptr_t h : hits) {
        if (mem::LooksLikeStack(h) || !StorageOk(h, o, base)) continue;
        g_storage = h;
        break;
    }
    Log("TContainerStorage: candidatos=%zu elegido=0x%llx", hits.size(), (unsigned long long)g_storage);
    return g_storage;
}

bool Snapshot(std::vector<Slot>& out) {
    out.clear();
    const Offsets& o = GetOffsets();
    uintptr_t st = FindStorage();
    if (!st) return false;
    const uintptr_t base = mem::ModuleBase();
    const uintptr_t cumVt = o.cumulativeVtable ? base + o.cumulativeVtable : 0;
    WalkMap(st, o.containerMapOff, [&](uint8_t cid, uintptr_t c) {
        uintptr_t vb = (uintptr_t)Read<uint64_t>(c + 0x68);
        uintptr_t ve = (uintptr_t)Read<uint64_t>(c + 0x70);
        if (!vb || ve < vb || (ve - vb) > 0x4000) return;
        size_t n = (ve - vb) / 16;
        for (size_t i = 0; i < n; ++i) {
            uintptr_t item = (uintptr_t)Read<uint64_t>(vb + i * 16);
            if (!item) continue;
            uintptr_t type = (uintptr_t)Read<uint64_t>(item + 8);
            if (!type) continue;
            Slot s;
            s.cid = cid;
            s.slot = (uint32_t)i;
            s.itemId = Read<uint32_t>(type);
            if (cumVt && (uintptr_t)Read<uint64_t>(item) == cumVt) s.count = Read<uint8_t>(item + 0x48);   // uint8
            out.push_back(s);
        }
    });
    return true;
}

void LogDump() {
    std::vector<Slot> v;
    if (!Snapshot(v)) { Log("contenedores: almacen no encontrado"); return; }
    Log("contenedores: %zu items", v.size());
    for (const auto& s : v)
        if (s.count) Log("  cont %u slot %u: id %u x%u", s.cid, s.slot, s.itemId, s.count);
}
}
