"""Vuelca los TContainer vivos y sus items.  Uso: python -u cont.py [itemdump_bytes_hex=0x60]
TContainer: +0x50 nslots, +0x60 firstIndex, +0x68/+0x70 vector<shared_ptr<Item>> (stride 0x10)."""
import ctypes, struct, sys
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
VT = 0x18377f8
dump = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x60
target = struct.pack("<Q", base + VT)

def q(a):
    b = scan.read(h, a, 8)
    return struct.unpack("<Q", b)[0] if b and len(b) == 8 else None

def ok_ptr(p):
    return p is not None and 0x10000 < p < 0x7fffffffffff

hits = []
for (a, sz) in scan.regions(h):
    pos = 0
    CH = 1 << 20
    while pos < sz:
        buf = scan.read(h, a + pos, min(CH, sz - pos))
        if not buf: break
        i = buf.find(target)
        while i != -1:
            if i % 8 == 0: hits.append(a + pos + i)
            i = buf.find(target, i + 1)
        pos += CH

print("candidatos TContainer:", len(hits))
for c in hits:
    n = struct.unpack("<I", scan.read(h, c + 0x50, 4))[0]
    first = struct.unpack("<I", scan.read(h, c + 0x60, 4))[0]
    vb, ve = q(c + 0x68), q(c + 0x70)
    if not (ok_ptr(vb) and ok_ptr(ve)) or ve < vb or (ve - vb) > 0x10000:
        continue
    cnt = (ve - vb) // 16
    if cnt == 0 or cnt > 4096: continue
    print("\n=== TContainer @0x%x nslots=%d first=%d vec=%d entradas" % (c, n, first, cnt))
    head = scan.read(h, c, 0xA0)
    for o in range(8, 0xA0, 8):
        v = struct.unpack_from("<Q", head, o)[0]
        print("  +0x%02x: 0x%016x" % (o, v))
    for i in range(min(cnt, 40)):
        p, ctrl = q(vb + i * 16), q(vb + i * 16 + 8)
        if not p: 
            continue
        vt = q(p)
        raw = scan.read(h, p, dump)
        dw = struct.unpack_from("<%dI" % (len(raw) // 4), raw)
        print("  slot %2d item=0x%x ctrl=0x%x vt=client+0x%x" % (i, p, ctrl or 0, (vt - base) if vt else 0))
        print("      " + " ".join("%08x" % d for d in dw))
