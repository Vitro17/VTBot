#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>
#include <string>

namespace vt::mem {
uintptr_t ModuleBase(const wchar_t* name = nullptr);   // nullptr = client.exe
size_t    ModuleSize(const wchar_t* name = nullptr);
bool      SafeRead(uintptr_t addr, void* out, size_t size);

template <typename T>
T Read(uintptr_t addr, T def = T{}) {
    T v = def;
    SafeRead(addr, &v, sizeof(T));
    return v;
}

// Busca en el heap privado objetos cuyo primer qword == vtable (direccion absoluta). Devuelve hasta maxHits.
std::vector<uintptr_t> FindByVtable(uintptr_t vtable, size_t maxHits = 16);

// Recorre las regiones privadas RW del proceso (heap). cb(base, datos, tamano) devuelve false para detener.
void ScanPrivate(const std::function<bool(uintptr_t, const uint8_t*, size_t)>& cb);

// Un QObject real cumple: obj[0] == vtable, obj[8] = d_ptr, d_ptr[8] = q_ptr == obj. Descarta copias viejas
// del puntero a vtable (pilas, memoria liberada) que provocarian un crash.
bool IsLiveQObject(uintptr_t obj, uintptr_t vtable);
bool LooksLikeStack(uintptr_t addr);

// Escaneo de patron IDA-style: "48 8B 05 ?? ?? ?? ??"
uintptr_t PatternScan(const std::string& pattern, const wchar_t* module = nullptr);
}
