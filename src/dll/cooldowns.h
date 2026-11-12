#pragma once
#include <cstdint>
#include <vector>

// Cooldowns reales de hechizos, leidos del TCooldownStorage del cliente.
namespace vt::cooldowns {

struct Entry {
    int id = 0;               // ID del hechizo
    int64_t remainingMs = 0;  // tiempo restante (>0)
};

// Lista de cooldowns activos. false si no se encuentra el almacen (no hay lectura fiable).
bool Active(std::vector<Entry>& out);

// Milisegundos restantes del hechizo `id` en una lista ya leida (0 = libre).
int64_t Remaining(const std::vector<Entry>& list, int id);
}
