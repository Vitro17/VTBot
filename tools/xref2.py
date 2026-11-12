"""Busca TODA referencia (qword absoluto, o disp32 rip-relativo en cualquier instruccion) a un RVA dado.
Uso: python xref2.py <rva_hex>"""
import sys, struct
import numpy as np
a = sys.argv[1:]
sys.argv = ["x"]
import scan

target = int(a[0], 16)
pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
size = scan.module_size(h, base)
img = b"".join((scan.read(h, base + o, min(0x100000, size - o)) or b"\0" * min(0x100000, size - o))
               for o in range(0, size, 0x100000))

# 1) qwords absolutos (punteros en tablas de datos)
q = np.frombuffer(img[: len(img) // 8 * 8], dtype=np.uint64)
for k in np.nonzero(q == np.uint64(base + target))[0]:
    print(f"puntero absoluto en +0x{int(k)*8:x}")

# 2) qwords como RVA de 32 bits (tablas con RVAs)
d = np.frombuffer(img[: len(img) // 4 * 4], dtype=np.uint32)
for k in np.nonzero(d == np.uint32(target))[0]:
    print(f"dword RVA en +0x{int(k)*4:x}")

# 3) disp32 rip-relativo: para cada posicion p, target == p+4+disp (+ 0..3 bytes de inmediato tras el disp)
disp = np.frombuffer(img[: len(img) // 4 * 4], dtype=np.int32).astype(np.int64)
for imm in (0, 1, 4):
    for k in np.nonzero(disp + (np.arange(len(disp), dtype=np.int64) * 4) + 4 + imm == target)[0]:
        p = int(k) * 4
        print(f"disp32 rip-rel en +0x{p:x} (instr ~ +0x{p-3:x}, imm={imm})")
