import re
t = open("xb_result.txt", encoding="utf-8", errors="replace").read().split("\n")
hits = []; cur = None
for l in t:
    if l.startswith("  tid="):
        cur = []; hits.append(cur)
    elif cur is not None:
        cur.append(l)
n = 0
for lines in hits:
    for i, l in enumerate(lines):
        if l.strip().startswith("nested de +0x08") and "18656e0" in "".join(lines[i+1:i+2]):
            n += 1
            print("=== giro #%d" % n)
            print("\n".join(lines[i:i+5]))
            for j, l2 in enumerate(lines):
                if l2.strip().startswith("nested de +0x10"):
                    print("\n".join(lines[j:j+5])); break
            # objeto accion (deref)
            for j, l2 in enumerate(lines):
                if "deref del 1er qword" in l2:
                    print("\n".join(lines[j:j+5])); break
            break
    if n >= 3: break
