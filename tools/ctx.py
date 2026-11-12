"""Muestra strings consecutivos alrededor de un offset-RVA.  Uso: python ctx.py <rva_hex> [antes=700] [despues=900]
Para client.exe las secciones de datos estan en la misma posicion en archivo que en memoria solo si rva==raw; aqui
se lee de memoria viva."""
import sys
a = sys.argv[1:]
sys.argv = ["x"]
import scan

rva = int(a[0], 16)
before = int(a[1]) if len(a) > 1 else 700
after = int(a[2]) if len(a) > 2 else 900
pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
d = scan.read(h, base + rva - before, before + after)
parts = d.split(b"\0")
off = rva - before
for p in parts:
    if len(p) >= 2:
        print(f"+0x{off:x}: {p.decode('latin1')}")
    off += len(p) + 1
