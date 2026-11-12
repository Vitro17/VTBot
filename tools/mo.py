import ctypes, struct, scan
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid())
base = scan.client_module(h)
mo = base + 0xffabb0
raw = scan.read(h, mo, 0x30)
q = struct.unpack("<6Q", raw)
print("metaobject:", [hex(x) for x in q])
sd = q[1]
hdr = scan.read(h, sd, 0x20)
u = struct.unpack("<8I", hdr)
print("stringdata hdr:", u)
off, size = u[0], u[1]
name = scan.read(h, sd + off, size + 1)
print("clase:", name)
sup = q[0]
if sup:
    r2 = scan.read(h, sup, 0x30); q2 = struct.unpack("<6Q", r2)
    h2 = struct.unpack("<8I", scan.read(h, q2[1], 0x20))
    print("super:", scan.read(h, q2[1] + h2[0], h2[1] + 1))
