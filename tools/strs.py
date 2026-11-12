"""Busca strings ASCII en client.exe (archivo).  Uso: python strs.py <regex> [max=60]"""
import re, sys
pat = re.compile(sys.argv[1].encode(), re.I)
mx = int(sys.argv[2]) if len(sys.argv) > 2 else 60
data = open(r"C:\Users\SuperUsuario\Pictures\Moduk\Modukot\bin\client.exe", "rb").read()
seen = set()
for m in re.finditer(rb"[\x20-\x7e]{4,160}", data):
    s = m.group()
    if pat.search(s) and s not in seen:
        seen.add(s)
        print(f"0x{m.start():x}: {s.decode()}")
        if len(seen) >= mx:
            break
