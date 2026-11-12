#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace vt {

enum class ActionKind : int {
    Spell = 0,   // lanzar un hechizo por sus palabras (p.ej. "exura vita")
    Item  = 1,   // usar un item sobre uno mismo por su ID (p.ej. pocion)
};

// Regla de curacion estilo ZeroBot: cuando <stat>% <= pct ejecuta la accion (sin hotkeys).
struct HealRule {
    std::atomic<bool> enabled{false};
    std::atomic<int>  pct{70};
    std::atomic<int>  kind{(int)ActionKind::Spell};
    std::atomic<int>  itemId{0};
    std::atomic<int>  minMana{0};        // mana minima necesaria (solo hechizos)
    std::atomic<int>  cooldownMs{1000};

    std::string Words() const { std::lock_guard<std::mutex> l(m); return words; }
    void SetWords(const std::string& w) { std::lock_guard<std::mutex> l(m); words = w; }

private:
    mutable std::mutex m;
    std::string words;
};

// Cadena protegida por mutex (la UI y el hilo del bot la comparten).
class SyncString {
public:
    std::string Get() const { std::lock_guard<std::mutex> l(m); return s; }
    void Set(const std::string& v) { std::lock_guard<std::mutex> l(m); s = v; }
private:
    mutable std::mutex m;
    std::string s;
};

// Regla del magic shooter: lanza `words` mientras haya objetivo y >= minMonsters monstruos a <= range casillas.
struct ShooterSpell {
    std::atomic<bool> enabled{false};
    std::atomic<int>  minMana{0};
    std::atomic<int>  cooldownMs{2000};
    std::atomic<int>  minMonsters{1};
    std::atomic<int>  range{6};
    SyncString words;
};

constexpr int kShooterSpells = 4;

struct Shooter {
    std::atomic<bool> enabled{false};         // magic shooter (hechizos de ataque)
    std::atomic<bool> autoTarget{true};       // atacar el monstruo mas cercano cuando no hay objetivo
    std::atomic<bool> requireTarget{true};    // solo lanzar hechizos con un objetivo seleccionado
    std::atomic<int>  maxDistance{7};         // distancia maxima (casillas) para considerar un monstruo
    SyncString        onlyNames;              // si no esta vacio: solo estos monstruos (separados por coma)
    SyncString        ignoreNames;            // monstruos a ignorar (separados por coma)
    ShooterSpell      spells[kShooterSpells]; // por prioridad: la primera disponible se lanza
};

// Herramientas varias.
struct Tools {
    // Comer: usa `eatItem` cuando hay hambre (o cada eatIntervalSec si no se conoce la bandera de hambre).
    std::atomic<bool> eat{false};
    std::atomic<int>  eatItem{3577};        // ID del item de comida (meat por defecto)
    std::atomic<int>  eatKind{0};           // tipo de uso (campo kind de la accion)
    std::atomic<int>  eatIntervalSec{120};  // modo por tiempo (si eatOnHunger = false)
    std::atomic<bool> eatOnHunger{true};    // comer solo cuando hay hambre (tiempo de comida restante <= eatHungerSec)
    std::atomic<int>  eatTimes{2};          // veces que come seguidas al tener hambre
    std::atomic<int>  eatGapMs{1500};       // pausa entre una comida y la siguiente
    std::atomic<int>  eatHungerSec{0};      // umbral: segundos de comida restantes (0 = solo con el icono de hambre)
    // Convertir monedas: usa gold (3031) y platinum (3035) para subirlas de denominacion.
    std::atomic<bool> coins{false};
    std::atomic<int>  coinsKind{0};
    std::atomic<int>  coinsIntervalSec{3};
    std::atomic<bool> coinsGold{true};
    std::atomic<bool> coinsPlat{true};
    // Anti AFK: gira el personaje cada afkIntervalSec.
    std::atomic<bool> antiAfk{false};
    std::atomic<int>  afkIntervalSec{240};
    // Condiciones: auto haste (lanza `hasteWords` cuando el jugador no tiene haste).
    std::atomic<bool> haste{false};
    std::atomic<int>  hasteMinMana{60};
    std::atomic<int>  hasteRetryMs{2500};   // minimo entre intentos (el hechizo puede estar en cooldown)
    SyncString        hasteWords;
    // Boton "VT" dentro del cliente (oculta/muestra el HUD). Posicion: distancia al borde derecho y al superior.
    std::atomic<bool> hudButton{true};
    std::atomic<int>  btnRight{9};
    std::atomic<int>  btnTop{215};
    // Look: muestra el ID del item en un mensaje blanco en medio de la pantalla.
    std::atomic<bool> lookId{true};
    std::atomic<int>  lookIdMs{3000};       // duracion del mensaje
    std::atomic<int>  lookDx{-141};         // desplazamiento (px) respecto al centro de la ventana del cliente
    std::atomic<int>  lookDy{0};
    // Entrenar con dummy: usa el arma de ejercicio sobre el dummy.
    std::atomic<bool> dummy{false};
    std::atomic<int>  dummyItem{0};         // ID del arma de ejercicio
    std::atomic<int>  dummyIntervalSec{2};  // pausa entre usos (el arma tiene ~2 s de espera)
    std::atomic<int>  dummyX{0}, dummyY{0}, dummyZ{0};   // casilla del dummy (coordenadas absolutas)
    std::atomic<int>  dummyStack{1};        // posicion del dummy en la pila de la casilla
    std::atomic<int>  dummyTarget{0};       // ID del objeto dummy
};

struct Config {
    Tools    tools;                    // comer, monedas, anti AFK, dummy
    std::atomic<bool> active{false};   // interruptor general del bot
    HealRule healSpell;                // curacion principal (por HP%)
    HealRule healPotion;               // pocion de vida (por HP%)
    HealRule manaPotion;               // pocion de mana (por mana%)
    Shooter  shooter;                  // targeting + magic shooter
};

// Estado del juego compartido: lo escribe el hilo del bot, lo lee la UI
struct SharedState {
    std::atomic<uint32_t> hp{0}, maxHp{0}, mana{0}, maxMana{0};
    std::atomic<bool> inGame{false};
};

Config&      GetConfig();
SharedState& GetState();
std::atomic<bool>& UnloadRequested();   // la UI (o el loader) lo activa para descargar el bot

void LoadConfig();   // vtbot_config.ini junto a la DLL
void SaveConfig();
}
