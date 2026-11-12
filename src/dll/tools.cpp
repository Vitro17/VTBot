#include "tools.h"
#include "config.h"
#include "actions.h"
#include "log.h"
#include "game.h"
#include <windows.h>
#include <atomic>

namespace vt::tools {


static DWORD g_lastEat = 0, g_lastCoins = 0, g_lastAfk = 0, g_lastDummy = 0;
static bool g_everEat = false, g_everCoins = false, g_everAfk = false, g_everDummy = false;
static DWORD g_nextCheck = 0;
static std::atomic<uint32_t> g_cond{0};
static std::atomic<bool> g_condKnown{false};
static DWORD g_lastHaste = 0;
static bool g_everHaste = false;
static std::atomic<uint32_t> g_regen{0};
static std::atomic<bool> g_regenKnown{false};
static int g_eatPending = 0;
static DWORD g_nextEatAt = 0, g_eatLockUntil = 0;

static bool Due(bool ever, DWORD last, DWORD now, int sec) {
    return !ever || (now - last) >= (DWORD)(sec < 1 ? 1 : sec) * 1000u;
}

static uint32_t Ago(bool ever, DWORD last, DWORD now) { return ever ? (now - last) / 1000u : 0xFFFFFFFFu; }

void Tick() {
    auto& cfg = GetConfig();
    auto& st = GetState();
    if (!st.inGame) return;
    DWORD now = GetTickCount();
    if ((int)(now - g_nextCheck) < 0) return;
    g_nextCheck = now + 500;
    auto& t = cfg.tools;

    // Condiciones del jugador (se leen aunque el bot este apagado, para mostrarlas en la UI).
    uint32_t cond = 0;
    bool condKnown = game::ReadConditions(cond);
    g_condKnown = condKnown;
    g_cond = cond;
    uint32_t regen = 0;
    bool regenKnown = game::ReadRegenSeconds(regen);
    g_regenKnown = regenKnown;
    g_regen = regen;
    if (!cfg.active) return;

    // --- Auto haste: si no hay haste, lanzar el hechizo (reintento limitado por hasteRetryMs) ---
    if (t.haste && condKnown && !(cond & game::kCondHaste) && st.mana >= (uint32_t)t.hasteMinMana &&
        (now - g_lastHaste) >= (DWORD)(t.hasteRetryMs < 500 ? 500 : t.hasteRetryMs.load())) {
        auto words = t.hasteWords.Get();
        if (!words.empty()) {
            bool ok = actions::Say(words);
            g_lastHaste = now; g_everHaste = true;
            Log("[haste] sin haste (estados 0x%x) -> '%s' %s", cond, words.c_str(), ok ? "encolado" : "FALLO");
        }
    }

    // --- Comer ---
    // Modo hambre: cuando el tiempo de comida restante (TPlayerData+0x88) es <= eatHungerSec (0 = el icono de
    // hambre esta visible) come `eatTimes` veces seguidas, separadas por eatGapMs. Tras la tanda espera unos
    // segundos a que el cliente actualice el contador antes de volver a evaluar.
    if (t.eat && t.eatItem > 0) {
        if (t.eatOnHunger) {
            if (g_eatPending == 0 && regenKnown && regen <= (uint32_t)t.eatHungerSec &&
                (int)(now - g_eatLockUntil) >= 0) {
                int n = t.eatTimes; if (n < 1) n = 1; if (n > 5) n = 5;
                g_eatPending = n; g_nextEatAt = now;
                Log("[comer] hambre (comida restante %us) -> comer %d veces", regen, n);
            }
            if (g_eatPending > 0 && (int)(now - g_nextEatAt) >= 0) {
                bool ok = actions::UseItem((uint32_t)t.eatItem.load(), t.eatKind);
                g_lastEat = now; g_everEat = true;
                Log("[comer] item %d (kind %d) -> %s", t.eatItem.load(), t.eatKind.load(), ok ? "encolado" : "FALLO");
                if (!ok) g_eatPending = 0; else --g_eatPending;
                g_nextEatAt = now + (DWORD)(t.eatGapMs < 500 ? 500 : t.eatGapMs.load());
                if (g_eatPending == 0) g_eatLockUntil = now + 8000;
            }
        } else if (Due(g_everEat, g_lastEat, now, t.eatIntervalSec)) {
            actions::UseItem((uint32_t)t.eatItem.load(), t.eatKind);
            g_lastEat = now; g_everEat = true;
            Log("[comer] item %d (kind %d) por tiempo", t.eatItem.load(), t.eatKind.load());
        }
    }
    // --- Convertir monedas: click derecho > Usar sobre un stack de 100 visible en un contenedor ---
    if (t.coins && Due(g_everCoins, g_lastCoins, now, t.coinsIntervalSec)) {
        actions::ConvertCoins(t.coinsGold, t.coinsPlat);   // si no hay stack de 100 no hace nada
        g_lastCoins = now; g_everCoins = true;
    }

    // --- Anti AFK: girar el personaje a una direccion distinta de la ultima ---
    if (t.antiAfk && Due(g_everAfk, g_lastAfk, now, t.afkIntervalSec)) {
        TurnRandom();
        g_lastAfk = now; g_everAfk = true;
    }

    // --- Dummy: arma de ejercicio (en un contenedor abierto) usada sobre la casilla del dummy ---
    if (t.dummy && t.dummyItem > 0 && t.dummyX > 0 && t.dummyTarget > 0 &&
        Due(g_everDummy, g_lastDummy, now, t.dummyIntervalSec)) {
        actions::UseItemOnMap((uint32_t)t.dummyItem.load(), t.dummyX, t.dummyY, t.dummyZ, (uint32_t)t.dummyStack.load(),
                              (uint32_t)t.dummyTarget.load());
        g_lastDummy = now; g_everDummy = true;
    }
}

static int g_lastDir = -1;

bool TurnRandom() {
    int d = rand() % 3;            // 0..2 -> se salta la ultima direccion
    if (g_lastDir >= 0 && d >= g_lastDir) ++d;
    bool ok = actions::Turn(d);
    if (ok) g_lastDir = d;
    Log("[antiafk] giro %d -> %s", d, ok ? "encolado" : "FALLO");
    return ok;
}

bool TurnDir(int dir) {
    bool ok = actions::Turn(dir);
    if (ok) g_lastDir = dir;
    Log("[antiafk] giro manual %d -> %s", dir, ok ? "encolado" : "FALLO");
    return ok;
}

Status GetStatus() {
    Status s;
    DWORD now = GetTickCount();
    s.condKnown = g_condKnown;
    s.regenKnown = g_regenKnown;
    s.regenSec = g_regen;
    s.conditions = g_cond;
    s.lastHasteAgoSec = Ago(g_everHaste, g_lastHaste, now);
    s.lastEatAgoSec = Ago(g_everEat, g_lastEat, now);
    s.lastCoinsAgoSec = Ago(g_everCoins, g_lastCoins, now);
    s.lastAfkAgoSec = Ago(g_everAfk, g_lastAfk, now);
    s.lastDummyAgoSec = Ago(g_everDummy, g_lastDummy, now);
    return s;
}
}
