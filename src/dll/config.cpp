#include "config.h"
#include "log.h"
#include <windows.h>
#include <string>

namespace vt {
static Config g_cfg;
static SharedState g_state;
static std::atomic<bool> g_unload{false};

Config& GetConfig() { return g_cfg; }
SharedState& GetState() { return g_state; }
std::atomic<bool>& UnloadRequested() { return g_unload; }

static std::wstring IniPath() {
    wchar_t path[MAX_PATH];
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&IniPath, &self);
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring p(path);
    return p.substr(0, p.find_last_of(L'\\') + 1) + L"vtbot_config.ini";
}

static std::wstring ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}
static std::string FromW(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

static void LoadRule(const std::wstring& ini, const wchar_t* sec, HealRule& r, int defPct, int defKind,
                     const char* defWords) {
    r.enabled = GetPrivateProfileIntW(sec, L"enabled", 0, ini.c_str()) != 0;
    r.pct = GetPrivateProfileIntW(sec, L"pct", defPct, ini.c_str());
    r.kind = GetPrivateProfileIntW(sec, L"kind", defKind, ini.c_str());
    r.itemId = GetPrivateProfileIntW(sec, L"itemId", 0, ini.c_str());
    r.minMana = GetPrivateProfileIntW(sec, L"minMana", 0, ini.c_str());
    r.cooldownMs = GetPrivateProfileIntW(sec, L"cooldownMs", 1000, ini.c_str());
    wchar_t buf[128] = {};
    GetPrivateProfileStringW(sec, L"words", ToW(defWords).c_str(), buf, 128, ini.c_str());
    r.SetWords(FromW(buf));
}

static void SaveRule(const std::wstring& ini, const wchar_t* sec, HealRule& r) {
    auto put = [&](const wchar_t* k, int v) {
        WritePrivateProfileStringW(sec, k, std::to_wstring(v).c_str(), ini.c_str());
    };
    put(L"enabled", r.enabled ? 1 : 0);
    put(L"pct", r.pct);
    put(L"kind", r.kind);
    put(L"itemId", r.itemId);
    put(L"minMana", r.minMana);
    put(L"cooldownMs", r.cooldownMs);
    WritePrivateProfileStringW(sec, L"words", ToW(r.Words()).c_str(), ini.c_str());
}

static std::string GetStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key) {
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(sec, key, L"", buf, 512, ini.c_str());
    return FromW(buf);
}
static void PutStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, const std::string& v) {
    WritePrivateProfileStringW(sec, key, ToW(v).c_str(), ini.c_str());
}
static void PutInt(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int v) {
    WritePrivateProfileStringW(sec, key, std::to_wstring(v).c_str(), ini.c_str());
}

static void LoadShooter(const std::wstring& ini) {
    auto& s = g_cfg.shooter;
    const wchar_t* sec = L"shooter";
    // `enabled` no se persiste (arranca apagado por seguridad), igual que el interruptor general.
    s.autoTarget = GetPrivateProfileIntW(sec, L"autoTarget", 1, ini.c_str()) != 0;
    s.requireTarget = GetPrivateProfileIntW(sec, L"requireTarget", 1, ini.c_str()) != 0;
    s.maxDistance = GetPrivateProfileIntW(sec, L"maxDistance", 7, ini.c_str());
    s.onlyNames.Set(GetStr(ini, sec, L"onlyNames"));
    s.ignoreNames.Set(GetStr(ini, sec, L"ignoreNames"));
    for (int i = 0; i < kShooterSpells; ++i) {
        std::wstring name = L"shooterSpell" + std::to_wstring(i);
        auto& r = s.spells[i];
        r.enabled = GetPrivateProfileIntW(name.c_str(), L"enabled", 0, ini.c_str()) != 0;
        r.minMana = GetPrivateProfileIntW(name.c_str(), L"minMana", 0, ini.c_str());
        r.cooldownMs = GetPrivateProfileIntW(name.c_str(), L"cooldownMs", 2000, ini.c_str());
        r.minMonsters = GetPrivateProfileIntW(name.c_str(), L"minMonsters", 1, ini.c_str());
        r.range = GetPrivateProfileIntW(name.c_str(), L"range", 6, ini.c_str());
        r.words.Set(GetStr(ini, name.c_str(), L"words"));
    }
}

static void SaveShooter(const std::wstring& ini) {
    auto& s = g_cfg.shooter;
    const wchar_t* sec = L"shooter";
    PutInt(ini, sec, L"autoTarget", s.autoTarget ? 1 : 0);
    PutInt(ini, sec, L"requireTarget", s.requireTarget ? 1 : 0);
    PutInt(ini, sec, L"maxDistance", s.maxDistance);
    PutStr(ini, sec, L"onlyNames", s.onlyNames.Get());
    PutStr(ini, sec, L"ignoreNames", s.ignoreNames.Get());
    for (int i = 0; i < kShooterSpells; ++i) {
        std::wstring name = L"shooterSpell" + std::to_wstring(i);
        auto& r = s.spells[i];
        PutInt(ini, name.c_str(), L"enabled", r.enabled ? 1 : 0);
        PutInt(ini, name.c_str(), L"minMana", r.minMana);
        PutInt(ini, name.c_str(), L"cooldownMs", r.cooldownMs);
        PutInt(ini, name.c_str(), L"minMonsters", r.minMonsters);
        PutInt(ini, name.c_str(), L"range", r.range);
        PutStr(ini, name.c_str(), L"words", r.words.Get());
    }
}

// Los interruptores `enabled` de las herramientas no se persisten (arrancan apagados).
static void LoadTools(const std::wstring& ini) {
    auto& t = g_cfg.tools;
    auto I = [&](const wchar_t* k, int d) { return (int)GetPrivateProfileIntW(L"tools", k, d, ini.c_str()); };
    t.eatItem = I(L"eatItem", 3577);
    t.eatKind = I(L"eatKind", 0);
    t.eatIntervalSec = I(L"eatIntervalSec", 120);
    t.eatOnHunger = I(L"eatOnHunger", 1) != 0;
    t.eatTimes = I(L"eatTimes", 2);
    t.eatGapMs = I(L"eatGapMs", 1500);
    t.eatHungerSec = I(L"eatHungerSec", 0);
    t.coinsKind = I(L"coinsKind", 0);
    t.coinsIntervalSec = I(L"coinsIntervalSec", 3);
    t.coinsGold = I(L"coinsGold", 1) != 0;
    t.coinsPlat = I(L"coinsPlat", 1) != 0;
    t.afkIntervalSec = I(L"afkIntervalSec", 240);
    t.hasteMinMana = I(L"hasteMinMana", 60);
    t.hasteRetryMs = I(L"hasteRetryMs", 2500);
    {
        std::string w = GetStr(ini, L"tools", L"hasteWords");
        t.hasteWords.Set(w.empty() ? "utani hur" : w);
    }
    t.hudButton = I(L"hudButton", 1) != 0;
    t.btnRight = I(L"btnRight", 9);
    t.btnTop = I(L"btnTop", 215);
    t.lookId = I(L"lookId", 1) != 0;
    t.lookIdMs = I(L"lookIdMs", 3000);
    t.lookDx = I(L"lookDx", -141);
    t.lookDy = I(L"lookDy", 0);
    t.dummyItem = I(L"dummyItem", 0);
    t.dummyIntervalSec = I(L"dummyIntervalSec", 2);
    t.dummyX = I(L"dummyX", 0); t.dummyY = I(L"dummyY", 0); t.dummyZ = I(L"dummyZ", 0);
    t.dummyStack = I(L"dummyStack", 1); t.dummyTarget = I(L"dummyTarget", 0);
}

static void SaveTools(const std::wstring& ini) {
    auto& t = g_cfg.tools;
    auto P = [&](const wchar_t* k, int v) { PutInt(ini, L"tools", k, v); };
    P(L"eatItem", t.eatItem); P(L"eatKind", t.eatKind); P(L"eatIntervalSec", t.eatIntervalSec);
    P(L"eatOnHunger", t.eatOnHunger ? 1 : 0); P(L"eatTimes", t.eatTimes); P(L"eatGapMs", t.eatGapMs); P(L"eatHungerSec", t.eatHungerSec);
    P(L"coinsKind", t.coinsKind); P(L"coinsIntervalSec", t.coinsIntervalSec);
    P(L"coinsGold", t.coinsGold ? 1 : 0); P(L"coinsPlat", t.coinsPlat ? 1 : 0);
    P(L"afkIntervalSec", t.afkIntervalSec);
    P(L"hasteMinMana", t.hasteMinMana); P(L"hasteRetryMs", t.hasteRetryMs);
    PutStr(ini, L"tools", L"hasteWords", t.hasteWords.Get());
    P(L"hudButton", t.hudButton ? 1 : 0); P(L"btnRight", t.btnRight); P(L"btnTop", t.btnTop);
    P(L"lookId", t.lookId ? 1 : 0); P(L"lookIdMs", t.lookIdMs); P(L"lookDx", t.lookDx); P(L"lookDy", t.lookDy);
    P(L"dummyItem", t.dummyItem); P(L"dummyIntervalSec", t.dummyIntervalSec);
    P(L"dummyX", t.dummyX); P(L"dummyY", t.dummyY); P(L"dummyZ", t.dummyZ);
    P(L"dummyStack", t.dummyStack); P(L"dummyTarget", t.dummyTarget);
}

void LoadConfig() {
    auto ini = IniPath();
    LoadTools(ini);
    LoadShooter(ini);
    LoadRule(ini, L"healSpell", g_cfg.healSpell, 70, (int)ActionKind::Spell, "exura vita");
    LoadRule(ini, L"healPotion", g_cfg.healPotion, 40, (int)ActionKind::Item, "");
    LoadRule(ini, L"manaPotion", g_cfg.manaPotion, 30, (int)ActionKind::Item, "");
    Log("config cargada");
}

void SaveConfig() {
    auto ini = IniPath();
    SaveRule(ini, L"healSpell", g_cfg.healSpell);
    SaveRule(ini, L"healPotion", g_cfg.healPotion);
    SaveRule(ini, L"manaPotion", g_cfg.manaPotion);
    SaveShooter(ini);
    SaveTools(ini);
}
}
