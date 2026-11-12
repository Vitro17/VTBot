#include "qt.h"
#include "log.h"
#include <windows.h>
#include <atomic>

namespace vt::qt {

// QSlotObjectBase (Qt6): { QAtomicInt m_ref; ImplFn m_impl; } -- sin vtable.
struct SlotObj {
    std::atomic<int> ref{1};
    void (*impl)(int, SlotObj*, void*, void**, bool*);
    std::function<void()> fn;
};
static void SlotImpl(int which, SlotObj* self, void*, void**, bool*) {
    if (which == 0)       delete self;          // Destroy
    else if (which == 1)  self->fn();           // Call
}

using InvokeFn = bool (*)(void* obj, void* slotObj, int connType, void* ret);
using FromUtf8Fn = void (*)(QStr* ret, const char* s, long long n);
using DeallocFn = void (*)(void* d, long long objSize, long long align);

using ActivateFn = void (*)(void* sender, const void* mo, int idx, void** argv);
static ActivateFn g_activate = nullptr;

bool Activate(void* sender, const void* metaObject, int localIndex, void** argv) {
    if (!g_activate || !sender || !metaObject) return false;
    g_activate(sender, metaObject, localIndex, argv);
    return true;
}

static InvokeFn   g_invoke = nullptr;
static FromUtf8Fn g_fromUtf8 = nullptr;
static DeallocFn  g_dealloc = nullptr;

bool Init() {
    HMODULE m = GetModuleHandleW(L"Qt6Core.dll");
    if (!m) { Log("qt: Qt6Core.dll no cargada"); return false; }
    // invokeMethodImpl(QObject*, QSlotObjectBase*, Qt::ConnectionType, void* ret)
    g_invoke = (InvokeFn)GetProcAddress(
        m, "?invokeMethodImpl@QMetaObject@@CA_NPEAVQObject@@PEAVQSlotObjectBase@QtPrivate@@W4ConnectionType@Qt@@PEAX@Z");
    g_fromUtf8 = (FromUtf8Fn)GetProcAddress(m, "?fromUtf8@QString@@SA?AV1@PEBD_J@Z");
    g_dealloc = (DeallocFn)GetProcAddress(m, "?deallocate@QArrayData@@SAXPEAU1@_J1@Z");
    g_activate = (ActivateFn)GetProcAddress(m, "?activate@QMetaObject@@SAXPEAVQObject@@PEBU1@HPEAPEAX@Z");
    Log("qt: invoke=%p fromUtf8=%p dealloc=%p activate=%p", (void*)g_invoke, (void*)g_fromUtf8,
        (void*)g_dealloc, (void*)g_activate);
    return g_invoke && g_fromUtf8 && g_dealloc && g_activate;
}

bool PostToObjectThread(void* qobject, std::function<void()> fn) {
    if (!g_invoke || !qobject) return false;
    auto* so = new SlotObj();
    so->impl = SlotImpl;
    so->fn = std::move(fn);
    constexpr int kQueued = 2;   // Qt::QueuedConnection
    bool ok = g_invoke(qobject, so, kQueued, nullptr);
    if (!ok) delete so;
    return ok;
}

QStr FromUtf8(const std::string& s) {
    QStr r;
    if (g_fromUtf8) g_fromUtf8(&r, s.c_str(), (long long)s.size());
    return r;
}

void Free(QStr& s) {
    if (!s.d) return;
    auto* ref = (std::atomic<int>*)s.d;       // QArrayData::ref_
    int cur = ref->load();
    if (cur >= 0 && ref->fetch_sub(1) == 1) g_dealloc(s.d, 2, 8);
    s = QStr{};
}
}
