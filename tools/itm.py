import ctypes, struct, scan
K = ctypes.windll.kernel32
pid = scan.find_pid(); h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
def rd(a,n): return scan.read(h,a,n)
def q(a): return struct.unpack("<Q", rd(a,8))[0]
c = 0x1c48b1bfc60
vb, ve = q(c+0x68), q(c+0x70)
for i in range((ve-vb)//16):
    p = q(vb+i*16)
    if not p: print(i,"vacio"); continue
    t = q(p+8)
    cnt = struct.unpack("<I", rd(p+0x48,4))[0]
    raw = rd(t, 0x80)
    dw = struct.unpack("<%dI"%(len(raw)//4), raw)
    ids = [(hex(o*4), d) for o,d in enumerate(dw) if d in (3031,3035,3043)]
    print(i, "item", hex(p), "count", cnt, "type", hex(t), "vt", hex(q(t)-base), "ids", ids)
