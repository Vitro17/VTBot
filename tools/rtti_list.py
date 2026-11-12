"""Lista nombres RTTI de client.exe.  Uso: python rtti_list.py <regex_sobre_nombre_clase>"""
import re, sys
pat = re.compile(sys.argv[1], re.I)
d = open(r"C:\Users\SuperUsuario\Pictures\Moduk\Modukot\bin\client.exe", "rb").read()
names = sorted(set(m.group(1).decode() for m in re.finditer(rb"\.\?AV([A-Za-z0-9_@?$<>]{3,200}?)@@", d)))
out = [n for n in names if pat.search(n)]
print(len(out))
for n in out:
    print(n)
