"""
Busca referencias de codigo (lea reg,[rip+disp]) a un string de client.exe vivo.
Uso: python xref.py <string_exacto> [bytes_antes_hex=0x60] [bytes_despues_hex=0x120]
"""
import sys, struct
import numpy as np
a = sys.argv[1:]
sys.argv = ["x"]
import scan, capstone

text = a[0].encode()
before = int(a[1], 16) if len(a) > 1 else 0x60
after = int(a[2], 16) if len(a) > 2 else 0x120

pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
size = scan.module_size(h, base)
img = b"".join((scan.read(h, base + o, min(0x100000, size - o)) or b"\0" * min(0x100000, size - o))
               for o in range(0, size, 0x100000))

# direcciones del string (terminado en NUL, no parte de otro string mas largo)
locs = []
i = -1
while True:
    i = img.find(b"\0" + text + b"\0", i + 1)
    if i < 0:
        break
    locs.append(i + 1)
print("string en RVAs:", [hex(x) for x in locs])

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
arr = np.frombuffer(img, dtype=np.uint8)
res = []
for s in locs:
    # lea r64,[rip+disp32] = REX(48|4C) 8D modrm(xx000101) disp32
    for rex in (0x48, 0x4C):
        idx = np.nonzero((arr[:-7] == rex) & (arr[1:-6] == 0x8D) & ((arr[2:-5] & 0xC7) == 0x05))[0]
        for p in idx:
            disp = struct.unpack_from("<i", img, int(p) + 3)[0]
            if int(p) + 7 + disp == s:
                res.append(int(p))
print("xrefs (RVA de la instruccion lea):", [hex(x) for x in res])

for p in res[:3]:
    print(f"\n=== xref en +0x{p:x} ===")
    start = max(0, p - before)
    for ins in md.disasm(img[start: p + after], base + start):
        rva = ins.address - base
        mark = "   <== " if rva == p else ""
        print(f"+0x{rva:x}: {ins.bytes.hex(' '):<28} {ins.mnemonic} {ins.op_str}{mark}")
