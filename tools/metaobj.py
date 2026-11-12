"""Decodifica un QMetaObject (Qt6) de client.exe vivo: clase, metodos (señales/slots) con firmas.
Layout observado en este cliente: +0 superdata, +8 getter, +0x10 stringdata, +0x18 data, +0x20 static_metacall.
Uso: python metaobj.py <rva_hex_del_QMetaObject> [filtro_regex]"""
import sys, struct, re
a = sys.argv[1:]
sys.argv = ["x"]
import scan

pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
base = scan.client_module(h)
size = scan.module_size(h, base)
rva = int(a[0], 16)
flt = re.compile(a[1], re.I) if len(a) > 1 else None


def rb(addr, n):
    return scan.read(h, addr, n) or b"\0" * n


def rq(addr): return struct.unpack("<Q", rb(addr, 8))[0]
def ru(addr): return struct.unpack("<I", rb(addr, 4))[0]
def tag(q): return f"client+0x{q-base:x}" if base <= q < base + size else f"0x{q:x}"


mo = base + rva
strd, data, smc = rq(mo + 0x10), rq(mo + 0x18), rq(mo + 0x20)
print(f"QMetaObject @ 0x{mo:x}: stringdata={tag(strd)} data={tag(data)} static_metacall={tag(smc)} (RVA 0x{smc-base:x})")


def qstr(i):
    off, ln = ru(strd + 8 * i), ru(strd + 8 * i + 4)
    return rb(strd + off, ln).decode("latin1")


hdr = [ru(data + 4 * i) for i in range(14)]
rev, cls_i, ci_n, ci_off, nmeth, meth_off, nprops, props_off = hdr[:8]
nsig = hdr[13]
print(f"revision={rev} class={qstr(cls_i)!r} methods={nmeth} (señales={nsig}) properties={nprops}")
for m in range(nmeth):
    o = data + 4 * (meth_off + 6 * m)
    name_i, argc, par_off, tag_i, flags, mt = [ru(o + 4 * k) for k in range(6)]
    po = data + 4 * par_off
    types = [ru(po + 4 * k) for k in range(1 + argc)]
    tn = [qstr(t & 0x7FFFFFFF) if t & 0x80000000 else f"#{t}" for t in types]
    kind = {0: "metodo", 4: "SIGNAL", 8: "slot", 12: "ctor"}[flags & 0xC]
    line = f"[{m:3d}] {kind:6s} {qstr(name_i)}({', '.join(tn[1:])})"
    if not flt or flt.search(line):
        print(line)
