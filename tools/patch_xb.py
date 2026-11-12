import re
s = open("xb.py", encoding="utf-8").read()
old = 'struct.unpack_from("<Q", d["obj"][3], 0)[0] == base + int(_ov, 16)):'
new = 'struct.unpack_from("<Q", d["obj"][3], 0)[0] in [base + int(x, 16) for x in _ov.split(",")]):'
assert old in s
s = s.replace(old, new)
open("xb.py", "w", encoding="utf-8").write(s)
print("ok")
