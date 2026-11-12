"""Resuelve una entrada de la IAT de client.exe vivo a  modulo!funcion_exportada.
Uso: python iat.py <rva_hex_de_la_entrada_IAT> [<rva_string_hex> ...]"""
import sys, struct, ctypes, ctypes.wintypes as wt
a = sys.argv[1:]
sys.argv = ["x"]
import scan

pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)

for r in a[1:]:
    d = scan.read(h, base + int(r, 16), 96) or b""
    print(f"string @+{r}: {d.split(b'\\0')[0].decode('latin1')!r}")

slot = int(a[0], 16)
fn = struct.unpack("<Q", scan.read(h, base + slot, 8))[0]
print(f"IAT +0x{slot:x} -> 0x{fn:x}")

# localizar modulo que contiene fn
PSAPI = scan.PSAPI
mods = (ctypes.c_void_p * 1024)()
need = wt.DWORD()
PSAPI.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 3)
for i in range(need.value // 8):
    m = mods[i]
    if not m:
        continue
    sz = scan.module_size(h, m)
    if m <= fn < m + sz:
        nm = ctypes.create_unicode_buffer(260)
        PSAPI.GetModuleFileNameExW(h, ctypes.c_void_p(m), nm, 260)
        print("modulo:", nm.value, f"base 0x{m:x}  rva 0x{fn - m:x}")
        # parsear exports
        img = scan.read(h, m, 0x400) or b""
        pe = struct.unpack_from("<I", img, 0x3C)[0]
        exp_rva, exp_sz = struct.unpack_from("<II", scan.read(h, m + pe + 0x88, 8))
        ed = scan.read(h, m + exp_rva, exp_sz)
        nfun, nnames, aof, aon, aoo = struct.unpack_from("<IIIII", ed, 20)
        for k in range(nnames):
            nrva = struct.unpack_from("<I", scan.read(h, m + aon + 4 * k, 4))[0]
            ordi = struct.unpack_from("<H", scan.read(h, m + aoo + 2 * k, 2))[0]
            frva = struct.unpack_from("<I", scan.read(h, m + aof + 4 * ordi, 4))[0]
            if frva == fn - m:
                name = scan.read(h, m + nrva, 200).split(b"\0")[0].decode()
                print("export:", name)
        break
