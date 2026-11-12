import ctypes, struct, scan
K = ctypes.windll.kernel32
h = K.OpenProcess(0x1FFFFF, False, scan.find_pid()); base = scan.client_module(h)
def rd(a,n): return scan.read(h,a,n)
def q(a):
    b = rd(a,8); return struct.unpack("<Q", b)[0] if b and len(b)==8 else None
def find(vt):
    target = struct.pack("<Q", base+vt); hits=[]
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
    return hits
conts = find(0x18377f8)
cset = set(conts)
print("TContainer +8 byte:")
for c in conts:
    vb, ve = q(c+0x68), q(c+0x70)
    ok = vb and ve and ve>=vb and (ve-vb)<0x4000
    print("  ", hex(c), "id_byte=", (q(c+8) or 0)&0xff, "valido" if ok else "invalido")
st = find(0x1837a40)
print("TContainerStorage:", [hex(s) for s in st])
for s in st:
    raw = rd(s, 0x100)
    for o in range(8, 0xf0, 8):
        p = struct.unpack_from("<Q", raw, o)[0]; sz = struct.unpack_from("<Q", raw, o+8)[0]
        if not (0x10000 < p < 0x7fffffffffff) or sz == 0 or sz > 64: continue
        hd = rd(p, 0x30)
        if not hd or len(hd) < 0x30 or hd[0x19] != 1: continue   # _Isnil del nodo cabeza
        # recorrer: head.left = minimo
        nodes=[]; stack=[]; 
        # in-order via left/right/parent pointers (simple DFS por right/left desde parent=root)
        root = struct.unpack_from("<Q", hd, 8)[0]
        todo=[root]; seen=set()
        while todo:
            n = todo.pop()
            if n in seen or n == p or not (0x10000 < n < 0x7fffffffffff): continue
            seen.add(n)
            nd = rd(n, 0x40)
            if not nd or nd[0x19]: continue
            l, r = struct.unpack_from("<QQ", nd, 0)[0], struct.unpack_from("<Q", nd, 16)[0]
            l = struct.unpack_from("<Q", nd, 0)[0]
            key = struct.unpack_from("<I", nd, 0x20)[0]
            val = struct.unpack_from("<QQ", nd, 0x28)
            nodes.append((key, val))
            todo += [l, r]
        print("  storage", hex(s), "off 0x%x" % o, "size", sz, "->", [(k, hex(v[0]), "TContainer" if v[0] in cset else "?") for k, v in nodes])
