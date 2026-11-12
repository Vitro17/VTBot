import struct, scan, sys
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid())
base = scan.client_module(h)
for a in sys.argv[1:]:
    col = int(a, 16)
    c = scan.read(h, base + col, 16)
    td = struct.unpack_from("<I", c, 12)[0]
    nm = scan.read(h, base + td + 0x10, 120)
    print(hex(col), nm.split(b"\0")[0].decode(errors="replace"))
