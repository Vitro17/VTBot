import sys, struct
sys.argv=["x"]
sys.path.insert(0, "tools")
import scan
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, scan.find_pid()); base = scan.client_module(h)
v = struct.unpack("<Q", scan.read(h, base+0x18753d0+8*11, 8))[0]
print("slot11 = %x (base %x) rva=%x" % (v, base, v-base))
for i in (10,12,13):
    v = struct.unpack("<Q", scan.read(h, base+0x18753d0+8*i, 8))[0]; print(i, hex(v-base))
