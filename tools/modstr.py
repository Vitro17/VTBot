import sys, re
sys.argv=["x"]
import scan
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid()); base = scan.client_module(h)
import ctypes, struct
# tamano del modulo
import ctypes.wintypes as wt
class MI(ctypes.Structure): _fields_=[("b",ctypes.c_void_p),("sz",wt.DWORD),("e",ctypes.c_void_p)]
size = 0x2400000
pats = [b"ungry", b"u\0n\0g\0r\0y\0", b"unger", b"u\0n\0g\0e\0r\0", b"nutrition", b"Starv", b"starv"]
pos = 0
while pos < size:
    buf = scan.read(h, base+pos, min(1<<20, size-pos))
    if buf:
        for p in pats:
            for m in re.finditer(re.escape(p), buf):
                s = max(0, m.start()-40); e = min(len(buf), m.end()+60)
                raw = buf[s:e]
                txt = raw.replace(b"\0", b"").decode("latin1")
                txt = re.sub(r"[^\x20-\x7e]", ".", txt)
                print("+0x%x  %s" % (pos+m.start(), txt))
    pos += 1<<20
