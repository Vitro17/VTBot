"""
Dado un nombre RTTI (p.ej. TGameActionSendChatMessage@input@tibia) localiza su vtable en client.exe vivo
y muestra los slots.  Uso: python rtti_vtable.py <NombreClase@ns@...> [n_slots=16]
"""
import sys, struct
import numpy as np
a = sys.argv[1:]
sys.argv = ["x"]
import scan

cls = a[0]
nslots = int(a[1]) if len(a) > 1 else 16
needle = (".?AV" + cls + "@@").encode()

pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
size = scan.module_size(h, base)
img = scan.read(h, base, size)
if img is None:
    # leer por trozos si falla la lectura completa
    img = b"".join((scan.read(h, base + o, min(0x100000, size - o)) or b"\0" * min(0x100000, size - o))
                   for o in range(0, size, 0x100000))

i = img.find(needle)
if i < 0:
    sys.exit("TypeDescriptor no encontrado")
td_rva = i - 0x10
print(f"TypeDescriptor RVA 0x{td_rva:x}")

u32 = np.frombuffer(img[: len(img) // 4 * 4], dtype=np.uint32)
cands = np.nonzero(u32 == td_rva)[0]
cols = []
for c in cands:
    off = int(c) * 4 - 12
    if off >= 0 and struct.unpack_from("<I", img, off)[0] == 1:
        cols.append(off)
print("COL RVAs:", [hex(c) for c in cols])

u64 = np.frombuffer(img[: len(img) // 8 * 8], dtype=np.uint64)
for col in cols:
    tgt = np.uint64(base + col)
    for k in np.nonzero(u64 == tgt)[0]:
        vt = int(k) * 8 + 8
        print(f"\nvtable RVA 0x{vt:x}  (COL 0x{col:x}, offset={struct.unpack_from('<I', img, col+4)[0]})")
        for s in range(nslots):
            q = struct.unpack_from("<Q", img, vt + s * 8)[0]
            if not (base <= q < base + size):
                break
            print(f"  slot {s:2d}: client+0x{q - base:x}")
