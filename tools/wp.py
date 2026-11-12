"""
Hardware write-watchpoint: averigua que instrucciones de client.exe escriben en una direccion.
Uso: python wp.py <addr_hex> [segundos=60] [len=4]
Resultado en tools/wp_result.txt. Al terminar (o Ctrl+C) desadjunta el depurador sin matar el cliente.
"""
import ctypes, ctypes.wintypes as wt, sys, time, os, collections
sys.argv_b = sys.argv
import scan

K = scan.K32
K.OpenThread.restype = wt.HANDLE
K.CreateToolhelp32Snapshot.restype = wt.HANDLE
K.CloseHandle.argtypes = [wt.HANDLE]
K.Thread32First.argtypes = [wt.HANDLE, ctypes.c_void_p]
K.Thread32Next.argtypes = [wt.HANDLE, ctypes.c_void_p]
K.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
K.SetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
K.SuspendThread.argtypes = [wt.HANDLE]
K.ResumeThread.argtypes = [wt.HANDLE]
K.WaitForDebugEvent.argtypes = [ctypes.c_void_p, wt.DWORD]
K.ContinueDebugEvent.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD]

addr = int(sys.argv[1], 16)
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60
length = int(sys.argv[3]) if len(sys.argv) > 3 else 4

CTX_SIZE = 1232
CTX_DEBUG = 0x100010          # solo registros de depuracion (NO tocar estado FPU/SSE)
CTX_READ = 0x100013           # CONTROL | INTEGER | DEBUG
CTX_ALL = CTX_READ
OFF = dict(flags=0x30, dr0=0x48, dr6=0x68, dr7=0x70, rax=0x78, rcx=0x80, rdx=0x88, rbx=0x90, rsp=0x98,
           rbp=0xA0, rsi=0xA8, rdi=0xB0, r8=0xB8, r9=0xC0, r10=0xC8, r11=0xD0, r12=0xD8, r13=0xE0,
           r14=0xE8, r15=0xF0, rip=0xF8)
ULL = ctypes.c_ulonglong


def new_ctx():
    raw = ctypes.create_string_buffer(CTX_SIZE + 16)
    a = ctypes.addressof(raw)
    a = (a + 15) & ~15
    return raw, a


def ctx_get(a, name):
    return ULL.from_address(a + OFF[name]).value


def ctx_set(a, name, v):
    ULL.from_address(a + OFF[name]).value = v


def thread_ids(pid):
    class TE(ctypes.Structure):
        _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                    ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
                    ("dwFlags", wt.DWORD)]
    snap = K.CreateToolhelp32Snapshot(4, 0)
    te = TE(); te.dwSize = ctypes.sizeof(te)
    out = []
    ok = K.Thread32First(snap, ctypes.byref(te))
    while ok:
        if te.th32OwnerProcessID == pid:
            out.append(te.th32ThreadID)
        ok = K.Thread32Next(snap, ctypes.byref(te))
    K.CloseHandle(snap)
    return out


def set_dr(tid, enable):
    th = K.OpenThread(0x1FFFFF, False, tid)
    if not th:
        return False
    K.SuspendThread(th)
    raw, a = new_ctx()
    ctx_set(a, "flags", CTX_DEBUG)
    if K.GetThreadContext(th, ctypes.c_void_p(a)):
        if enable:
            lenbits = {1: 0, 2: 1, 4: 3, 8: 2}[length]
            ctx_set(a, "dr0", addr)
            ctx_set(a, "dr6", 0)
            # L0=1, RW0=01 (write), LEN0
            ctx_set(a, "dr7", 1 | (1 << 16) | (lenbits << 18))
        else:
            ctx_set(a, "dr7", 0)
            ctx_set(a, "dr6", 0)
        ctx_set(a, "flags", CTX_DEBUG)
        K.SetThreadContext(th, ctypes.c_void_p(a))
    K.ResumeThread(th)
    K.CloseHandle(th)
    return True


pid = scan.find_pid()
hproc = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(hproc)
msize = scan.module_size(hproc, base)
print(f"PID {pid}, client base 0x{base:x}")

K.DebugSetProcessKillOnExit(False)
if not K.DebugActiveProcess(pid):
    sys.exit(f"DebugActiveProcess fallo: {ctypes.get_last_error()}")

hits = collections.OrderedDict()
buf = ctypes.create_string_buffer(256)
tids_armed = set()
try:
    # armar en todos los hilos existentes
    for t in thread_ids(pid):
        if set_dr(t, True):
            tids_armed.add(t)
    print(f"armado en {len(tids_armed)} hilos. Esperando escrituras {seconds}s ... (cambia tu HP ahora)")
    end = time.time() + seconds
    while time.time() < end:
        if not K.WaitForDebugEvent(buf, 200):
            continue
        code = ctypes.c_uint32.from_buffer(buf, 0).value
        dpid = ctypes.c_uint32.from_buffer(buf, 4).value
        dtid = ctypes.c_uint32.from_buffer(buf, 8).value
        status = 0x80010001  # DBG_EXCEPTION_NOT_HANDLED
        if code == 1:  # EXCEPTION_DEBUG_EVENT
            exc = ctypes.c_uint32.from_buffer(buf, 16).value
            if exc == 0x80000004:  # SINGLE_STEP -> hardware bp
                th = K.OpenThread(0x1FFFFF, False, dtid)
                raw, a = new_ctx()
                ctx_set(a, "flags", CTX_ALL)
                if K.GetThreadContext(th, ctypes.c_void_p(a)):
                    rip = ctx_get(a, "rip")
                    # leer 32 bytes antes del rip para ver la instruccion que escribio
                    before = scan.read(hproc, rip - 24, 24) or b""
                    after = scan.read(hproc, rip, 8) or b""
                    regs = {r: ctx_get(a, r) for r in ("rax", "rcx", "rdx", "rbx", "rsi", "rdi", "r8", "r9")}
                    key = rip
                    if key not in hits:
                        hits[key] = dict(count=0, before=before, after=after, regs=regs, tid=dtid)
                        print(f"HIT rip=0x{rip:x} (client+0x{rip-base:x})" if base <= rip < base + msize
                              else f"HIT rip=0x{rip:x} (fuera de client.exe)")
                    hits[key]["count"] += 1
                    ctx_set(a, "dr6", 0)
                    ctx_set(a, "flags", CTX_DEBUG)
                    K.SetThreadContext(th, ctypes.c_void_p(a))
                K.CloseHandle(th)
                status = 0x00010002  # DBG_CONTINUE
            elif exc == 0x80000003:  # breakpoint inicial del attach
                status = 0x00010002
        elif code == 2:  # CREATE_THREAD
            if set_dr(dtid, True):
                tids_armed.add(dtid)
        K.ContinueDebugEvent(dpid, dtid, status)
except KeyboardInterrupt:
    pass
finally:
    for t in thread_ids(pid):
        set_dr(t, False)
    K.DebugActiveProcessStop(pid)
    print("depurador desadjuntado")

with open(os.path.join(os.path.dirname(__file__), "wp_result.txt"), "w") as f:
    f.write(f"client base 0x{base:x}\n")
    for rip, h in hits.items():
        f.write(f"\nrip=0x{rip:x} client+0x{rip-base:x} count={h['count']} tid={h['tid']}\n")
        f.write("  bytes antes: " + h["before"].hex(" ") + "\n")
        f.write("  bytes desde: " + h["after"].hex(" ") + "\n")
        f.write("  regs: " + " ".join(f"{k}=0x{v:x}" for k, v in h["regs"].items()) + "\n")
print(f"{len(hits)} sitios distintos escriben en 0x{addr:x}")
