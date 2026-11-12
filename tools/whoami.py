import ctypes, struct, sys
import scan
K = ctypes.windll.kernel32
pid = scan.find_pid()
h = K.OpenProcess(0x1FFFFF, False, pid)
base = scan.client_module(h)
print("pid", pid, "base", hex(base) if base else None)
for a in sys.argv[1:]:
    addr = int(a, 16)
    raw = scan.read(h, addr, 0x40)
    if not raw:
        print(hex(addr), "no legible"); continue
    q = struct.unpack("<8Q", raw)
    print(hex(addr), [hex(x) for x in q])
    if base:
        print("  vtable rva:", hex(q[0] - base))
