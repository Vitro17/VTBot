"""Lista compacta de TCreature vivos.  Uso: python crlist.py [vtableRVA=1837d80]"""
import ctypes, struct, sys
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
rva = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x1837d80
target = struct.pack("<Q", base + rva)

def rd(a, n):
    return scan.read(h, a, n)

def u64(a):
    r = rd(a, 8)
    return struct.unpack("<Q", r)[0] if r else 0

hits = []
for (a, sz) in scan.regions(h):
    pos = 0; CH = 1 << 20
    while pos < sz:
        buf = rd(a + pos, min(CH, sz - pos))
        if not buf: break
        i = buf.find(target)
        while i != -1:
            if i % 8 == 0: hits.append(a + pos + i)
            i = buf.find(target, i + 1)
        pos += CH

seen = {}
for o in hits:
    raw = rd(o, 0x100)
    if not raw or len(raw) < 0x100: continue
    q = struct.unpack("<32Q", raw)
    # vivo: shared_from_this apunta a si mismo (+0x10 == obj) y +0x18 == obj-0x10
    if q[2] != o or q[3] != o - 0x10: continue
    cid = q[4] & 0xffffffff
    sz_name = q[7]
    nm = ""
    if 0 < sz_name < 40 and 0x10000 < q[6] < 0x7fffffffffff:
        r = rd(q[6], sz_name * 2)
        if r: nm = r.decode("utf-16le", "replace")
    print("0x%x id=%d(0x%x) tipo?=%d name=%r  hp?=%d,%d  +0x60=%016x +0x88=%016x +0x90=%016x +0x98=%016x" % (
        o, cid, cid, q[4] >> 32, nm, raw[0xe8], raw[0xe9], q[12], q[17], q[18], q[19]))
print("hits:", len(hits))
