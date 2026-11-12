"""Vuelca los nodos del std::map de cooldowns (TCooldownStorage+0x20, tamano en +0x28) cuando cambia.
Uso: python -u cdwatch.py <obj_hex> <segundos>"""
import ctypes, struct, sys, time
import scan
K = ctypes.windll.kernel32
K.GetTickCount64.restype = ctypes.c_ulonglong
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
obj = int(sys.argv[1], 16); secs = float(sys.argv[2])

def u64(a):
    r = scan.read(h, a, 8)
    return struct.unpack("<Q", r)[0] if r else 0

def nodes():
    head = u64(obj + 0x20)
    root = u64(head + 8)
    out, stack, seen = [], [root], set()
    while stack and len(seen) < 64:
        n = stack.pop()
        if not n or n in seen: continue
        seen.add(n)
        nil = scan.read(h, n + 0x19, 1)
        if not nil or nil[0]: continue
        stack += [u64(n), u64(n + 0x10)]
        out.append((n, scan.read(h, n + 0x20, 0x50)))
    return out

print("armado, vigilando %.0fs" % secs, flush=True)
last = -1; t0 = time.time(); redump = 0
while time.time() - t0 < secs:
    size = u64(obj + 0x28)
    now = time.time()
    if size != last or (redump and now >= redump):
        redump = 0 if size != last else 0
        print("--- %.2fs size=%d  tick64=%d perf_ns=%d" % (now - t0, size, K.GetTickCount64(), time.perf_counter_ns()), flush=True)
        for n, raw in nodes():
            q = struct.unpack("<%dQ" % (len(raw) // 8), raw)
            print("   node 0x%x: %s" % (n, " ".join("%x" % v for v in q)), flush=True)
        if size != last and size > 0: redump = now + 0.4
        last = size
    time.sleep(0.02)
print("fin", flush=True)
