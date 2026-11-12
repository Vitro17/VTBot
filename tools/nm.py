import sys, struct
sys.argv=["x"]
sys.path.insert(0, "tools")
import scan
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid()); base = scan.client_module(h)
def name(vt):
    try:
        col = struct.unpack("<Q", scan.read(h, base+vt-8, 8))[0]
        c = scan.read(h, col, 16); td = struct.unpack_from("<I", c, 12)[0]
        return scan.read(h, base+td+0x10, 160).split(b"\0")[0].decode(errors="replace")
    except Exception as e:
        return "ERR col=%x %s" % (col, e)
# col es RVA o VA? probar ambos
def name2(vt):
    raw = struct.unpack("<Q", scan.read(h, base+vt-8, 8))[0]
    for col in (raw, base+raw):
        try:
            c = scan.read(h, col, 16)
            if not c: continue
            td = struct.unpack_from("<I", c, 12)[0]
            n = scan.read(h, base+td+0x10, 160)
            if n: return n.split(b"\0")[0].decode(errors="replace")
        except Exception: pass
    return "?? raw=%x" % raw
for vt in (0x185cb38, 0x185ca20, 0x1844260, 0x18639b0, 0x18688f8, 0x1868910):
    print(hex(vt), name2(vt))
