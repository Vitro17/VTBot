#pragma once
#include <cstdint>

namespace vt::game {
struct PlayerState {
    uint32_t hp = 0, maxHp = 0, mana = 0, maxMana = 0;
    int32_t x = 0, y = 0, z = 0;   // pendiente de descubrir
};

// Localiza (y cachea) el objeto de stats del jugador por su vtable. Devuelve 0 si no esta en juego.
uintptr_t FindPlayerObject(bool forceRescan = false);
bool ReadPlayer(PlayerState& out);

// Mascara de condiciones del jugador (TPlayerData+0x94). Bits conocidos: kCondHaste. false si no se localiza.
constexpr uint32_t kCondHaste = 0x40;
bool ReadConditions(uint32_t& out);

// Segundos de comida restantes (TPlayerData+0x88). 0 = hambre (aparece el icono del tenedor). false si no se localiza.
bool ReadRegenSeconds(uint32_t& out);
}
