#include "healer.h"
#include "config.h"
#include "actions.h"
#include "log.h"
#include <windows.h>

namespace vt::healer {

static DWORD g_lastFire[3] = {0, 0, 0};
static bool g_warnedItem[3] = {false, false, false};

static void Eval(int slot, HealRule& r, uint32_t cur, uint32_t max, uint32_t mana, const char* name) {
    if (!r.enabled || max == 0) return;
    if (cur * 100u > (uint32_t)r.pct * max) return;                 // aun por encima del umbral
    DWORD now = GetTickCount();
    if (now - g_lastFire[slot] < (DWORD)r.cooldownMs) return;      // en cooldown

    if (r.kind == (int)ActionKind::Spell) {
        auto words = r.Words();
        if (words.empty() || mana < (uint32_t)r.minMana) return;
        if (actions::Say(words)) {
            g_lastFire[slot] = now;
            Log("[%s] %u/%u -> hechizo '%s'", name, cur, max, words.c_str());
        }
    } else {
        int id = r.itemId.load();
        if (id <= 0) {
            if (!g_warnedItem[slot]) { Log("[%s] regla de item sin ID configurado", name); g_warnedItem[slot] = true; }
            return;
        }
        if (actions::UseItemOnSelf((uint32_t)id)) {
            g_lastFire[slot] = now;
            Log("[%s] %u/%u -> item %d", name, cur, max, id);
        }
    }
}

void Tick() {
    auto& cfg = GetConfig();
    auto& st = GetState();
    if (!cfg.active || !st.inGame) return;
    uint32_t hp = st.hp, maxHp = st.maxHp, mana = st.mana, maxMana = st.maxMana;
    Eval(0, cfg.healSpell, hp, maxHp, mana, "heal");
    Eval(1, cfg.healPotion, hp, maxHp, mana, "pocion HP");
    Eval(2, cfg.manaPotion, mana, maxMana, mana, "pocion MP");
}
}
