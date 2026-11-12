import re
p = r"c:\Users\SuperUsuario\Documents\Test\VTBot\src\dll\actions.cpp"
s = open(p, encoding="utf-8").read()
a = s.index("// Un QObject real cumple")
b = s.index("static uintptr_t FindChat()")
s = s[:a] + "using mem::IsLiveQObject;\nusing mem::LooksLikeStack;\n\n" + s[b:]
open(p, "w", encoding="utf-8").write(s)
print("ok", len(s))
