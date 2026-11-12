"""Desensambla una region de client.exe en vivo.  Uso: python disasm.py <rva_hex_inicio> [bytes_hex=0x140] [rva_marca_hex]"""
import sys
a = sys.argv[1:]
sys.argv = ["x"]
import scan, capstone

start = int(a[0], 16)
size = int(a[1], 16) if len(a) > 1 else 0x140
mark = int(a[2], 16) if len(a) > 2 else None
pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
data = scan.read(h, base + start, size)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for i in md.disasm(data, base + start):
    rva = i.address - base
    tag = "   <== " if mark is not None and rva == mark else ""
    print(f"+0x{rva:x}: {i.bytes.hex(' '):<30} {i.mnemonic} {i.op_str}{tag}")
