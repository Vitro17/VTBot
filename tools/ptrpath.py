"""
Busca rutas de punteros estaticas  client.exe+RVA -> [+off] -> ... -> objetivo.
Uso: python ptrpath.py <addr_hex_objetivo> [profundidad=4] [ventana_hex=0x100]
Hace UNA pasada por la memoria, guarda todos los qword que parecen punteros y hace BFS hacia atras.
"""
import sys, numpy as np
sys.argv_backup = sys.argv
import scan

target = int(sys.argv[1], 16)
depth = int(sys.argv[2]) if len(sys.argv) > 2 else 4
window = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x100

scan.sys.argv = [sys.argv[0]]
pid = scan.find_pid()
h = scan.K32.OpenProcess(scan.PROCESS_ALL, False, pid)
mbase = scan.client_module(h)
msize = scan.module_size(h, mbase)

# 1) recolectar (direccion_del_qword, valor) de cualquier qword que apunte a heap o a la imagen
lo_heap, hi_heap = 0x10000000000, 0x7FF000000000
addrs, vals = [], []
for base, size in scan.regions(h):
    data = scan.read(h, base, size)
    if not data:
        continue
    n = len(data) // 8
    arr = np.frombuffer(data, dtype=np.uint64, count=n)
    m = ((arr >= lo_heap) & (arr < hi_heap)) | ((arr >= mbase) & (arr < mbase + msize))
    idx = np.nonzero(m)[0]
    if idx.size:
        addrs.append(base + idx.astype(np.uint64) * 8)
        vals.append(arr[idx])
A = np.concatenate(addrs)
V = np.concatenate(vals)
order = np.argsort(V)
A, V = A[order], V[order]
print(f"{len(A)} punteros indexados")


def holders(obj):
    """Qwords cuyo valor cae en [obj-window, obj] -> (direccion_del_qword, offset)"""
    i0 = np.searchsorted(V, obj - window, side="left")
    i1 = np.searchsorted(V, obj, side="right")
    return [(int(A[i]), obj - int(V[i])) for i in range(i0, i1)]


found = []
frontier = [(target, [])]
seen = {target}
for d in range(depth):
    nxt = []
    for obj, path in frontier:
        for addr, off in holders(obj):
            p = [off] + path
            if mbase <= addr < mbase + msize:
                found.append((addr - mbase, p))
            elif addr not in seen and len(nxt) < 4000:
                seen.add(addr)
                nxt.append((addr, p))
    print(f"nivel {d+1}: {len(nxt)} nodos, {len(found)} rutas estaticas")
    frontier = nxt
    if len(found) >= 15:
        break

for rva, p in found[:30]:
    print(f"client.exe+0x{rva:x} -> " + " -> ".join(f"+0x{o:x}" for o in p))
