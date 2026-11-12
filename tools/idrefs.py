"""Busca referencias (dwords alineados) a IDs de criaturas vivas fuera de los propios TCreature.
  python idrefs.py save <f.json>      guarda {id: [direcciones]}
  python idrefs.py diff <a.json> <b.json>
"""
import ctypes, json, re, struct, sys
import scan
RVA = 0x1837d80
K = ctypes.windll.kernel32

def main():
    pid = scan.find_pid()
    h = K.OpenProcess(0x1FFFFF, False, pid)
    base = scan.client_module(h)
    cmd = sys.argv[1]
    if cmd == "diff":
        a = json.load(open(sys.argv[2])); b = json.load(open(sys.argv[3]))
        names = b.get("_names", {})
        for cid in sorted(set(a) | set(b)):
            if cid == "_names": continue
            sa = set(a.get(cid, [])); sb = set(b.get(cid, []))
            if sa != sb:
                print(cid, names.get(cid, "?"), "solo en A:", [hex(x) for x in sorted(sa - sb)][:8],
                      "solo en B:", [hex(x) for x in sorted(sb - sa)][:8])
        return
    # 1) TCreature vivos
    target = struct.pack("<Q", base + RVA)
    objs = {}
    for (a, sz) in scan.regions(h):
        pos = 0; CH = 1 << 20
        while pos < sz:
            buf = scan.read(h, a + pos, min(CH, sz - pos))
            if not buf: break
            i = buf.find(target)
            while i != -1:
                if i % 8 == 0:
                    o = a + pos + i
                    raw = scan.read(h, o, 0x100)
                    if raw and len(raw) == 0x100:
                        q = struct.unpack("<32Q", raw)
                        if q[2] == o and q[3] == o - 0x10 and raw[0xe8] > 0:
                            cid = q[4] & 0xffffffff
                            nm = ""
                            if 0 < q[7] < 40:
                                r = scan.read(h, q[6], q[7] * 2)
                                if r: nm = r.decode("utf-16le", "replace")
                            if (cid >> 28) == 5:    # monstruos
                                objs[cid] = (o, nm)
                i = buf.find(target, i + 1)
            pos += CH
    print("monstruos vivos:", len(objs))
    spans = [(o, o + 0x400) for (o, _) in objs.values()]
    pat = re.compile(b"|".join(re.escape(struct.pack("<I", c)) for c in objs))
    res = {str(c): [] for c in objs}
    for (a, sz) in scan.regions(h):
        pos = 0; CH = 1 << 22
        while pos < sz:
            buf = scan.read(h, a + pos, min(CH, sz - pos))
            if not buf: break
            for m in pat.finditer(buf):
                off = m.start()
                if off % 4: continue
                addr = a + pos + off
                if any(s <= addr < e for s, e in spans): continue
                res[str(struct.unpack("<I", m.group())[0])].append(addr)
            pos += CH
    res["_names"] = {str(c): v[1] for c, v in objs.items()}
    json.dump(res, open(sys.argv[2], "w"))
    tot = sum(len(v) for k, v in res.items() if k != "_names")
    print("referencias totales:", tot, "->", sys.argv[2])

main()
