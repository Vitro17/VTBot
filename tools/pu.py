import re
t = open("xb_result.txt", encoding="utf-8", errors="replace").read().split("\n")
sec = None; hits = {}; cur = None
for l in t:
    m = re.match(r"client\+0x([0-9a-f]+): (\d+) hits", l)
    if m: sec = m.group(1); hits[sec] = []; continue
    if l.startswith("  tid="):
        cur = []; hits[sec].append(cur)
    elif cur is not None: cur.append(l)
def qwords(lines, hdr):
    for i, l in enumerate(lines):
        if l.strip().startswith(hdr):
            out = []
            for l2 in lines[i+1:i+14]:
                m = re.match(r"\s+\+0x([0-9a-f]+): 0x([0-9a-f]+)", l2)
                if not m: break
                out.append((int(m.group(1),16), int(m.group(2),16)))
            return out
    return []
for sec, hs in hits.items():
    print("== BP", sec, "hits guardados:", len(hs))
    if sec != "5ba3e0": continue
    for n, lines in enumerate(hs):
        print("-- hit", n, lines[0].strip()[:90])
        c1 = dict(qwords(lines, "nested de +0x08"))
        c2 = dict(qwords(lines, "nested de +0x18"))
        if c1:
            v = c1.get(8, 0)
            print("   ctx1 vt=%x byte8=%d  dword0xC=%d  b10=%d b11=%d b12=%d" % (c1.get(0,0) & 0xffffff, v & 0xff, v >> 32, c1.get(0x10,0) & 0xff, (c1.get(0x10,0)>>8)&0xff, (c1.get(0x10,0)>>16)&0xff))
        if c2:
            print("   ctx2 vt=%x  +8=%016x (lo16=%d) +0x10=%016x +0x18=%016x" % (c2.get(0,0)&0xffffff, c2.get(8,0), c2.get(8,0)&0xffff, c2.get(0x10,0), c2.get(0x18,0)))
