import ctypes, struct, scan
K = ctypes.windll.kernel32
h = K.OpenProcess(0x1FFFFF, False, scan.find_pid()); base = scan.client_module(h)
def rd(a,n): return scan.read(h,a,n)
def q(a):
    b = rd(a,8); return struct.unpack("<Q", b)[0] if b and len(b)==8 else None
for it in (0x1c48670aef0, 0x1c48670add0, 0x1c48670a0b0, 0x1c48670a7d0):
    t = q(it+8)
    print("item", hex(it), "type", hex(t))
    for o in range(0, 0x80, 8):
        v = q(t+o)
        d = struct.pack("<Q", v)
        lo, hi = struct.unpack("<II", d)
        print("   +0x%02x: %016x   lo=%d hi=%d" % (o, v, lo, hi))
    print("   --- item +0x20..0x60")
    for o in range(0x18, 0x60, 8):
        v = q(it+o); lo, hi = struct.unpack("<II", struct.pack("<Q", v))
        print("   +0x%02x: %016x   lo=%d hi=%d" % (o, v, lo, hi))
