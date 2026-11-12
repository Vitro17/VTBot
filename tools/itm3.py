import ctypes, struct, scan
K = ctypes.windll.kernel32
h = K.OpenProcess(0x1FFFFF, False, scan.find_pid()); base = scan.client_module(h)
CUM = base + 0x1868c80
def rd(a,n): return scan.read(h,a,n)
def q(a):
    b = rd(a,8); return struct.unpack("<Q", b)[0] if b and len(b)==8 else None
def u32(a):
    b = rd(a,4); return struct.unpack("<I", b)[0] if b and len(b)==4 else None
target = struct.pack("<Q", base + 0x18377f8)
hits = []
for (a, sz) in scan.regions(h):
    pos = 0
    while pos < sz:
        buf = scan.read(h, a+pos, min(1<<20, sz-pos))
        if not buf: break
        i = buf.find(target)
        while i != -1:
            if i % 8 == 0: hits.append(a+pos+i)
            i = buf.find(target, i+1)
        pos += 1<<20
for c in hits:
    n = u32(c+0x50); vb, ve = q(c+0x68), q(c+0x70)
    if not vb or not ve or ve < vb or (ve-vb) > 0x4000: print(hex(c), "invalido"); continue
    out = []
    for i in range((ve-vb)//16):
        p = q(vb+i*16)
        if not p: out.append("-"); continue
        t = q(p+8); iid = u32(t) if t else None
        if q(p) == CUM: out.append("%s x%s" % (iid, u32(p+0x48)))
        else: out.append("%s" % iid)
    print(hex(c), "nslots", n, "refs", q(c+0x08), "|", ", ".join(out))
