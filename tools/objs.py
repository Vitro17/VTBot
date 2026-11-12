"""Vuelca objetos del heap por vtable.  Uso: python objs.py <vtableRVA_hex> [maxHits=8] [size_hex=0x100] [texto_a_buscar]
Para cada objeto: qwords, y marca punteros que apuntan a texto (utf16/ascii) y si contiene `texto`."""
import ctypes, struct, sys
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
rva = int(sys.argv[1], 16)
maxhits = int(sys.argv[2]) if len(sys.argv) > 2 else 8
size = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x100
needle = sys.argv[4] if len(sys.argv) > 4 else None
target = struct.pack("<Q", base + rva)

def readstr(addr):
    raw = scan.read(h, addr, 64)
    if not raw: return None
    # utf16
    try:
        n = 0
        while n + 1 < len(raw) and (raw[n] or raw[n+1]) and n < 60: n += 2
        s = raw[:n].decode("utf-16le")
        if len(s) >= 2 and all(32 <= ord(c) < 127 for c in s): return "u16:" + s
    except Exception: pass
    n = 0
    while n < len(raw) and 32 <= raw[n] < 127: n += 1
    if n >= 3: return "a8:" + raw[:n].decode()
    return None

hits = []
for (a, sz) in scan.regions(h):
    if len(hits) >= maxhits * 6: break
    pos = 0
    CH = 1 << 20
    while pos < sz and len(hits) < maxhits * 6:
        buf = scan.read(h, a + pos, min(CH, sz - pos))
        if not buf: break
        i = buf.find(target)
        while i != -1:
            if i % 8 == 0: hits.append(a + pos + i)
            i = buf.find(target, i + 1)
        pos += CH

shown = 0
for obj in hits:
    raw = scan.read(h, obj, size)
    if not raw: continue
    q = struct.unpack("<%dQ" % (len(raw)//8), raw)
    texts = {}
    for i, v in enumerate(q):
        if 0x10000 < v < 0x7fffffffffff and i > 0:
            s = readstr(v)
            if s: texts[i*8] = s
    if needle and not any(needle in t for t in texts.values()):
        continue
    shown += 1
    print("== obj 0x%x" % obj)
    for i, v in enumerate(q):
        extra = ""
        if i*8 in texts: extra = "   -> " + texts[i*8]
        elif base <= v < base + 0x2000000: extra = "   client+0x%x" % (v - base)
        print("  +0x%02x: %016x  (i32: %d, %d)%s" % (i*8, v, struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0], struct.unpack('<i', struct.pack('<I', v >> 32))[0], extra))
    if shown >= maxhits: break
print("hits totales:", len(hits), "mostrados:", shown)
