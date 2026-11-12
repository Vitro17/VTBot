"""Vigila varios objetos por vtable y registra los cambios de dwords con marca de tiempo.
Uso: python -u watchmulti.py <segundos> <size_hex> nombre=rvaHex [...]"""
import ctypes, struct, sys, time
import scan
K = ctypes.windll.kernel32
h = K.OpenProcess(0x1FFFFF, False, scan.find_pid())
base = scan.client_module(h)
secs = float(sys.argv[1]); size = int(sys.argv[2], 16)
specs = [a.split("=") for a in sys.argv[3:]]
def live(o, vt):
    r = scan.read(h, o, 16)
    if not r: return False
    v, d = struct.unpack("<QQ", r)
    if v != vt or not d or d & 7: return False
    r2 = scan.read(h, d + 8, 8)
    return bool(r2) and struct.unpack("<Q", r2)[0] == o
def find(vt):
    t = struct.pack("<Q", vt); out = []
    for (a, sz) in scan.regions(h):
        pos = 0
        while pos < sz:
            buf = scan.read(h, a+pos, min(1<<20, sz-pos))
            if not buf: break
            i = buf.find(t)
            while i != -1:
                if i % 8 == 0: out.append(a+pos+i)
                i = buf.find(t, i+1)
            pos += 1<<20
    return out
objs = {}
for name, rva in specs:
    vt = base + int(rva, 16)
    c = [o for o in find(vt) if live(o, vt)]
    if c:
        objs[name] = c[0]
        print("objeto", name, hex(c[0]), "(candidatos vivos: %d)" % len(c), flush=True)
    else:
        print("sin objeto vivo:", name, flush=True)
prev = {n: scan.read(h, o, size) for n, o in objs.items()}
t0 = time.time()
print("VIGILANDO %.0fs ... ahora lanza haste y espera a que expire" % secs, flush=True)
while time.time() - t0 < secs:
    time.sleep(0.15)
    for n, o in objs.items():
        cur = scan.read(h, o, size)
        if not cur or not prev[n]: continue
        for off in range(0, size - 3, 4):
            a = struct.unpack_from("<I", prev[n], off)[0]; b = struct.unpack_from("<I", cur, off)[0]
            if a != b:
                print("[%6.1fs] %-10s +0x%03x: %08x -> %08x" % (time.time()-t0, n, off, a, b), flush=True)
        prev[n] = cur
print("fin", flush=True)
