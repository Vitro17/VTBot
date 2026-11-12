#include "shooter.h"
#include "config.h"
#include "creatures.h"
#include "actions.h"
#include "cooldowns.h"
#include "spells.h"
#include "log.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace vt::shooter {

using creatures::Creature;

// Area visible en pantalla: el cliente muestra 15x11 casillas alrededor del jugador.
constexpr int kViewX = 7;
constexpr int kViewY = 5;

static std::mutex g_mx;
static Status g_status;

static DWORD g_lastRead = 0;
static DWORD g_lastAttackSent = 0;
static uint32_t g_lastAttackId = 0;
static DWORD g_lastShot = 0;
static DWORD g_lastFire[kShooterSpells] = {};
static std::unordered_map<uint32_t, DWORD> g_blacklist;   // id -> hasta cuando (GetTickCount)

// Palabras de cada regla estables durante >= 1.5 s antes de buscar su ID (evita buscar por cada tecla)
static std::string g_wordsSeen[kShooterSpells];
static DWORD g_wordsSince[kShooterSpells] = {};

static std::string Lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::vector<std::string> SplitNames(const std::string& csv) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        size_t a = cur.find_first_not_of(" \t"), b = cur.find_last_not_of(" \t");
        if (a != std::string::npos) out.push_back(Lower(cur.substr(a, b - a + 1)));
        cur.clear();
    };
    for (char c : csv) { if (c == ',') flush(); else cur += c; }
    flush();
    return out;
}

static bool In(const std::vector<std::string>& v, const std::string& lname) {
    return std::find(v.begin(), v.end(), lname) != v.end();
}

Status GetStatus() {
    std::lock_guard<std::mutex> l(g_mx);
    return g_status;
}

// Monstruos vivos, en el mismo piso, en pantalla, dentro de maxDistance y que pasan los filtros de nombre.
static std::vector<const Creature*> Candidates(const creatures::View& v, int maxDist,
                                               const std::vector<std::string>& only,
                                               const std::vector<std::string>& ignore) {
    std::vector<const Creature*> out;
    for (const auto& c : v.all) {
        if (!c.IsMonster() || c.hp <= 0 || c.z != v.self.z) continue;
        int dx = std::abs(c.x - v.self.x), dy = std::abs(c.y - v.self.y);
        if (dx > kViewX || dy > kViewY || creatures::Dist(c, v.self) > maxDist) continue;
        std::string ln = Lower(c.name);
        if (!only.empty() && !In(only, ln)) continue;
        if (In(ignore, ln)) continue;
        out.push_back(&c);
    }
    std::sort(out.begin(), out.end(), [&](const Creature* a, const Creature* b) {
        int da = creatures::Dist(*a, v.self), db = creatures::Dist(*b, v.self);
        return da != db ? da < db : a->hp < b->hp;
    });
    return out;
}

static const Creature* Find(const creatures::View& v, uint32_t id) {
    if (!id) return nullptr;
    for (const auto& c : v.all) if (c.id == id) return &c;
    return nullptr;
}

bool AttackNearest() {
    creatures::View v;
    if (!creatures::Snapshot(v)) return false;
    auto& s = GetConfig().shooter;
    auto cand = Candidates(v, s.maxDistance, SplitNames(s.onlyNames.Get()), SplitNames(s.ignoreNames.Get()));
    if (cand.empty()) { Log("shooter: prueba de ataque -> no hay monstruos cercanos"); return false; }
    Log("shooter: prueba de ataque -> %s (0x%x)", cand[0]->name.c_str(), cand[0]->id);
    return actions::Attack(cand[0]->id);
}

void Tick() {
    DWORD now = GetTickCount();
    if (now - g_lastRead < 200) return;
    g_lastRead = now;

    auto& cfg = GetConfig();
    auto& st = GetState();
    auto& sh = cfg.shooter;

    creatures::View v;
    if (!st.inGame || !creatures::Snapshot(v)) {
        std::lock_guard<std::mutex> l(g_mx);
        g_status = Status{};
        return;
    }

    auto only = SplitNames(sh.onlyNames.Get());
    auto ignore = SplitNames(sh.ignoreNames.Get());
    int maxDist = sh.maxDistance;
    auto cand = Candidates(v, maxDist, only, ignore);

    // limpiar lista negra caducada
    for (auto it = g_blacklist.begin(); it != g_blacklist.end();)
        it = ((int)(now - it->second) >= 0) ? g_blacklist.erase(it) : std::next(it);   // caducada si now >= hasta

    // ---- cooldowns reales de las reglas ----
    std::vector<cooldowns::Entry> cdList;
    const bool cdOk = cooldowns::Active(cdList);
    int ruleId[kShooterSpells] = {};
    std::string ruleWords[kShooterSpells];
    for (int i = 0; i < kShooterSpells; ++i) {
        ruleWords[i] = sh.spells[i].words.Get();
        if (ruleWords[i] != g_wordsSeen[i]) { g_wordsSeen[i] = ruleWords[i]; g_wordsSince[i] = now; }
        if (sh.spells[i].enabled && !ruleWords[i].empty() && now - g_wordsSince[i] >= 1500)
            ruleId[i] = spells::IdForWords(ruleWords[i]);
    }

    // estado para la UI
    const Creature* tgt = Find(v, v.targetId);
    {
        Status s;
        for (int i = 0; i < kShooterSpells; ++i) {
            s.ruleId.push_back(ruleId[i]);
            s.ruleCdMs.push_back(cdOk && ruleId[i] ? (int)cooldowns::Remaining(cdList, ruleId[i]) : -1);
        }
        s.ok = true;
        s.x = v.self.x; s.y = v.self.y; s.z = v.self.z;
        if (tgt) { s.targetName = tgt->name; s.targetHp = tgt->hp; }
        s.monstersNear = (int)cand.size();
        for (size_t i = 0; i < cand.size() && i < 12; ++i)
            s.list.push_back({cand[i]->name, cand[i]->hp, creatures::Dist(*cand[i], v.self),
                              cand[i]->id == v.targetId});
        std::lock_guard<std::mutex> l(g_mx);
        g_status = std::move(s);
    }

    if (!cfg.active || !sh.enabled) { g_lastAttackId = 0; return; }

    // ---- targeting ----
    bool needNew = v.targetId == 0 || !tgt || tgt->hp <= 0;
    if (sh.autoTarget) {
        // el ataque anterior no tomo efecto (criatura no visible, etc.): a la lista negra unos segundos
        if (g_lastAttackId && now - g_lastAttackSent > 1500 && v.targetId != g_lastAttackId) {
            Log("shooter: 0x%x no se pudo atacar; ignorado 10 s", g_lastAttackId);
            g_blacklist[g_lastAttackId] = now + 10000;
            g_lastAttackId = 0;
        }
        if (v.targetId == g_lastAttackId) g_lastAttackId = 0;   // ya es el objetivo

        if (needNew && now - g_lastAttackSent >= 800) {
            for (const Creature* c : cand) {
                if (g_blacklist.count(c->id)) continue;
                if (actions::Attack(c->id)) {
                    g_lastAttackSent = now;
                    g_lastAttackId = c->id;
                    Log("shooter: objetivo -> %s (%d%%, dist %d)", c->name.c_str(), c->hp,
                        creatures::Dist(*c, v.self));
                }
                break;
            }
        }
    }

    // ---- magic shooter ----
    bool haveTarget = tgt && tgt->hp > 0 && tgt->z == v.self.z && creatures::Dist(*tgt, v.self) <= maxDist;
    if (sh.requireTarget ? !haveTarget : cand.empty()) return;
    if (now - g_lastShot < 250) return;

    for (int i = 0; i < kShooterSpells; ++i) {
        auto& r = sh.spells[i];
        if (!r.enabled) continue;
        const std::string& words = ruleWords[i];
        if (words.empty()) continue;
        if (st.mana < (uint32_t)r.minMana) continue;
        if (cdOk && ruleId[i]) {
            // cooldown REAL del hechizo; 600 ms de guarda hasta que el servidor confirme el cast
            if (cooldowns::Remaining(cdList, ruleId[i]) > 0) continue;
            if (now - g_lastFire[i] < 600) continue;
        } else if (now - g_lastFire[i] < (DWORD)r.cooldownMs) {
            continue;   // ID aun desconocido: cooldown manual de la regla
        }
        int range = r.range, n = 0;
        for (const Creature* c : cand) if (creatures::Dist(*c, v.self) <= range) ++n;
        if (n < r.minMonsters) continue;
        if (actions::Say(words)) {
            g_lastFire[i] = now;
            g_lastShot = now;
            Log("shooter: '%s' (%d monstruos a <=%d)", words.c_str(), n, range);
        }
        break;
    }
}
}
