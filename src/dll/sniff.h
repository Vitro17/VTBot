#pragma once
#include <cstdint>

// Observador de acciones del cliente. Parchea el slot del despachador generico (TGenericGameActionHandler) para
// ver pasar las acciones Look y Use-with ANTES de que el cliente las procese (no las modifica ni las bloquea).
// Sirve para: mostrar el ID del item al dar "Look" y para capturar el dummy (posicion + id) de un Use-with real.
namespace vt::sniff {

enum class Where : uint8_t { None = 0, World, Container, Other };

struct Seen {
    bool     valid = false;
    uint32_t seq = 0;          // se incrementa en cada captura (la UI detecta eventos nuevos)
    uint32_t itemId = 0;       // id del objeto mirado / del objeto destino
    Where    where = Where::None;
    int32_t  x = 0, y = 0, z = 0;   // si where == World
    uint32_t stack = 0;             // posicion en la pila de la casilla
    uint32_t cid = 0, slot = 0;     // si where == Container
    uint32_t tick = 0;
};

struct Info {
    bool installed = false;
    Seen look;                 // ultimo Look
    Seen useWithTarget;        // ultimo Use-with: objeto/casilla destino
    uint32_t useWithItemId = 0;     // ultimo Use-with: item usado (p.ej. arma de ejercicio)
    uint32_t looks = 0, useWiths = 0;
};

bool Install();   // idempotente; false si no se pudo parchear (offsets no validos o slot inesperado)
void Remove();    // restaura el slot original (llamar antes de descargar la DLL)
Info Get();

}
