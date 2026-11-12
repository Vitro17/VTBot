"""
VTBot memory scanner (estilo Cheat Engine) para localizar offsets en client.exe.

Uso:
  python scan.py first <valor>          # primer escaneo de uint32 (alineado a 4)
  python scan.py next  <valor>          # filtra las direcciones que ahora valen <valor>
  python scan.py show [n]               # muestra n candidatos (con RVA si estan en client.exe)
  python scan.py read <addr_hex> [n]    # vuelca n uint32 desde addr
  python scan.py ptr  <addr_hex>        # busca punteros (qword) que apunten a addr (o cerca)
  python scan.py reset
Opciones: --pid N (por defecto toma el primer client.exe)
"""
import ctypes, ctypes.wintypes as wt, sys, os, struct, subprocess
import numpy as np

K32 = ctypes.WinDLL("kernel32", use_last_error=True)
PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
PROCESS_ALL = 0x0400 | 0x0010 | 0x0008  # QUERY_INFO | VM_READ | VM_OPERATION
STATE = os.path.join(os.path.dirname(__file__), ".scan_state.npy")

K32.OpenProcess.restype = wt.HANDLE
K32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.POINTER(ctypes.c_size_t)]


class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wt.DWORD), ("PartitionId", wt.WORD),
                ("RegionSize", ctypes.c_size_t), ("State", wt.DWORD),
                ("Protect", wt.DWORD), ("Type", wt.DWORD)]


def find_pid():
    if "--pid" in sys.argv:
        return int(sys.argv[sys.argv.index("--pid") + 1])
    out = subprocess.check_output(["tasklist", "/fi", "imagename eq client.exe", "/fo", "csv", "/nh"], text=True)
    pids = [int(l.split('","')[1]) for l in out.strip().splitlines() if "client.exe" in l]
    if not pids:
        sys.exit("No hay client.exe")
    return pids[0]


def regions(h):
    addr = 0
    mbi = MBI()
    while K32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        base = mbi.BaseAddress or 0
        if mbi.State == 0x1000 and not (mbi.Protect & 0x101) and (mbi.Protect & 0xCC):  # COMMIT, readable/writable
            yield base, mbi.RegionSize
        addr = base + mbi.RegionSize
        if addr >= 0x7FFFFFFFFFFF:
            break


def read(h, addr, size):
    buf = (ctypes.c_char * size)()
    got = ctypes.c_size_t()
    if not K32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size, ctypes.byref(got)):
        return None
    return bytes(buf[:got.value])


def client_module(h):
    mods = (ctypes.c_void_p * 1024)()
    need = wt.DWORD()
    PSAPI.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 3)
    base = mods[0]
    return base


def module_size(h, base):
    class MI(ctypes.Structure):
        _fields_ = [("lpBaseOfDll", ctypes.c_void_p), ("SizeOfImage", wt.DWORD), ("EntryPoint", ctypes.c_void_p)]
    mi = MI()
    PSAPI.GetModuleInformation(h, ctypes.c_void_p(base), ctypes.byref(mi), ctypes.sizeof(mi))
    return mi.SizeOfImage


def first_scan(h, value):
    hits = []
    for base, size in regions(h):
        data = read(h, base, size)
        if not data:
            continue
        n = len(data) // 4
        arr = np.frombuffer(data, dtype=np.uint32, count=n)
        idx = np.nonzero(arr == value)[0]
        if idx.size:
            hits.append(base + idx.astype(np.uint64) * 4)
    res = np.concatenate(hits) if hits else np.array([], dtype=np.uint64)
    np.save(STATE, res)
    print(f"{len(res)} candidatos")


def next_scan(h, value):
    cur = np.load(STATE)
    keep = []
    for a in cur:
        d = read(h, int(a), 4)
        if d and struct.unpack("<I", d)[0] == value:
            keep.append(a)
    res = np.array(keep, dtype=np.uint64)
    np.save(STATE, res)
    print(f"{len(res)} candidatos")


def show(h, n):
    cur = np.load(STATE)
    base = client_module(h)
    size = module_size(h, base)
    print(f"client.exe base=0x{base:x} size=0x{size:x}")
    for a in cur[:n]:
        a = int(a)
        tag = f"client.exe+0x{a - base:x}" if base <= a < base + size else "heap"
        print(f"0x{a:x}  {tag}")


def main():
    cmd = sys.argv[1]
    if cmd == "reset":
        if os.path.exists(STATE):
            os.remove(STATE)
        return
    pid = find_pid()
    h = K32.OpenProcess(PROCESS_ALL, False, pid)
    if not h:
        sys.exit(f"OpenProcess fallo ({ctypes.get_last_error()}). Prueba como administrador.")
    print(f"PID {pid}")
    args = [a for a in sys.argv[2:] if not a.startswith("--") and not a.isdigit() or a.isdigit()]
    if cmd == "first":
        first_scan(h, int(sys.argv[2]))
    elif cmd == "next":
        next_scan(h, int(sys.argv[2]))
    elif cmd == "seq":
        # python scan.py seq 24040 24040 7995 7995  -> busca esa secuencia consecutiva de uint32
        vals = np.array([int(v) for v in sys.argv[2:] if v.isdigit()], dtype=np.uint32)
        k = len(vals)
        base = client_module(h)
        for rb, size in regions(h):
            data = read(h, rb, size)
            if not data or len(data) < 4 * k:
                continue
            arr = np.frombuffer(data, dtype=np.uint32, count=len(data) // 4)
            m = arr[: len(arr) - k + 1] == vals[0]
            for j in range(1, k):
                m &= arr[j: len(arr) - k + 1 + j] == vals[j]
            for i in np.nonzero(m)[0]:
                print(f"0x{rb + int(i) * 4:x}")
    elif cmd == "show":
        show(h, int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 40)
    elif cmd == "read":
        addr = int(sys.argv[2], 16)
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 16
        d = read(h, addr, n * 4)
        for i in range(n):
            print(f"+0x{i*4:03x}: {struct.unpack_from('<I', d, i*4)[0]}")
    elif cmd == "ptr":
        target = int(sys.argv[2], 16)
        for base, size in regions(h):
            data = read(h, base, size)
            if not data:
                continue
            arr = np.frombuffer(data, dtype=np.uint64, count=len(data) // 8)
            idx = np.nonzero((arr <= target) & (arr > target - 0x400))[0]
            for i in idx[:20]:
                print(f"0x{base + int(i)*8:x} -> 0x{int(arr[i]):x} (+0x{target-int(arr[i]):x})")


if __name__ == "__main__":
    main()
