"""
Breakpoints de EJECUCION por hardware (hasta 4) en client.exe.  Solo toca DR0-3/DR7/EFlags.
Uso: python xb.py <segundos> <rva_hex> [<rva_hex> ...]
Resultado en tools/xb_result.txt: por cada direccion: nº de hits y detalle de los primeros.
"""
import ctypes, ctypes.wintypes as wt, sys, time, os, struct, collections
a = sys.argv[1:]
sys.argv = ["x"]
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

REG = next((x.split("=")[1] for x in a if x.startswith("--reg=")), None)
DUMP_ARGV = ("--argv" in a) or REG is not None   # volcar memoria apuntada (argv[1] o un registro)
a = [x for x in a if x != "--argv" and not x.startswith("--reg=")]
seconds = float(a[0])
rvas = [int(x, 16) for x in a[1:5]]

CTX_DEBUG = 0x100010
CTX_CD = 0x100011      # CONTROL | DEBUG
CTX_READ = 0x100013    # CONTROL | INTEGER | DEBUG
CTX_SIZE = 1232
OFF = dict(flags=0x30, eflags=0x44, dr0=0x48, dr1=0x50, dr2=0x58, dr3=0x60, dr6=0x68, dr7=0x70,
           rax=0x78, rcx=0x80, rdx=0x88, rbx=0x90, rsp=0x98, rbp=0xA0, rsi=0xA8, rdi=0xB0,
           r8=0xB8, r9=0xC0, r10=0xC8, r11=0xD0, r12=0xD8, r13=0xE0, r14=0xE8, r15=0xF0, rip=0xF8)
U64 = ctypes.c_ulonglong
U32 = ctypes.c_uint32


def new_ctx():
    raw = ctypes.create_string_buffer(CTX_SIZE + 16)
    return raw, (ctypes.addressof(raw) + 15) & ~15


def g(addr, n, w=U64):
    return w.from_address(addr + OFF[n]).value


def s(addr, n, v):
    U64.from_address(addr + OFF[n]).value = v


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


def arm(tid, addrs):
    """addrs: lista (hasta 4) de direcciones absolutas activas; [] = desarmar"""
    th = K.OpenThread(0x1FFFFF, False, tid)
    if not th:
        return
    K.SuspendThread(th)
    raw, c = new_ctx()
    s(c, "flags", CTX_DEBUG)
    if K.GetThreadContext(th, ctypes.c_void_p(c)):
        dr7 = 0
        for i, ad in enumerate(addrs):
            s(c, f"dr{i}", ad)
            dr7 |= 1 << (2 * i)          # Li=1, RWi=00 (ejecucion), LENi=00
        s(c, "dr7", dr7)
        s(c, "dr6", 0)
        s(c, "flags", CTX_DEBUG)
        K.SetThreadContext(th, ctypes.c_void_p(c))
    K.ResumeThread(th)
    K.CloseHandle(th)


pid = scan.find_pid()
hproc = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(hproc)
msize = scan.module_size(hproc, base)
addrs = [base + r for r in rvas]
print(f"PID {pid} base 0x{base:x}; BPs: {[hex(r) for r in rvas]}")

K.DebugSetProcessKillOnExit(False)
if not K.DebugActiveProcess(pid):
    sys.exit(f"DebugActiveProcess fallo: {ctypes.get_last_error()}")

active = list(addrs)
counts = collections.Counter()
details = collections.defaultdict(list)
buf = ctypes.create_string_buffer(256)


def is_ret(q):
    """q parece una direccion de retorno si justo antes hay un call"""
    if not (base <= q < base + msize):
        return False
    b = scan.read(hproc, q - 7, 7) or b""
    return len(b) == 7 and (b[2] == 0xE8 or b[1:3] in (b"\xff\x15",) or b[5] == 0xFF or b[4] == 0xFF or b[3] == 0xFF
                            or b[6] == 0xFF or b[0:2] == b"\xff\x15")


try:
    for t in thread_ids(pid):
        arm(t, active)
    print(f"armado en {len(thread_ids(pid))} hilos. {seconds}s ... ahora lanza el hechizo/accion")
    end = time.time() + seconds
    while time.time() < end:
        if not K.WaitForDebugEvent(buf, 200):
            continue
        code = U32.from_buffer(buf, 0).value
        dpid = U32.from_buffer(buf, 4).value
        dtid = U32.from_buffer(buf, 8).value
        status = 0x80010001
        if code == 1:
            exc = U32.from_buffer(buf, 16).value
            if exc == 0x80000004:
                th = K.OpenThread(0x1FFFFF, False, dtid)
                raw, c = new_ctx()
                s(c, "flags", CTX_READ)
                if K.GetThreadContext(th, ctypes.c_void_p(c)):
                    rip = g(c, "rip")
                    counts[rip] += 1
                    if len(details[rip]) < int(os.environ.get("XB_MAX", "6")):
                        rsp = g(c, "rsp")
                        st = scan.read(hproc, rsp, 0x400) or b""
                        rets = []
                        for i in range(0, len(st) - 7, 8):
                            q = struct.unpack_from("<Q", st, i)[0]
                            if is_ret(q):
                                rets.append(q - base)
                            if len(rets) >= 10:
                                break
                        d = dict(tid=dtid, rcx=g(c, "rcx"), rdx=g(c, "rdx"), r8=g(c, "r8"),
                                 r9=g(c, "r9"), rets=rets, obj=None)
                        # qt_static_metacall(obj, call=edx, id=r8d, argv=r9): si call==0 (InvokeMetaMethod)
                        if DUMP_ARGV:
                            # QMetaObject::activate(sender, mo, idx, argv): argv[1] = puntero al argumento.
                            # Volcamos lo apuntado (objeto directo, p.ej. mensaje protobuf) y, si parece
                            # un shared_ptr {obj, ctrl}, tambien el objeto apuntado.
                            try:
                                if REG:     # --reg=r8 : volcar directamente la memoria apuntada por ese registro
                                    q1 = d[REG]
                                else:
                                    argv = d["r9"]
                                    q1 = struct.unpack("<Q", scan.read(hproc, argv + 8, 8))[0]
                                raw1 = scan.read(hproc, q1, 0xC0) or b""
                                p0 = struct.unpack("<Q", raw1[:8])[0] if raw1 else 0
                                raw2 = b""
                                if p0 and not (base <= p0 < base + msize):
                                    raw2 = scan.read(hproc, p0, 0x80) or b""
                                nested = []
                                for ii in range(0, min(len(raw2), 0x60), 8):
                                    qq = struct.unpack_from("<Q", raw2, ii)[0]
                                    if 0x10000 < qq < 0x7fff00000000 and not (base <= qq < base + msize):
                                        rr = scan.read(hproc, qq, 0x60) or b""
                                        if rr:
                                            nested.append((ii, qq, rr))
                                d["nested"] = nested
                                d["obj"] = (q1, raw1, p0, raw2)
                            except Exception as e:
                                d["obj"] = (0, b"", 0, b"")
                        _ov = os.environ.get("XB_ONLYVT")
                        if _ov and not (d.get("obj") and len(d["obj"][3]) >= 8 and
                                        struct.unpack_from("<Q", d["obj"][3], 0)[0] in [base + int(x, 16) for x in _ov.split(",")]):
                            pass
                        else:
                            details[rip].append(d)
                    if counts[rip] == int(os.environ.get("XB_LIMIT", "300")) and rip in active:      # demasiado caliente: desarmar esa direccion
                        active.remove(rip)
                        for t in thread_ids(pid):
                            arm(t, active)
                        print(f"  (desarmado 0x{rip-base:x}: demasiados hits)")
                    s(c, "dr6", 0)
                    s(c, "eflags", g(c, "eflags") | 0x10000)     # RF: ejecutar la instruccion sin re-disparar
                    s(c, "flags", CTX_CD)
                    K.SetThreadContext(th, ctypes.c_void_p(c))
                K.CloseHandle(th)
                status = 0x00010002
            elif exc == 0x80000003:
                status = 0x00010002
        elif code == 2:
            arm(dtid, active)
        K.ContinueDebugEvent(dpid, dtid, status)
except KeyboardInterrupt:
    pass
finally:
    for t in thread_ids(pid):
        arm(t, [])
    K.DebugActiveProcessStop(pid)
    print("depurador desadjuntado")

with open(os.path.join(os.path.dirname(__file__), "xb_result.txt"), "w") as f:
    for rip, n in counts.items():
        f.write(f"\nclient+0x{rip-base:x}: {n} hits\n")
        for d in details[rip]:
            f.write(f"  tid={d['tid']} rcx=0x{d['rcx']:x} rdx=0x{d['rdx']:x} r8=0x{d['r8']:x} r9=0x{d['r9']:x}\n")
            f.write("    ret: " + " ".join(f"+0x{r:x}" for r in d["rets"]) + "\n")
            f.write(f"    call=edx={d['rdx'] & 0xFFFFFFFF} id=r8d={d['r8'] & 0xFFFFFFFF}\n")
            if os.environ.get("XB_COMPACT") and d.get("obj") and len(d["obj"][3]) >= 8:
                v0 = struct.unpack_from("<Q", d["obj"][3], 0)[0]
                f.write(f"    OBJVT={'client+0x%x' % (v0 - base) if base <= v0 < base + msize else hex(v0)}\n")
                continue
            if d.get("obj") and d["obj"][1]:
                q1, raw1, p0, raw2 = d["obj"]
                for title, addr_, raw in ((f"argv[1] -> 0x{q1:x} (objeto directo / shared_ptr)", q1, raw1),
                                          (f"  deref del 1er qword -> 0x{p0:x}", p0, raw2)):
                    if not raw:
                        continue
                    f.write(f"    {title}\n")
                    for i in range(0, len(raw), 8):
                        q = struct.unpack_from("<Q", raw, i)[0]
                        t = f" client+0x{q-base:x}" if base <= q < base + msize else ""
                        f.write(f"      +0x{i:02x}: 0x{q:016x}{t}  (lo32={q & 0xFFFFFFFF} hi32={q >> 32})\n")
            for (ii, qq, rr) in d.get("nested", []):
                f.write(f"    nested de +0x{ii:02x} -> 0x{qq:x}\n")
                for i in range(0, len(rr), 8):
                    q = struct.unpack_from("<Q", rr, i)[0]
                    t = f" client+0x{q-base:x}" if base <= q < base + msize else ""
                    f.write(f"      +0x{i:02x}: 0x{q:016x}{t}  (lo32={q & 0xFFFFFFFF} hi32={q >> 32})\n")
    if not counts:
        f.write("sin hits\n")
for rip, n in counts.items():
    print(f"client+0x{rip-base:x}: {n} hits")
print("fin")
