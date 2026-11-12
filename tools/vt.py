"""Inspecciona un objeto: vtable (RVA) y primeros qwords.  Uso: python vt.py <addr_objeto_hex> [n_qwords=24]"""
import sys, struct
a = sys.argv[1:]
sys.argv = ["x"]
import scan

obj = int(a[0], 16)
n = int(a[1]) if len(a) > 1 else 24
pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
size = scan.module_size(h, base)
d = scan.read(h, obj, n * 8)
print(f"client base=0x{base:x} size=0x{size:x}")
for i in range(n):
    q = struct.unpack_from("<Q", d, i * 8)[0]
    tag = f"  client+0x{q-base:x}" if base <= q < base + size else ""
    print(f"+0x{i*8:03x}: 0x{q:016x}{tag}")
