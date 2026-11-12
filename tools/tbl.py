import struct, scan
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid())
base = scan.client_module(h)
start = 0x1874080
raw = scan.read(h, base + start, 0x1c0)
for i in range(0, len(raw), 8):
    q = struct.unpack_from("<Q", raw, i)[0]
    lab = f"client+0x{q-base:x}" if base <= q < base + 0x1c9d000 else hex(q)
    print(f"+0x{start+i:x}: {lab}")
