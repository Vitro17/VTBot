#pragma once
#include <cstdint>

// Herramientas: comer, convertir monedas, anti AFK y entrenamiento con dummy.
namespace vt::tools {

// Llamada periodica desde el bucle del bot (es barata: solo comprueba temporizadores).
void Tick();

// Gira el personaje (0=N,1=E,2=S,3=O). TurnRandom elige una direccion distinta de la ultima.
bool TurnRandom();
bool TurnDir(int dir);

// Estado para la UI.
struct Status {
    bool hungryKnown = false;   // se conoce la bandera de hambre del jugador
    bool hungry = false;
    bool regenKnown = false;     // se pudo leer el tiempo de comida restante
    uint32_t regenSec = 0;       // segundos de comida restantes (0 = hambre)
    bool condKnown = false;      // se pudo leer la mascara de condiciones
    uint32_t conditions = 0;     // mascara cruda (bit 0x40 = haste)
    uint32_t lastHasteAgoSec = 0;
    uint32_t lastEatAgoSec = 0, lastCoinsAgoSec = 0, lastAfkAgoSec = 0, lastDummyAgoSec = 0;
};
Status GetStatus();
}
