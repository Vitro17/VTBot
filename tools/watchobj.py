"""Vigila un objeto y los bloques a los que apunta (1 nivel); imprime solo los cambios.
Uso: python -u watchobj.py <addr_hex> <segundos> [size_hex=0x100] [blk_size_hex=0x80]"""
import ctypes, struct, sys, time
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
obj = int(sys.argv[1], 16); secs = float(sys.argv[2])
size = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x100
bsz = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x80

def snap():
    raw = scan.read(h, obj, size)
    if not raw: return None, {}
    q = struct.unpack("<%dQ" % (size // 8), raw)
    blocks = {}
    for i, v in enumerate(q):
        if i >= 2 and 0x10000 < v < 0x7fffffffffff and not (base <= v < base + 0x2000000):
            b = scan.read(h, v, bsz)
            if b: blocks[i * 8] = (v, b)
    return raw, blocks

prev_raw, prev_blocks = snap()
print("vigilando %.0fs, obj=0x%x, bloques=%s" % (secs, obj, [hex(k) for k in prev_blocks]), flush=True)
t0 = time.time(); lines = 0
while time.time() - t0 < secs and lines < 600:
    raw, blocks = snap()
    if raw is None: break
    t = time.time() - t0
    for off in range(0, size, 4):
        a = struct.unpack_from("<I", prev_raw, off)[0]; b = struct.unpack_from("<I", raw, off)[0]
        if a != b and off >= 0x18:
            print("%6.2fs obj+0x%x: %d -> %d (0x%x -> 0x%x)" % (t, off, a, b, a, b), flush=True); lines += 1
    for k, (addr, b) in blocks.items():
        pb = prev_blocks.get(k)
        if not pb or pb[0] != addr:
            continue
        for off in range(0, bsz, 4):
            x = struct.unpack_from("<I", pb[1], off)[0]; y = struct.unpack_from("<I", b, off)[0]
            if x != y:
                print("%6.2fs blk@obj+0x%x(+0x%x): %d -> %d (0x%x -> 0x%x)" % (t, k, off, x, y, x, y), flush=True); lines += 1
    prev_raw, prev_blocks = raw, blocks
    time.sleep(0.05)
print("fin", flush=True)
