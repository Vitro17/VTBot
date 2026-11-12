import re
t = open("xb_result.txt", encoding="utf-8", errors="replace").read().split("\n")
hits = []; cur = None
for i, l in enumerate(t):
    if l.startswith("  tid="):
        cur = [i]; hits.append(cur)
    elif cur is not None:
        cur.append(i)
def block(lines, start_pat):
    out = []; on = False
    for l in lines:
        if l.lstrip().startswith(start_pat): on = True; out.append(l); continue
        if on:
            if re.match(r"\s+\+0x", l): out.append(l)
            else: break
    return out
n = 0
for h in hits:
    lines = [t[i] for i in h]
    d = [i for i, l in enumerate(lines) if "deref del 1er qword" in l]
    if not d: continue
    first = lines[d[0] + 1]
    if "18656b8" not in first: continue
    n += 1
    if n > 12: break
    print("=== intent #%d" % n)
    for l in lines[d[0]: d[0] + 12]: print(l)
    for i, l in enumerate(lines):
        if l.strip().startswith("nested de"):
            print(l)
            for k in range(i + 1, min(i + 7, len(lines))): print(lines[k])
print("total intents:", n)
