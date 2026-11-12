import sys, struct, re
sys.argv=["x"]
sys.path.insert(0, "tools")
import scan
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid()); base = scan.client_module(h)
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
    return "?"
lines = open("tools/xb_result.txt", encoding="utf-8", errors="replace").read().split("\n")
hit = -1; sect = None; out = []
cache = {}
def nm(vt):
    if vt not in cache: cache[vt] = name2(vt)
    return cache[vt]
cur = None
for ln in lines:
    if ln.startswith("  tid="):
        hit += 1; cur = {"n": hit, "secs": []}; out.append(cur); continue
    if cur is None: continue
    m = re.match(r"\s+(deref del 1er qword|nested de \+0x([0-9a-f]+)) ->", ln)
    if m:
        sec = {"name": m.group(2) or "ACTION", "q": {}}
        cur["secs"].append(sec); continue
    m = re.match(r"\s+\+0x([0-9a-f]+): 0x([0-9a-f]{16})", ln)
    if m and cur["secs"]:
        cur["secs"][-1]["q"][int(m.group(1),16)] = int(m.group(2),16)
seen = set()
for hh in out:
    if not hh["secs"]: continue
    a = hh["secs"][0]
    avt = a["q"].get(0, 0) - base
    desc = ["#%d %s" % (hh["n"], nm(avt))]
    for s in hh["secs"][1:]:
        if s["name"] not in ("08","18","28","20","10","30"): continue
        vt = s["q"].get(0,0) - base
        if not (0x1800000 < vt < 0x1900000): continue
        nmv = nm(vt)
        if "Control" in nmv or "ctrl" in nmv.lower() or "_Ref_count" in nmv: continue
        q8 = s["q"].get(8,0)
        desc.append("  +%s %s  q8=%016x" % (s["name"], nmv, q8))
    key = "|".join(desc[1:])
    if key in seen: continue
    seen.add(key)
    print("\n".join(desc))
