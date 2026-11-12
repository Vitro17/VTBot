#include "actions.h"
#include "memory.h"
#include "offsets.h"
#include "qt.h"
#include "log.h"
#include "creatures.h"
#include "containers.h"
#include <windows.h>

namespace vt::actions {

static uintptr_t g_chat = 0;

// void qt_static_metacall(QObject* obj, QMetaObject::Call c, int id, void** argv)
using MetaCallFn = void (*)(void* obj, int call, int id, void** argv);

using mem::IsLiveQObject;
using mem::LooksLikeStack;

static uintptr_t FindChat() {
    const auto& o = GetOffsets();
    if (!o.chatVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.chatVtable;
    if (g_chat && IsLiveQObject(g_chat, vt)) return g_chat;   // sigue valido
    g_chat = 0;
    auto hits = mem::FindByVtable(vt, 32);
    int valid = 0;
    for (uintptr_t h : hits) {
        if (LooksLikeStack(h) || !IsLiveQObject(h, vt)) continue;
        ++valid;
        if (!g_chat) g_chat = h;
    }
    Log("TChatController: candidatos=%zu validos=%d elegido=0x%llx", hits.size(), valid,
        (unsigned long long)g_chat);
    return g_chat;
}

// ---------------------------------------------------------------------------------------------------------
// Uso de items sobre uno mismo. Se replica lo que hace TActionButtonGameActionHandler al pulsar una hotkey:
//   settings(boton) -> fabrica -> shared_ptr<TGameActionUseOnYourself> -> senal publishGameAction.
// El cliente resuelve el item en inventario/contenedores por si mismo (por ID de tipo).
// ---------------------------------------------------------------------------------------------------------
static uintptr_t g_useHandler = 0;

static uintptr_t FindUseHandler() {
    const auto& o = GetOffsets();
    if (!o.useHandlerVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.useHandlerVtable;
    auto ok = [&](uintptr_t h) {   // vivo y con fabrica de la clase esperada (si se conoce su vtable)
        if (!IsLiveQObject(h, vt)) return false;
        if (!o.useFactoryVtable) return true;
        uintptr_t f = (uintptr_t)mem::Read<uint64_t>(h + o.useFactoryField);
        return f && (uintptr_t)mem::Read<uint64_t>(f) == mem::ModuleBase() + o.useFactoryVtable;
    };
    if (g_useHandler && ok(g_useHandler)) return g_useHandler;
    g_useHandler = 0;
    auto hits = mem::FindByVtable(vt, 32);
    int valid = 0;
    for (uintptr_t h : hits) {
        if (LooksLikeStack(h) || !ok(h)) continue;
        ++valid;
        if (!g_useHandler) g_useHandler = h;
    }
    Log("ActionButtonHandler: candidatos=%zu validos=%d elegido=0x%llx", hits.size(), valid,
        (unsigned long long)g_useHandler);
    return g_useHandler;
}

using FactoryFn = void* (*)(void* factory, void** outSharedPtr, void* settings);

// Llama a la fabrica; devuelve false si hubo una excepcion SEH. Funcion sin objetos C++ para poder usar __try.
static bool CallFactory(FactoryFn fn, void* factory, void** out, void* settings) {
    __try {
        fn(factory, out, settings);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool EmitSignal(void* handler, const void* mo, int idx, void** out) {
    __try {
        void* argv[2] = {nullptr, out};
        return qt::Activate(handler, mo, idx, argv);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Libera un std::shared_ptr (ctrl = bloque de control MSVC: +8 uses, +0xc weaks, vtable [dispose, destroy]).
static void ReleaseShared(void* ctrl) {
    if (!ctrl) return;
    auto* c = (volatile LONG*)((uint8_t*)ctrl + 8);
    auto* w = (volatile LONG*)((uint8_t*)ctrl + 0xc);
    void** vt = *(void***)ctrl;
    if (InterlockedDecrement(c) == 0) {
        ((void (*)(void*))vt[0])(ctrl);
        if (InterlockedDecrement(w) == 0) ((void (*)(void*))vt[1])(ctrl);
    }
}

bool UseItem(uint32_t itemId, int kind) {
    static bool qtOk = qt::Init();
    if (!qtOk || !itemId) return false;
    const Offsets o = GetOffsets();
    if (!o.useFactoryFn || !o.usePublishMeta) return false;
    uintptr_t h = FindUseHandler();
    if (!h) return false;
    uintptr_t factory = (uintptr_t)mem::Read<uint64_t>(h + o.useFactoryField);
    if (!factory || (factory & 7)) return false;

    const uintptr_t base = mem::ModuleBase();
    return qt::PostToObjectThread((void*)h, [=]() {
        alignas(16) uint8_t settings[0x100] = {};
        *(int32_t*)(settings + o.useSettingsKind) = kind;
        *(uint32_t*)(settings + o.useSettingsItem) = itemId;
        void* out[2] = {nullptr, nullptr};
        if (!CallFactory((FactoryFn)(base + o.useFactoryFn), (void*)factory, out, settings) || !out[0]) {
            Log("UseItem(%u,k%d): la fabrica no devolvio accion", itemId, kind);
            return;
        }
        if (!EmitSignal((void*)h, (const void*)(base + o.usePublishMeta), o.usePublishIndex, out))
            Log("UseItem(%u,k%d): fallo al emitir la accion", itemId, kind);
        ReleaseShared(out[1]);
    });
}

bool UseItemOnSelf(uint32_t itemId) { return UseItem(itemId, GetOffsets().useKindSelf); }

// ---------------------------------------------------------------------------------------------------------
// Click derecho > Usar sobre el slot de un contenedor. El cliente publica una TGameActionUse con dos contextos:
//   ctx1 TLocationContainerWithSlotGameActionContext {vtable, u8 idContenedor, u32 slot, u8 0, u8 0, u8 0}
//   ctx2 TObjectApperanceTypeGameActionContext       {vtable, u16 idItem}
// (make_shared: bloque de control {vtable, uses, weaks} + objeto). Se construyen con el operator new del cliente y
// los bloques de control reales, y se publican por la misma senal que usa el "comer" (publishGameAction).
// ---------------------------------------------------------------------------------------------------------
using NewFn2 = void* (*)(size_t);

static uint8_t* MakeBlock(NewFn2 alloc, size_t total, uintptr_t ctrlVtbl, uintptr_t objVtbl) {
    auto* b = (uint8_t*)alloc(total);
    if (!b) return nullptr;
    memset(b, 0, total);
    *(uint64_t*)(b + 0x00) = ctrlVtbl;
    *(uint32_t*)(b + 0x08) = 1;   // uses
    *(uint32_t*)(b + 0x0C) = 1;   // weaks
    *(uint64_t*)(b + 0x10) = objVtbl;
    return b;
}

// Debe ejecutarse en el hilo de Qt del manejador `h`. Devuelve true si se publico la accion.
static bool UseSlotOnThread(uintptr_t h, uint8_t cid, uint32_t slot, uint32_t itemId) {
    const Offsets& o = GetOffsets();
    const uintptr_t base = mem::ModuleBase();
    NewFn2 alloc = (NewFn2)(base + o.opNew);
    uint8_t* c1 = MakeBlock(alloc, 0x28, base + o.ctxSlotCtrl, base + o.ctxSlotVtable);
    uint8_t* c2 = MakeBlock(alloc, 0x20, base + o.ctxTypeCtrl, base + o.ctxTypeVtable);
    uint8_t* ac = MakeBlock(alloc, 0x38, base + o.useActionCtrl, base + o.useActionVtable);
    if (!c1 || !c2 || !ac) return false;
    *(uint8_t*)(c1 + 0x18) = cid;
    *(uint32_t*)(c1 + 0x1C) = slot;
    *(uint16_t*)(c2 + 0x18) = (uint16_t)itemId;
    *(uint64_t*)(ac + 0x18) = (uint64_t)(c1 + 0x10);   // ctx1.ptr
    *(uint64_t*)(ac + 0x20) = (uint64_t)c1;            // ctx1.ctrl
    *(uint64_t*)(ac + 0x28) = (uint64_t)(c2 + 0x10);   // ctx2.ptr
    *(uint64_t*)(ac + 0x30) = (uint64_t)c2;            // ctx2.ctrl
    void* out[2] = {ac + 0x10, ac};
    bool ok = EmitSignal((void*)h, (const void*)(base + o.usePublishMeta), o.usePublishIndex, out);
    ReleaseShared(ac);   // nuestra referencia; el cliente conserva las suyas
    return ok;
}

bool ConvertCoins(bool gold, bool platinum) {
    static bool qtOk = qt::Init();
    if (!qtOk || (!gold && !platinum)) return false;
    const Offsets o = GetOffsets();
    if (!o.usePublishMeta || !o.opNew || !o.useActionVtable || !o.useActionCtrl || !o.ctxSlotVtable ||
        !o.ctxSlotCtrl || !o.ctxTypeVtable || !o.ctxTypeCtrl)
        return false;
    uintptr_t h = FindUseHandler();
    if (!h) return false;
    return qt::PostToObjectThread((void*)h, [=]() {
        std::vector<containers::Slot> v;
        if (!containers::Snapshot(v)) { Log("[monedas] no se encontro el almacen de contenedores"); return; }
        for (const auto& s : v) {
            bool want = (s.itemId == 3031 && gold) || (s.itemId == 3035 && platinum);
            if (!want || s.count != 100) continue;
            bool ok = UseSlotOnThread(h, s.cid, s.slot, s.itemId);
            Log("[monedas] usar %u x100 (contenedor %u, slot %u) -> %s", s.itemId, s.cid, s.slot,
                ok ? "publicado" : "FALLO");
            return;   // uno por pasada (el temporizador volvera a llamar)
        }
    });
}

// ---------------------------------------------------------------------------------------------------------
// Usar un item de un contenedor CON una casilla del mapa (p.ej. arma de ejercicio sobre el dummy). El cliente publica
// una TGameActionUseWith (bloque 0x58) con 4 contextos: origen (contenedor+slot), tipo del item usado, destino
// (TLocationWorldMapCoordinateWithStackPositionContext: x,y,z,pila) y tipo del objeto destino.
// ---------------------------------------------------------------------------------------------------------
static bool UseWithOnThread(uintptr_t h, uint8_t cid, uint32_t slot, uint32_t itemId, int32_t x, int32_t y, int32_t z,
                            uint32_t stack, uint32_t targetId) {
    const Offsets& o = GetOffsets();
    const uintptr_t base = mem::ModuleBase();
    NewFn2 alloc = (NewFn2)(base + o.opNew);
    uint8_t* c1 = MakeBlock(alloc, 0x28, base + o.ctxSlotCtrl, base + o.ctxSlotVtable);
    uint8_t* c2 = MakeBlock(alloc, 0x20, base + o.ctxTypeCtrl, base + o.ctxTypeVtable);
    uint8_t* c3 = MakeBlock(alloc, 0x38, base + o.ctxWorldCtrl, base + o.ctxWorldVtable);
    uint8_t* c4 = MakeBlock(alloc, 0x20, base + o.ctxTypeCtrl, base + o.ctxTypeVtable);
    uint8_t* ac = MakeBlock(alloc, 0x58, base + o.useWithCtrl, base + o.useWithVtable);
    if (!c1 || !c2 || !c3 || !c4 || !ac) return false;
    *(uint8_t*)(c1 + 0x18) = cid;
    *(uint32_t*)(c1 + 0x1C) = slot;
    *(uint16_t*)(c2 + 0x18) = (uint16_t)itemId;
    *(uint64_t*)(c3 + 0x18) = base + o.ctxWorldSubVtable;   // obj+8: vtable del subobjeto coordenada
    *(int32_t*)(c3 + 0x20) = x;
    *(int32_t*)(c3 + 0x24) = y;
    *(int32_t*)(c3 + 0x28) = z;
    *(uint8_t*)(c3 + 0x30) = (uint8_t)stack;
    *(uint16_t*)(c4 + 0x18) = (uint16_t)targetId;
    *(uint64_t*)(ac + 0x18) = (uint64_t)(c1 + 0x10); *(uint64_t*)(ac + 0x20) = (uint64_t)c1;
    *(uint64_t*)(ac + 0x28) = (uint64_t)(c2 + 0x10); *(uint64_t*)(ac + 0x30) = (uint64_t)c2;
    *(uint64_t*)(ac + 0x38) = (uint64_t)(c3 + 0x10); *(uint64_t*)(ac + 0x40) = (uint64_t)c3;
    *(uint64_t*)(ac + 0x48) = (uint64_t)(c4 + 0x10); *(uint64_t*)(ac + 0x50) = (uint64_t)c4;
    void* out[2] = {ac + 0x10, ac};
    bool ok = EmitSignal((void*)h, (const void*)(base + o.usePublishMeta), o.usePublishIndex, out);
    ReleaseShared(ac);
    return ok;
}

bool UseItemOnMap(uint32_t itemId, int32_t x, int32_t y, int32_t z, uint32_t stack, uint32_t targetId) {
    static bool qtOk = qt::Init();
    if (!qtOk || !itemId) return false;
    const Offsets o = GetOffsets();
    if (!o.usePublishMeta || !o.opNew || !o.useWithVtable || !o.useWithCtrl || !o.ctxSlotVtable || !o.ctxSlotCtrl ||
        !o.ctxTypeVtable || !o.ctxTypeCtrl || !o.ctxWorldVtable || !o.ctxWorldCtrl || !o.ctxWorldSubVtable)
        return false;
    uintptr_t h = FindUseHandler();
    if (!h) return false;
    return qt::PostToObjectThread((void*)h, [=]() {
        std::vector<containers::Slot> v;
        if (!containers::Snapshot(v)) { Log("[dummy] no se encontro el almacen de contenedores"); return; }
        for (const auto& s : v) {
            if (s.itemId != itemId) continue;
            bool ok = UseWithOnThread(h, s.cid, s.slot, itemId, x, y, z, stack, targetId);
            Log("[dummy] usar %u (contenedor %u, slot %u) con (%d,%d,%d) pila %u id %u -> %s", itemId, s.cid, s.slot, x,
                y, z, stack, targetId, ok ? "publicado" : "FALLO");
            return;
        }
        Log("[dummy] el item %u no esta en ningun contenedor abierto", itemId);
    });
}

bool UseContainerSlot(uint8_t containerId, uint32_t slot, uint32_t itemId) {
    static bool qtOk = qt::Init();
    if (!qtOk) return false;
    const Offsets o = GetOffsets();
    if (!o.usePublishMeta || !o.opNew || !o.useActionVtable) return false;
    uintptr_t h = FindUseHandler();
    if (!h) return false;
    return qt::PostToObjectThread((void*)h, [=]() {
        bool ok = UseSlotOnThread(h, containerId, slot, itemId);
        Log("UseContainerSlot(c%u,s%u,id%u) -> %s", containerId, slot, itemId, ok ? "publicado" : "FALLO");
    });
}

// ---------------------------------------------------------------------------------------------------------
// Atacar una criatura por ID. TCreaturesGameActionHandler::handleAttack(this, shared_ptr<Accion>*) solo hace:
//   id = (*accion)->vtable[3]();   storage->setAttackedCreature(id);   emitir GameclientMessageAttack(id)
// Se le pasa una "accion" falsa nuestra: un objeto con una vtable cuyo slot 3 devuelve el ID.
// ---------------------------------------------------------------------------------------------------------
struct FakeCreatureAction { void** vtbl; uint32_t id; };
static uint32_t FakeGetCreatureId(FakeCreatureAction* a) { return a->id; }
static void* g_fakeVtbl[16] = {nullptr, nullptr, nullptr, (void*)&FakeGetCreatureId};

static uintptr_t g_atkHandler = 0;

static uintptr_t FindAttackHandler() {
    const auto& o = GetOffsets();
    if (!o.attackHandlerVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.attackHandlerVtable;
    uintptr_t storage = creatures::StorageAddr();
    if (!storage) return 0;
    auto ok = [&](uintptr_t h) {   // vivo y apuntando (+0x10) al almacen de criaturas vigente
        return IsLiveQObject(h, vt) && (uintptr_t)mem::Read<uint64_t>(h + 0x10) == storage;
    };
    if (g_atkHandler && ok(g_atkHandler)) return g_atkHandler;
    g_atkHandler = 0;
    auto hits = mem::FindByVtable(vt, 16);
    for (uintptr_t h : hits) {
        if (LooksLikeStack(h) || !ok(h)) continue;
        g_atkHandler = h;
        break;
    }
    Log("CreaturesHandler: candidatos=%zu elegido=0x%llx", hits.size(), (unsigned long long)g_atkHandler);
    return g_atkHandler;
}

using AttackFn = void (*)(void* handler, void** sharedPtrLike);

static bool CallAttack(AttackFn fn, void* handler, uint32_t id) {
    FakeCreatureAction act{g_fakeVtbl, id};
    void* sp[2] = {&act, nullptr};   // shared_ptr falso {ptr, ctrl}: handleAttack solo lee [sp]
    __try {
        fn(handler, sp);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool Attack(uint32_t creatureId) {
    static bool qtOk = qt::Init();
    if (!qtOk || !creatureId) return false;
    const Offsets o = GetOffsets();
    if (!o.attackFn) return false;
    uintptr_t h = FindAttackHandler();
    if (!h) return false;
    const uintptr_t fn = mem::ModuleBase() + o.attackFn;
    return qt::PostToObjectThread((void*)h, [=]() {
        if (!CallAttack((AttackFn)fn, (void*)h, creatureId))
            Log("Attack(0x%x): excepcion al llamar a handleAttack", creatureId);
    });
}

// ---------------------------------------------------------------------------------------------------------
// Girar el personaje (anti-AFK). Se construye lo mismo que el cliente: una TGameActionPlayerMovementIntent
// (make_shared: bloque de control {vtable, uses, weaks} + objeto {vtable, shared_ptr<intencion>}) que contiene
// una TMovementIntentRotate {vtable, u8 direccion, u8 flag}. Los objetos se reservan con el operator new del
// cliente y llevan las vtables/bloques de control reales, asi el cliente los libera solo al terminar.
// ---------------------------------------------------------------------------------------------------------
static uintptr_t g_moveHandler = 0;

static uintptr_t FindMoveHandler() {
    const auto& o = GetOffsets();
    if (!o.moveHandlerVtable) return 0;
    uintptr_t vt = mem::ModuleBase() + o.moveHandlerVtable;
    if (g_moveHandler && IsLiveQObject(g_moveHandler, vt)) return g_moveHandler;
    g_moveHandler = 0;
    auto hits = mem::FindByVtable(vt, 32);
    int valid = 0;
    for (uintptr_t h : hits) {
        if (LooksLikeStack(h) || !IsLiveQObject(h, vt)) continue;
        ++valid;
        if (!g_moveHandler) g_moveHandler = h;
    }
    Log("MovementHandler: candidatos=%zu validos=%d elegido=0x%llx", hits.size(), valid,
        (unsigned long long)g_moveHandler);
    return g_moveHandler;
}

using NewFn = void* (*)(size_t);
using DispatchFn = void (*)(void* handler, void** sharedPtrLike);

static bool CallDispatch(DispatchFn fn, void* handler, void** sp) {
    __try {
        fn(handler, sp);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool Turn(int dir) {
    static bool qtOk = qt::Init();
    if (!qtOk || dir < 0 || dir > 3) return false;
    const Offsets o = GetOffsets();
    if (!o.moveDispatchFn || !o.intentActionVtable || !o.intentRotateVtable || !o.intentRotateCtrl ||
        !o.actionCtrl || !o.opNew)
        return false;
    uintptr_t h = FindMoveHandler();
    if (!h) return false;
    const uintptr_t base = mem::ModuleBase();
    return qt::PostToObjectThread((void*)h, [=]() {
        NewFn alloc = (NewFn)(base + o.opNew);
        // intencion de giro: ctrl(0x10) + objeto(0x10)
        auto* rot = (uint8_t*)alloc(0x20);
        // accion: ctrl(0x10) + objeto(0x18)
        auto* act = (uint8_t*)alloc(0x28);
        if (!rot || !act) return;
        memset(rot, 0, 0x20);
        memset(act, 0, 0x28);
        *(uint64_t*)(rot + 0x00) = base + o.intentRotateCtrl;
        *(uint32_t*)(rot + 0x08) = 1;   // uses
        *(uint32_t*)(rot + 0x0C) = 1;   // weaks
        *(uint64_t*)(rot + 0x10) = base + o.intentRotateVtable;
        *(uint8_t*)(rot + 0x18) = (uint8_t)dir;
        *(uint8_t*)(rot + 0x19) = 1;
        *(uint64_t*)(act + 0x00) = base + o.actionCtrl;
        *(uint32_t*)(act + 0x08) = 1;
        *(uint32_t*)(act + 0x0C) = 1;
        *(uint64_t*)(act + 0x10) = base + o.intentActionVtable;
        *(uint64_t*)(act + 0x18) = (uint64_t)(rot + 0x10);   // shared_ptr<intencion>.ptr
        *(uint64_t*)(act + 0x20) = (uint64_t)rot;            // shared_ptr<intencion>.ctrl
        void* sp[2] = {act + 0x10, act};
        if (!CallDispatch((DispatchFn)(base + o.moveDispatchFn), (void*)h, sp))
            Log("Turn(%d): excepcion al despachar", dir);
        ReleaseShared(act);   // nuestra referencia; el cliente conserva las suyas si encola la accion
    });
}

bool Probe() {
    bool chat = FindChat() != 0;
    FindUseHandler();     // solo para dejar el diagnostico en el log
    FindAttackHandler();
    FindMoveHandler();
    return chat;
}

bool Say(const std::string& text) {
    static bool qtOk = qt::Init();
    if (!qtOk) return false;
    const auto& o = GetOffsets();
    if (!o.chatMetacall) return false;
    uintptr_t chat = FindChat();
    if (!chat) return false;

    MetaCallFn mc = (MetaCallFn)(mem::ModuleBase() + o.chatMetacall);
    int idx = o.chatSayIndex;
    int vol = o.chatSayVolume;
    std::string t = text;
    return qt::PostToObjectThread((void*)chat, [=]() {
        qt::QStr q = qt::FromUtf8(t);
        int v = vol;
        void* argv[3] = {nullptr, &q, &v};
        mc((void*)chat, /*QMetaObject::InvokeMetaMethod*/ 0, idx, argv);
        qt::Free(q);
    });
}
}
