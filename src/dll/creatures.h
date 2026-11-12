#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Lectura de criaturas desde el TCreatureStorage del cliente (std::map<id, shared_ptr<TCreature>>).
namespace vt::creatures {

struct Creature {
    uint32_t id = 0;
    int x = 0, y = 0, z = 0;
    int hp = 0;              // porcentaje de vida 0..100 (0 = muerta)
    std::string name;

    // Tipo segun el rango del id: 1 = jugador propio/otros jugadores, 5 = monstruo, 8 = NPC
    int Kind() const { return (int)(id >> 28); }
    bool IsMonster() const { return Kind() == 5; }
};

struct View {
    Creature self;
    uint32_t targetId = 0;           // objetivo atacado actualmente (0 = ninguno)
    std::vector<Creature> all;       // todas las del almacen (incluye antiguas: filtrar por z y distancia)
};

// Lee el estado actual. false si no se encuentra el almacen (no estas en juego).
bool Snapshot(View& out);

// Direccion del almacen vigente (0 si no se encuentra). Sirve para validar objetos que dependen de el.
uintptr_t StorageAddr();

inline int Dist(const Creature& a, const Creature& b) {
    int dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    int dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;   // distancia de Chebyshev (casillas)
}
}
