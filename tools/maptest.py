"""Recorre el std::map de criaturas del TCreatureStorage.  Uso: python maptest.py [storage_hex]"""
import ctypes, struct, sys
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)

def u64(a):
    r = scan.read(h, a, 8)
    return struct.unpack("<Q", r)[0] if r else 0
def u32(a):
    r = scan.read(h, a, 4)
    return struct.unpack("<I", r)[0] if r else 0

if len(sys.argv) > 1:
    st = int(sys.argv[1], 16)
else:
    vt = base + 0x18256e8
    st = 0
    t = struct.pack("<Q", vt)
    for (a, sz) in scan.regions(h):
        pos = 0
        while pos < sz and not st:
            buf = scan.read(h, a + pos, min(1 << 20, sz - pos))
            if not buf: break
            i = buf.find(t)
            while i != -1:
                o = a + pos + i
                if i % 8 == 0 and u32(o + 0x20) >> 28 == 1:   # id de jugador
                    st = o; break
                i = buf.find(t, i + 1)
            pos += 1 << 20
print("storage = 0x%x  self=0x%x  size=%d" % (st, u32(st + 0x20), u64(st + 0x18)))
head = u64(st + 0x10)
print("head = 0x%x  left=%x parent=%x right=%x" % (head, u64(head), u64(head + 8), u64(head + 0x10)))
root = u64(head + 8)
out = []
def walk(n, depth=0):
    if len(out) > 400 or depth > 64: return
    if not n: return
    isnil = scan.read(h, n + 0x19, 1)
    if not isnil or isnil[0]: return
    walk(u64(n), depth + 1)
    out.append((u32(n + 0x20), u64(n + 0x28), u64(n + 0x30)))
    walk(u64(n + 0x10), depth + 1)
walk(root)
print("nodos recorridos:", len(out))
for k, p, c in out[:60]:
    nm = ""
    sz = u64(p + 0x38)
    if 0 < sz < 40:
        r = scan.read(h, u64(p + 0x30), sz * 2)
        if r: nm = r.decode("utf-16le", "replace")
    print("  id=0x%x ptr=0x%x ctrl=0x%x x=%d y=%d z=%d hp=%d name=%r" % (
        k, p, c, u32(p + 0x60), u32(p + 0x64), u32(p + 0x68), (scan.read(h, p + 0xe8, 1) or b"\0")[0], nm))
