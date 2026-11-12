"""Dado el RTTI de una clase QObject, halla vtable, QMetaObject (via metaObject() = slot0) y vuelca sus metodos.
Uso: python qtclass.py <TClase@ns@...> [filtro_regex]"""
import sys, subprocess, re, os
here = os.path.dirname(os.path.abspath(__file__))
cls = sys.argv[1]
flt = sys.argv[2:] 
out = subprocess.check_output([sys.executable, os.path.join(here, "rtti_vtable.py"), cls, "3"], text=True)
m = re.search(r"vtable RVA 0x([0-9a-f]+)", out)
if not m:
    sys.exit("sin vtable:\n" + out)
slots = re.findall(r"slot\s+(\d+): client\+0x([0-9a-f]+)", out)
print(f"{cls}: vtable 0x{m.group(1)}  slot0=0x{slots[0][1]}")
dis = subprocess.check_output([sys.executable, os.path.join(here, "disasm.py"), slots[0][1], "0x60"], text=True)
mo = None
for line in dis.splitlines():
    mm = re.search(r"lea r[a-z0-9]+, \[rip \+ (0x[0-9a-f]+)\]", line)
    if mm:
        addr = int(re.match(r"\+0x([0-9a-f]+):", line).group(1), 16)
        ln = int(line.split()[1 - 1].strip("+:").replace("0x", ""), 16) if False else None
        # direccion de la instruccion siguiente = addr + longitud; obtener bytes
        by = re.match(r"\+0x[0-9a-f]+: ((?:[0-9a-f]{2} )+)", line).group(1).split()
        nxt = addr + len(by)
        cand = nxt + int(mm.group(1), 16)
        mo = cand
        break
if mo is None:
    print(dis)
    sys.exit("no encontre lea en metaObject()")
print(f"QMetaObject RVA 0x{mo:x}")
args = [sys.executable, os.path.join(here, "metaobj.py"), f"{mo:x}"] + flt
print(subprocess.check_output(args, text=True))
