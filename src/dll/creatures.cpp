#include "creatures.h"
#include "memory.h"
#include "offsets.h"
#include "log.h"
#include <windows.h>
#include <unordered_set>

namespace vt::creatures {

// Layout de la build 15.03 (verificado por diferencia de memoria):
//   TCreatureStorage: +0x10 cabeza del std::map, +0x18 tamano, +0x20 id propio (uint32), +0x2C id objetivo
//   Nodo del map (MSVC): +0x00 left, +0x08 parent, +0x10 right, +0x19 isnil, +0x20 key(uint32), +0x28 shared_ptr.ptr
//   TCreature: +0x20 id, +0x28 QString nombre (d, ptr, size), +0x60 x, +0x64 y, +0x68 z (uint32), +0xE8 vida% (byte)
constexpr uint32_t kMapHead = 0x10, kMapSize = 0x18, kSelfId = 0x20, kTargetId = 0x2C;
constexpr uint32_t kNodeIsNil = 0x19, kNodeKey = 0x20, kNodeVal = 0x28;
constexpr uint32_t kCrId = 0x20, kCrNamePtr = 0x30, kCrNameSize = 0x38;
constexpr uint32_t kCrX = 0x60, kCrY = 0x64, kCrZ = 0x68, kCrHp = 0xE8;

static uintptr_t g_storage = 0;

// Cuenta los nodos alcanzables del std::map; un almacen liberado/corrupto no da el mismo numero que su tamano.
static size_t CountNodes(uintptr_t head) {
    uintptr_t root = head ? (uintptr_t)mem::Read<uint64_t>(head + 8) : 0;
    std::vector<uintptr_t> stack;
    std::unordered_set<uintptr_t> seen;
    if (root) stack.push_back(root);
    while (!stack.empty() && seen.size() < 4096) {
        uintptr_t n = stack.back(); stack.pop_back();
        if (!n || (n & 7) || !seen.insert(n).second) continue;
        if (mem::Read<uint8_t>(n + kNodeIsNil, 1) != 0) continue;
        stack.push_back((uintptr_t)mem::Read<uint64_t>(n));
        stack.push_back((uintptr_t)mem::Read<uint64_t>(n + 0x10));
    }
    return seen.size();
}

static bool StorageSane(uintptr_t st, uintptr_t vt) {
    if (!mem::IsLiveQObject(st, vt)) return false;
    uint32_t self = mem::Read<uint32_t>(st + kSelfId);
    uint64_t size = mem::Read<uint64_t>(st + kMapSize);
    if ((self >> 28) != 1 || size == 0 || size >= 100000) return false;
    uintptr_t head = (uintptr_t)mem::Read<uint64_t>(st + kMapHead);
    // el arbol incluye el nodo cabeza en `seen` solo si es alcanzable; aceptamos size +-1
    size_t n = CountNodes(head);
    return n + 1 >= size && n <= size + 1;
}

static uintptr_t FindStorage() {
    const auto& o = GetOffsets();
    if (!o.storageVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.storageVtable;
    if (g_storage && StorageSane(g_storage, vt)) return g_storage;
    g_storage = 0;
    static DWORD lastTry = 0;
    if (GetTickCount() - lastTry < 3000) return 0;   // el escaneo del heap es caro: no repetirlo cada tick
    lastTry = GetTickCount();
    auto hits = mem::FindByVtable(vt, 16);
    for (uintptr_t h : hits) {
        if (mem::LooksLikeStack(h) || !StorageSane(h, vt)) continue;
        g_storage = h;
        break;
    }
    Log("TCreatureStorage: candidatos=%zu elegido=0x%llx", hits.size(), (unsigned long long)g_storage);
    return g_storage;
}

uintptr_t StorageAddr() { return FindStorage(); }

static std::string ReadName(uintptr_t cr) {
    uint64_t size = mem::Read<uint64_t>(cr + kCrNameSize);
    uintptr_t ptr = (uintptr_t)mem::Read<uint64_t>(cr + kCrNamePtr);
    if (!ptr || size == 0 || size > 64) return "";
    wchar_t w[65] = {};
    if (!mem::SafeRead(ptr, w, (size_t)size * 2)) return "";
    char u[200] = {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)size, u, sizeof(u) - 1, nullptr, nullptr);
    return std::string(u, n > 0 ? n : 0);
}

static bool ReadCreature(uintptr_t cr, Creature& c) {
    if (!cr || (cr & 7)) return false;
    c.id = mem::Read<uint32_t>(cr + kCrId);
    if (!c.id) return false;
    c.x = (int)mem::Read<uint32_t>(cr + kCrX);
    c.y = (int)mem::Read<uint32_t>(cr + kCrY);
    c.z = (int)mem::Read<uint32_t>(cr + kCrZ);
    c.hp = mem::Read<uint8_t>(cr + kCrHp);
    c.name = ReadName(cr);
    return true;
}

bool Snapshot(View& out) {
    uintptr_t st = FindStorage();
    if (!st) return false;
    out.all.clear();
    out.targetId = mem::Read<uint32_t>(st + kTargetId);
    uint32_t selfId = mem::Read<uint32_t>(st + kSelfId);

    // Recorrido iterativo del arbol rojo-negro (los accesos son seguros; el arbol puede mutar en otro hilo).
    uintptr_t head = (uintptr_t)mem::Read<uint64_t>(st + kMapHead);
    uintptr_t root = head ? (uintptr_t)mem::Read<uint64_t>(head + 8) : 0;
    std::vector<uintptr_t> stack;
    std::unordered_set<uintptr_t> seen;
    if (root) stack.push_back(root);
    while (!stack.empty() && seen.size() < 1024) {
        uintptr_t n = stack.back(); stack.pop_back();
        if (!n || (n & 7) || !seen.insert(n).second) continue;
        if (mem::Read<uint8_t>(n + kNodeIsNil, 1) != 0) continue;
        stack.push_back((uintptr_t)mem::Read<uint64_t>(n));          // left
        stack.push_back((uintptr_t)mem::Read<uint64_t>(n + 0x10));   // right
        uint32_t key = mem::Read<uint32_t>(n + kNodeKey);
        uintptr_t cr = (uintptr_t)mem::Read<uint64_t>(n + kNodeVal);
        Creature c;
        if (!ReadCreature(cr, c) || c.id != key) continue;
        if (c.id == selfId) out.self = c;
        out.all.push_back(std::move(c));
    }
    return out.self.id == selfId && selfId != 0;
}
}
