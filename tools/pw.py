import re
L = open("tools/xb_result.txt", encoding="utf-8", errors="replace").read().split("\n")
hit=-1; want={2,3,5,7,9,11}; sec=None; pr=False
for ln in L:
    if ln.startswith("  tid="):
        hit+=1; pr=False; continue
    if hit in want:
        m = re.match(r"\s+(deref del 1er qword|nested de \+0x([0-9a-f]+)) ->", ln)
        if m:
            nm = m.group(2) or "ACTION"
            pr = nm in ("28","08","38","30","ACTION","18")
            if pr: print("--- hit", hit, nm)
            continue
        if pr and re.match(r"\s+\+0x[0-9a-f]+: 0x", ln):
            off = int(ln.split("+0x")[1].split(":")[0],16)
            if off <= 0x40: print(ln.split("(")[0])
