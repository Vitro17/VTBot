p = r"c:\Users\SuperUsuario\Documents\Test\VTBot\src\dll\ui.cpp"
s = open(p, encoding="utf-8").read()
a = s.index("static void TabTargeting() {")
b = s.index("static void TabPlaceholder(")
new = open(r"c:\Users\SuperUsuario\Documents\Test\VTBot\tools\newtab.txt", encoding="utf-8").read()
s = s[:a] + new + s[b:]
s = s.replace("int x = 100, y = 100, w = 420, h = 440;", "int x = 100, y = 100, w = 580, h = 640;")
open(p, "w", encoding="utf-8").write(s)
print("ok")
