"""Vigila campos del TCreatureStorage y muestra los cambios.  Uso: python watchtarget.py <obj_hex> <segundos>"""
import ctypes, struct, sys, time
import scan
import crsnap
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
obj = int(sys.argv[1], 16); secs = float(sys.argv[2])
names = {int(k): v["name"] for k, v in crsnap.snapshot().items()}
print("listo, vigilando %.0fs ..." % secs, flush=True)
last = None
t0 = time.time()
while time.time() - t0 < secs:
    raw = scan.read(h, obj + 0x18, 0x20)
    vals = struct.unpack("<8I", raw)    # +0x18,+0x1c,+0x20,+0x24,+0x28,+0x2c,+0x30,+0x34
    key = vals
    if key != last:
        def n(i): return "%d(%s)" % (i, names.get(i, "?")) if i else "0"
        print("%5.1fs  +0x18=%d +0x1c=%d +0x20=%s +0x24=%d +0x28=%s +0x2c=%s +0x30=%d" % (
            time.time() - t0, vals[0], vals[1], n(vals[2]), vals[3], n(vals[4]), n(vals[5]), vals[6]), flush=True)
        last = key
    time.sleep(0.1)
print("fin")
