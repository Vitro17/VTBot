"""Instantanea/diff de objetos QObject vivos por vtable.
  python objdiff.py save <out.json> <size_hex> <nombre=rvaHex> [nombre=rvaHex ...]
  python objdiff.py diff <a.json> <b.json>
"""
import ctypes, json, struct, sys
import scan
K = ctypes.windll.kernel32

def live(h, o, vt):
    r = scan.read(h, o, 16)
    if not r: return False
    v, d = struct.unpack("<QQ", r)
    if v != vt or not d or d & 7: return False
    r2 = scan.read(h, d + 8, 8)
    return bool(r2) and struct.unpack("<Q", r2)[0] == o

def main():
    if sys.argv[1] == "diff":
        a = json.load(open(sys.argv[2])); b = json.load(open(sys.argv[3]))
        for name in b:
            for oa, ra_hex in a.get(name, {}).items():
                rb_hex = b[name].get(oa)
                if not rb_hex: continue
                ra = bytes.fromhex(ra_hex); rb = bytes.fromhex(rb_hex)
                ch = []
                for off in range(0, min(len(ra), len(rb)), 4):
                    x = struct.unpack_from("<I", ra, off)[0]; y = struct.unpack_from("<I", rb, off)[0]
                    if x != y: ch.append("+0x%x:%d->%d(0x%x->0x%x)" % (off, x, y, x, y))
                print(name, oa, "cambios:", len(ch))
                for c in ch[:40]: print("    ", c)
        return
    out = sys.argv[2]; size = int(sys.argv[3], 16)
    pid = scan.find_pid()
    h = K.OpenProcess(0x1FFFFF, False, pid)
    base = scan.client_module(h)
    res = {}
    for spec in sys.argv[4:]:
        name, rva = spec.split("=")
        vt = base + int(rva, 16)
        target = struct.pack("<Q", vt)
        res[name] = {}
        for (a, sz) in scan.regions(h):
            pos = 0; CH = 1 << 20
            while pos < sz:
                buf = scan.read(h, a + pos, min(CH, sz - pos))
                if not buf: break
                i = buf.find(target)
                while i != -1:
                    if i % 8 == 0 and live(h, a + pos + i, vt):
                        raw = scan.read(h, a + pos + i, size)
                        if raw: res[name]["0x%x" % (a + pos + i)] = raw.hex()
                    i = buf.find(target, i + 1)
                pos += CH
        print(name, "objetos vivos:", list(res[name].keys()))
    json.dump(res, open(out, "w"))

main()
