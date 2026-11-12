"""Busca un texto (utf16) en memoria y los QString/estructuras que lo referencian.
Uso: python findtext.py "exori gran" [maxhits=6]"""
import ctypes, struct, sys
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
text = sys.argv[1]; maxhits = int(sys.argv[2]) if len(sys.argv) > 2 else 6
pat = text.encode("utf-16le")
size = len(text)

def regions():
    for (a, sz) in scan.regions(h):
        pos = 0; CH = 1 << 22
        while pos < sz:
            buf = scan.read(h, a + pos, min(CH, sz - pos))
            if not buf: break
            yield a + pos, buf
            pos += CH

hits = []
for addr, buf in regions():
    i = buf.find(pat)
    while i != -1:
        a = addr + i
        # texto exacto: terminado (siguiente caracter nulo o fin de palabra) o string completo
        hits.append(a)
        i = buf.find(pat, i + 1)
print("ocurrencias del texto:", len(hits))
hits = hits[:40]
targets = {struct.pack("<Q", a): a for a in hits}
refs = []
for addr, buf in regions():
    for t, ta in targets.items():
        i = buf.find(t)
        while i != -1:
            if i % 8 == 0 and (addr + i + 8) and len(refs) < 200:
                # QString {d, ptr, size}: size == len(text) en +8
                after = buf[i + 8:i + 16]
                if len(after) == 8 and struct.unpack("<Q", after)[0] == size:
                    refs.append((addr + i - 8, ta))      # inicio del QString (d)
            i = buf.find(t, i + 1)
print("QStrings que lo referencian:", len(refs))
for qs, ta in refs[:maxhits]:
    raw = scan.read(h, qs - 0x60, 0x100)
    print("== QString en 0x%x (texto en 0x%x)" % (qs, ta))
    for k in range(0, 0x100, 8):
        v = struct.unpack_from("<Q", raw, k)[0]
        mark = "  <== QString.d" if k == 0x60 else ""
        extra = "   client+0x%x" % (v - base) if base <= v < base + 0x2000000 else ""
        print("  %+#05x: %016x  (%d, %d)%s%s" % (k - 0x60, v, struct.unpack_from("<i", raw, k)[0], struct.unpack_from("<i", raw, k + 4)[0], mark, extra))
