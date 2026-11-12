"""Instantaneas de TCreature.
  python crsnap.py save <archivo.json>            guarda 0x180 bytes de cada TCreature vivo (por id)
  python crsnap.py diff <a.json> <b.json> [soloVivos]  muestra offsets que cambiaron por criatura (id,nombre)
"""
import ctypes, json, struct, sys
import scan

RVA = 0x1837d80

def snapshot():
    K = ctypes.windll.kernel32
    pid = scan.find_pid()
    h = K.OpenProcess(0x1FFFFF, False, pid)
    base = scan.client_module(h)
    target = struct.pack("<Q", base + RVA)
    hits = []
    for (a, sz) in scan.regions(h):
        pos = 0; CH = 1 << 20
        while pos < sz:
            buf = scan.read(h, a + pos, min(CH, sz - pos))
            if not buf: break
            i = buf.find(target)
            while i != -1:
                if i % 8 == 0: hits.append(a + pos + i)
                i = buf.find(target, i + 1)
            pos += CH
    out = {}
    for o in hits:
        raw = scan.read(h, o, 0x180)
        if not raw or len(raw) < 0x180: continue
        q = struct.unpack("<48Q", raw)
        if q[2] != o or q[3] != o - 0x10: continue
        cid = q[4] & 0xffffffff
        nm = ""
        if 0 < q[7] < 40 and 0x10000 < q[6] < 0x7fffffffffff:
            r = scan.read(h, q[6], q[7] * 2)
            if r: nm = r.decode("utf-16le", "replace")
        out[str(cid)] = {"addr": o, "name": nm, "raw": raw.hex()}
    return out

def main():
    cmd = sys.argv[1]
    if cmd == "save":
        s = snapshot()
        json.dump(s, open(sys.argv[2], "w"))
        print("guardadas", len(s), "criaturas ->", sys.argv[2])
    elif cmd == "diff":
        a = json.load(open(sys.argv[2])); b = json.load(open(sys.argv[3]))
        for cid, vb in b.items():
            va = a.get(cid)
            if not va: continue
            ra = bytes.fromhex(va["raw"]); rb = bytes.fromhex(vb["raw"])
            ch = []
            for off in range(0, 0x180, 4):
                if off in (0x08,): continue
                x = struct.unpack_from("<I", ra, off)[0]; y = struct.unpack_from("<I", rb, off)[0]
                if x != y: ch.append("+0x%x:%d->%d" % (off, x, y))
            if ch:
                print("%s %-16s %s" % (cid, vb["name"], "  ".join(ch[:14])))
main()
