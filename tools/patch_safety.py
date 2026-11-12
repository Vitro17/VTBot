import re
base = r"c:\Users\SuperUsuario\Documents\Test\VTBot\src\dll\\"

def patch(name, fn):
    p = base + name
    s = open(p, encoding="utf-8").read()
    s2 = fn(s)
    assert s2 != s, name
    open(p, "w", encoding="utf-8").write(s2)

# ---- creatures: validar por conteo de nodos + exponer StorageAddr ----
def cr(s):
    s = s.replace('''static bool StorageSane(uintptr_t st, uintptr_t vt) {
    if (!mem::IsLiveQObject(st, vt)) return false;
    uint32_t self = mem::Read<uint32_t>(st + kSelfId);
    uint64_t size = mem::Read<uint64_t>(st + kMapSize);
    return (self >> 28) == 1 && size > 0 && size < 100000;
}''', '''// Cuenta los nodos alcanzables del std::map; un almacen liberado/corrupto no da el mismo numero que su tamano.
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
}''')
    s = s.replace('''static std::string ReadName(''', '''uintptr_t StorageAddr() { return FindStorage(); }

static std::string ReadName(''')
    return s
patch("creatures.cpp", cr)

def crh(s):
    return s.replace("bool Snapshot(View& out);", "bool Snapshot(View& out);\n\n// Direccion del almacen vigente (0 si no se encuentra). Sirve para validar objetos que dependen de el.\nuintptr_t StorageAddr();")
patch("creatures.h", crh)

# ---- actions: verificaciones cruzadas ----
def ac(s):
    s = s.replace('#include "log.h"', '#include "log.h"\n#include "creatures.h"', 1)
    # manejador de ataque: +0x10 debe ser el almacen vigente
    s = s.replace('''    if (g_atkHandler && IsLiveQObject(g_atkHandler, vt)) return g_atkHandler;
    g_atkHandler = 0;
    auto hits = mem::FindByVtable(vt, 16);
    for (uintptr_t h : hits) {
        if (LooksLikeStack(h) || !IsLiveQObject(h, vt)) continue;
        g_atkHandler = h;
        break;
    }''', '''    uintptr_t storage = creatures::StorageAddr();
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
    }''')
    # manejador de pociones: la fabrica debe tener la vtable esperada
    s = s.replace('''    if (g_useHandler && IsLiveQObject(g_useHandler, vt)) return g_useHandler;
    g_useHandler = 0;
    auto hits = mem::FindByVtable(vt, 32);
    int valid = 0;
    for (uintptr_t h : hits) {
        if (LooksLikeStack(h) || !IsLiveQObject(h, vt)) continue;''', '''    auto ok = [&](uintptr_t h) {   // vivo y con fabrica de la clase esperada (si se conoce su vtable)
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
        if (LooksLikeStack(h) || !ok(h)) continue;''')
    return s
patch("actions.cpp", ac)

def oh(s):
    return s.replace("    uintptr_t useFactoryFn = 0;", "    uintptr_t useFactoryVtable = 0;      // RVA vtable de la fabrica (validacion del manejador)\n    uintptr_t useFactoryFn = 0;")
patch("offsets.h", oh)

def oc(s):
    return s.replace('    g_off.useFactoryFn     =', '    g_off.useFactoryVtable = (uintptr_t)Get(ini.c_str(), L"useFactoryVtable", 0);\n    g_off.useFactoryFn     =')
patch("offsets.cpp", oc)
print("ok")
