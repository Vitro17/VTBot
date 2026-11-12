import sys, struct
sys.argv=["x"]
import scan, capstone
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid())
base = scan.client_module(h)
def name(vt):
    col = struct.unpack("<Q", scan.read(h, base+vt-8, 8))[0]
    c = scan.read(h, col, 16); td = struct.unpack_from("<I", c, 12)[0]
    return scan.read(h, base+td+0x10, 100).split(b"\0")[0].decode(errors="replace")
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for vt in (0x1868c80, 0x1868ce8, 0x18726a0, 0x1868db8):
    print("=== vt 0x%x %s" % (vt, name(vt)))
    for s in range(0, 34):
        f = struct.unpack("<Q", scan.read(h, base+vt+s*8, 8))[0]
        d = scan.read(h, f, 0x28)
        ins = list(md.disasm(d, f))
        txt = []
        for i in ins:
            txt.append(f"{i.mnemonic} {i.op_str}")
            if i.mnemonic in ("ret","jmp"): break
        print(f" slot {s:2d} +0x{f-base:x}: " + " ; ".join(txt)[:150])
