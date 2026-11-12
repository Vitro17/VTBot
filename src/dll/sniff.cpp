#include "sniff.h"
#include "offsets.h"
#include "memory.h"
#include "log.h"
#include "config.h"
#include "overlay.h"
#include <string>
#include <windows.h>
#include <mutex>

namespace vt::sniff {

static std::mutex g_mu;
static Info g_info;
static uintptr_t* g_slot = nullptr;       // direccion del slot parcheado en la vtable
static uintptr_t  g_orig = 0;
static uint32_t   g_lookSeq = 0, g_useSeq = 0;

using DispatchFn = void (*)(void* self, void** sp);

struct RawCtx {   // lectura "cruda" de un contexto de ubicacion
    Where    where = Where::None;
    int32_t  x = 0, y = 0, z = 0;
    uint32_t stack = 0, cid = 0, slot = 0;
};

// Sin destructores C++ para poder usar SEH.
static bool ReadCtxLoc(uintptr_t ctx, RawCtx* out) {
    const Offsets& o = GetOffsets();
    const uintptr_t base = mem::ModuleBase();
    __try {
        if (!ctx) return false;
        uintptr_t vt = *(uintptr_t*)ctx;
        if (vt == base + o.ctxWorldVtable) {
            out->where = Where::World;
            out->x = *(int32_t*)(ctx + 0x10);
            out->y = *(int32_t*)(ctx + 0x14);
            out->z = *(int32_t*)(ctx + 0x18);
            out->stack = *(uint8_t*)(ctx + 0x20);
        } else if (vt == base + o.ctxSlotVtable) {
            out->where = Where::Container;
            out->cid = *(uint8_t*)(ctx + 0x08);
            out->slot = *(uint32_t*)(ctx + 0x0C);
        } else {
            out->where = Where::Other;   // inventario/equipo u otro
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool ReadCtxItem(uintptr_t ctx, uint32_t* id) {
    __try {
        if (!ctx) return false;
        *id = *(uint16_t*)(ctx + 0x08);   // TObject...TypeGameActionContext {vtable, u16 id}
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void FillSeen(Seen& s, const RawCtx& c, uint32_t id, uint32_t seq) {
    s.valid = true; s.seq = seq; s.itemId = id; s.where = c.where;
    s.x = c.x; s.y = c.y; s.z = c.z; s.stack = c.stack; s.cid = c.cid; s.slot = c.slot;
    s.tick = GetTickCount();
}

static bool ReadPtr(uintptr_t addr, uintptr_t* out) {
    __try { *out = *(uintptr_t*)addr; return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Inspecciona la accion (shared_ptr<Accion>* sp). Nunca lanza excepciones.
static void Observe(void** sp) {
    const Offsets& o = GetOffsets();
    const uintptr_t base = mem::ModuleBase();
    uintptr_t act = 0, vt = 0;
    if (!sp || !ReadPtr((uintptr_t)sp, &act) || !act || !ReadPtr(act, &vt)) return;
    if (vt == base + o.lookVtable) {
        uintptr_t c1 = 0, c2 = 0;
        if (!ReadPtr(act + 0x08, &c1) || !ReadPtr(act + 0x18, &c2)) return;
        RawCtx loc; uint32_t id = 0;
        if (!ReadCtxLoc(c1, &loc) || !ReadCtxItem(c2, &id)) return;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            ++g_info.looks;
            FillSeen(g_info.look, loc, id, ++g_lookSeq);
        }
        {
            auto& t = GetConfig().tools;
            if (t.lookId) {
                int ms = t.lookIdMs; if (ms < 500) ms = 500; if (ms > 15000) ms = 15000;
                overlay::Show("ID: " + std::to_string(id), (unsigned)ms);
            }
        }
        if (loc.where == Where::World)
            Log("[look] item ID %u en (%d,%d,%d) pila %u", id, loc.x, loc.y, loc.z, loc.stack);
        else if (loc.where == Where::Container)
            Log("[look] item ID %u en contenedor %u slot %u", id, loc.cid, loc.slot);
        else
            Log("[look] item ID %u (inventario/equipo)", id);
    } else if (vt == base + o.useWithVtable) {
        uintptr_t c2 = 0, c3 = 0, c4 = 0;
        if (!ReadPtr(act + 0x18, &c2) || !ReadPtr(act + 0x28, &c3) || !ReadPtr(act + 0x38, &c4)) return;
        if (!c3) return;   // fase de "elegir objeto": aun sin destino
        uint32_t weapon = 0, target = 0;
        RawCtx loc;
        ReadCtxItem(c2, &weapon);
        ReadCtxItem(c4, &target);
        if (!ReadCtxLoc(c3, &loc)) return;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            ++g_info.useWiths;
            g_info.useWithItemId = weapon;
            FillSeen(g_info.useWithTarget, loc, target, ++g_useSeq);
        }
        Log("[usewith] item %u sobre ID %u en (%d,%d,%d) pila %u", weapon, target, loc.x, loc.y, loc.z, loc.stack);
    }
}

static void Hook(void* self, void** sp) {
    Observe(sp);
    ((DispatchFn)g_orig)(self, sp);
}

bool Install() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_slot) return true;
    const Offsets& o = GetOffsets();
    if (!o.genericHandlerVtable || !o.genericDispatchFn || !o.lookVtable || !o.useWithVtable) return false;
    const uintptr_t base = mem::ModuleBase();
    auto* slot = (uintptr_t*)(base + o.genericHandlerVtable + 8ull * o.genericDispatchSlot);
    uintptr_t cur = mem::Read<uint64_t>((uintptr_t)slot);
    if (cur != base + o.genericDispatchFn) {
        Log("sniff: el slot %u de la vtable apunta a 0x%llx (se esperaba 0x%llx); no se instala", o.genericDispatchSlot,
            (unsigned long long)(cur - base), (unsigned long long)o.genericDispatchFn);
        return false;
    }
    DWORD old = 0;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) { Log("sniff: VirtualProtect fallo (%lu)", GetLastError()); return false; }
    g_orig = cur;
    *(volatile uintptr_t*)slot = (uintptr_t)&Hook;
    VirtualProtect(slot, 8, old, &old);
    g_slot = slot;
    g_info.installed = true;
    Log("sniff: instalado (vtable 0x%llx slot %u)", (unsigned long long)o.genericHandlerVtable, o.genericDispatchSlot);
    return true;
}

void Remove() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_slot) return;
    DWORD old = 0;
    if (VirtualProtect(g_slot, 8, PAGE_READWRITE, &old)) {
        *(volatile uintptr_t*)g_slot = g_orig;
        VirtualProtect(g_slot, 8, old, &old);
    }
    g_slot = nullptr;
    g_info.installed = false;
    Log("sniff: retirado");
}

Info Get() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_info;
}

}
