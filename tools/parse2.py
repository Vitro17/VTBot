import re, collections
t = open("xb_result.txt", encoding="utf-8", errors="replace").read().split("\n")
hits = []; cur = None
for l in t:
    if l.startswith("  tid="):
        cur = []; hits.append(cur)
    elif cur is not None:
        cur.append(l)
def q(line): 
    m = re.search(r"\+0x([0-9a-f]+): 0x([0-9a-f]{16})(.*)", line); return (int(m.group(1),16), int(m.group(2),16), m.group(3)) if m else None
rows = collections.Counter(); samples = {}
for lines in hits:
    # nested +0x08 block
    for i, l in enumerate(lines):
        if l.strip().startswith("nested de +0x08"):
            blk = [q(x) for x in lines[i+1:i+7]]
            blk = [b for b in blk if b]
            vt = [b for b in blk if b[0]==0]
            if not vt: continue
            m = re.search(r"client\+0x([0-9a-f]+)", vt[0][2])
            vtr = m.group(1) if m else hex(vt[0][1])
            f8 = [b for b in blk if b[0]==8][0][1]
            key = (vtr, f8 & 0xFFFFFFFF, (f8 >> 32) & 0xFFFF)
            rows[key] += 1
            samples.setdefault(key, lines)
            break
for k, v in sorted(rows.items()): print("intent vtable=client+0x%s  +8 lo32=%d  hi16=%d  -> %d veces" % (k[0], k[1], k[2], v))
print("total:", sum(rows.values()))
