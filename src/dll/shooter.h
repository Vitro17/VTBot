#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Targeting + magic shooter: elige objetivos entre los monstruos cercanos y lanza hechizos de ataque.
namespace vt::shooter {

struct NearMonster {
    std::string name;
    int hp = 0;
    int dist = 0;
    bool isTarget = false;
};

struct Status {
    bool ok = false;            // se leyeron criaturas
    int x = 0, y = 0, z = 0;    // posicion propia
    std::string targetName;     // vacio = sin objetivo
    int targetHp = 0;
    int monstersNear = 0;       // monstruos que pasan los filtros
    std::vector<NearMonster> list;   // ordenados por distancia
    // Estado de cada regla de hechizo: id conocido (0 = aun no) y cooldown restante en ms
    // (-1 = sin lectura de cooldowns reales; se usa el CD manual)
    std::vector<int> ruleId;
    std::vector<int> ruleCdMs;
};

void Tick();            // llamar periodicamente desde el hilo del bot
Status GetStatus();     // copia segura para la UI
bool AttackNearest();   // fuerza atacar el monstruo mas cercano (boton de prueba)
}
