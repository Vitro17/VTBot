#include "ui.h"
#include "config.h"
#include "log.h"
#include "actions.h"
#include "shooter.h"
#include "tools.h"
#include "containers.h"
#include "sniff.h"
#include "overlay.h"
#include "game.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace vt::ui {

static HANDLE g_thread = nullptr;
static std::atomic<bool> g_stop{false};

// --- D3D11 ---------------------------------------------------------------------------------------
static ID3D11Device*           g_dev = nullptr;
static ID3D11DeviceContext*    g_ctx = nullptr;
static IDXGISwapChain*         g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

static void CreateRTV() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) { g_dev->CreateRenderTargetView(back, nullptr, &g_rtv); back->Release(); }
}
static void ReleaseRTV() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

static bool InitD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &fl, &g_ctx);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) { Log("ui: D3D11 fallo 0x%08lx", (unsigned long)hr); return false; }
    CreateRTV();
    return true;
}

static void CleanupD3D() {
    ReleaseRTV();
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx)  { g_ctx->Release();  g_ctx = nullptr; }
    if (g_dev)  { g_dev->Release();  g_dev = nullptr; }
}

// --- Ventana -------------------------------------------------------------------------------------
static std::atomic<HWND> g_hwnd{nullptr};

void ToggleVisible() { if (HWND h = g_hwnd) PostMessageW(h, WM_APP + 1, 0, 0); }
bool IsVisible() { HWND h = g_hwnd; return h && IsWindowVisible(h) && !IsIconic(h); }

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, msg, w, l)) return TRUE;
    switch (msg) {
    case WM_SIZE:
        if (g_dev && w != SIZE_MINIMIZED) {
            ReleaseRTV();
            g_swap->ResizeBuffers(0, LOWORD(l), HIWORD(l), DXGI_FORMAT_UNKNOWN, 0);
            CreateRTV();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((w & 0xfff0) == SC_KEYMENU) return 0;
        if ((w & 0xfff0) == SC_MINIMIZE) { ShowWindow(h, SW_HIDE); return 0; }   // se recupera con el boton VT del cliente
        break;
    case WM_APP + 1:                    // alternar visibilidad (boton VT dentro del cliente)
        if (IsWindowVisible(h) && !IsIconic(h)) {
            ShowWindow(h, SW_HIDE);
        } else {
            if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
        }
        return 0;
    case WM_CLOSE:                      // la X solo oculta; para descargar usa Ajustes > Descargar
        ShowWindow(h, SW_HIDE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

struct FindCtx { DWORD pid; HWND hwnd; };
static BOOL CALLBACK EnumCb(HWND h, LPARAM lp) {
    auto* c = (FindCtx*)lp;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid != c->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    wchar_t cls[64]; GetClassNameW(h, cls, 64);
    if (wcscmp(cls, L"VTBotWnd") == 0) return TRUE;
    wchar_t title[128]; GetWindowTextW(h, title, 128);
    if (wcsncmp(title, L"Tibia", 5) == 0) { c->hwnd = h; return FALSE; }
    return TRUE;
}
static HWND FindClientWindow() {
    FindCtx c{GetCurrentProcessId(), nullptr};
    EnumWindows(EnumCb, (LPARAM)&c);
    return c.hwnd;
}

// --- Estilo --------------------------------------------------------------------------------------
static const ImVec4 kAccent(0.10f, 0.78f, 0.52f, 1.0f);

static void ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 2; s.GrabRounding = 2; s.ChildRounding = 0;
    s.FramePadding = ImVec2(6, 4); s.ItemSpacing = ImVec2(8, 6);
    s.WindowBorderSize = 0; s.FrameBorderSize = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]       = ImVec4(0.16f, 0.16f, 0.16f, 1);
    c[ImGuiCol_ChildBg]        = ImVec4(0.19f, 0.19f, 0.19f, 1);
    c[ImGuiCol_Border]         = ImVec4(0.32f, 0.32f, 0.32f, 1);
    c[ImGuiCol_FrameBg]        = ImVec4(0.11f, 0.11f, 0.11f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.18f, 0.18f, 0.18f, 1);
    c[ImGuiCol_Button]         = ImVec4(0.24f, 0.24f, 0.24f, 1);
    c[ImGuiCol_ButtonHovered]  = ImVec4(0.30f, 0.30f, 0.30f, 1);
    c[ImGuiCol_ButtonActive]   = ImVec4(0.10f, 0.50f, 0.35f, 1);
    c[ImGuiCol_CheckMark]      = kAccent;
    c[ImGuiCol_SliderGrab]     = kAccent;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.2f, 0.9f, 0.6f, 1);
    c[ImGuiCol_Header]         = ImVec4(0.10f, 0.50f, 0.35f, 0.6f);
    c[ImGuiCol_HeaderHovered]  = ImVec4(0.10f, 0.60f, 0.40f, 0.7f);
    c[ImGuiCol_Text]           = ImVec4(0.88f, 0.88f, 0.88f, 1);
}

// --- Contenido -----------------------------------------------------------------------------------
static const char* kTabs[] = {"Resumen", "Healing", "Targeting", "Herramientas", "Scripting", "Ajustes"};
static int g_tab = 0;
static bool g_dirty = false;
static DWORD g_startTick = 0;

static void Bar(const char* label, uint32_t cur, uint32_t max, ImVec4 col) {
    float f = max ? (float)cur / (float)max : 0.f;
    char txt[64]; snprintf(txt, sizeof(txt), "%u / %u  (%.0f%%)", cur, max, f * 100.f);
    ImGui::TextUnformatted(label);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    ImGui::ProgressBar(f, ImVec2(-1, 20), txt);
    ImGui::PopStyleColor();
}

static const char* kKindNames[] = {"Hechizo", "Item"};

// Regla estilo ZeroBot: se elige QUE ejecutar (hechizo por palabras / item por ID), no una tecla.
// allowSpell=false para pociones: solo items.
static void RuleEditor(const char* id, const char* title, HealRule& r, const char* statName, bool allowSpell) {
    ImGui::PushID(id);
    ImGui::SeparatorText(title);
    bool en = r.enabled;
    if (ImGui::Checkbox("Activo", &en)) { r.enabled = en; g_dirty = true; }

    int pct = r.pct;
    char lbl[48]; snprintf(lbl, sizeof(lbl), "Si %s <= %%", statName);
    if (ImGui::SliderInt(lbl, &pct, 1, 99, "%d%%")) { r.pct = pct; g_dirty = true; }

    int kind = allowSpell ? r.kind.load() : (int)ActionKind::Item;
    if (allowSpell) {
        if (ImGui::Combo("Tipo", &kind, kKindNames, IM_ARRAYSIZE(kKindNames))) { r.kind = kind; g_dirty = true; }
    }

    if (kind == (int)ActionKind::Spell) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", r.Words().c_str());
        if (ImGui::InputTextWithHint("Palabras", "ej: exura vita", buf, sizeof(buf))) { r.SetWords(buf); g_dirty = true; }
        int mm = r.minMana;
        if (ImGui::InputInt("Mana minima", &mm, 10, 100)) { r.minMana = mm < 0 ? 0 : mm; g_dirty = true; }
        if (ImGui::Button("Probar ahora")) {
            auto w = r.Words();
            if (!w.empty()) Log("ui: prueba de hechizo '%s' -> %s", w.c_str(), actions::Say(w) ? "encolado" : "FALLO");
        }
    } else {
        int item = r.itemId;
        if (ImGui::InputInt("ID del item", &item, 1, 100)) { r.itemId = item < 0 ? 0 : item; g_dirty = true; }
        ImGui::TextDisabled("Se usa sobre ti mismo (el item debe estar en tu inventario).");
        if (ImGui::Button("Probar ahora")) {
            if (item > 0)
                Log("ui: prueba de item %d -> %s", item, actions::UseItemOnSelf((uint32_t)item) ? "encolado" : "FALLO");
        }
    }

    int cd = r.cooldownMs;
    if (ImGui::SliderInt("Cooldown", &cd, 200, 5000, "%d ms")) { r.cooldownMs = cd; g_dirty = true; }
    ImGui::PopID();
}

static void TabResumen() {
    auto& st = GetState(); auto& cfg = GetConfig();
    bool in = st.inGame;
    ImGui::TextColored(in ? kAccent : ImVec4(0.9f, 0.4f, 0.3f, 1), in ? "En juego" : "Jugador no encontrado");
    ImGui::Spacing();
    Bar("Vida", st.hp, st.maxHp, ImVec4(0.75f, 0.18f, 0.18f, 1));
    Bar("Mana", st.mana, st.maxMana, ImVec4(0.25f, 0.35f, 0.85f, 1));
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    bool a = cfg.active;
    if (ImGui::Checkbox("Bot activo (interruptor general)", &a)) {
        cfg.active = a;
        Log("ui: bot %s", a ? "ACTIVADO" : "DESACTIVADO");
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Healing (hechizos y pociones) y Targeting/Magic Shooter: operativos.");
    ImGui::PopTextWrapPos();
    DWORD s = (GetTickCount() - g_startTick) / 1000;
    ImGui::Spacing();
    ImGui::TextDisabled("Tiempo cargado: %02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
}

static void TabHealing() {
    auto& c = GetConfig();
    RuleEditor("spell", "Curacion principal", c.healSpell, "HP", true);
    RuleEditor("hpPot", "Pocion de vida", c.healPotion, "HP", false);
    RuleEditor("mpPot", "Pocion de mana", c.manaPotion, "Mana", false);
}

static void SyncText(const char* label, const char* hint, SyncString& s, float width = -1) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", s.Get().c_str());
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputTextWithHint(label, hint, buf, sizeof(buf))) { s.Set(buf); g_dirty = true; }
}

static void IntCell(const char* id, std::atomic<int>& v, int lo, int hi) {
    int x = v;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputInt(id, &x, 0, 0)) { v = x < lo ? lo : (x > hi ? hi : x); g_dirty = true; }
}

static void TabTargeting() {
    auto& sh = GetConfig().shooter;

    bool en = sh.enabled;
    if (ImGui::Checkbox("Targeting + Magic Shooter activo", &en)) {
        sh.enabled = en;
        Log("ui: magic shooter %s", en ? "ACTIVADO" : "DESACTIVADO");
    }
    ImGui::TextDisabled("Requiere tambien el interruptor general (abajo a la izquierda).");

    // ---- Estado (arriba: es lo que se consulta mas a menudo) ----
    ImGui::Spacing(); ImGui::Separator(); ImGui::TextColored(kAccent, "Estado");
    auto s = shooter::GetStatus();
    if (!s.ok) {
        ImGui::TextDisabled("Sin lectura de criaturas (no estas en juego).");
    } else {
        ImGui::Text("Posicion: %d, %d, %d", s.x, s.y, s.z);
        if (s.targetName.empty()) ImGui::TextDisabled("Objetivo: ninguno");
        else ImGui::Text("Objetivo: %s (%d%%)", s.targetName.c_str(), s.targetHp);
        ImGui::Text("Monstruos cercanos: %d", s.monstersNear);
        if (ImGui::Button("Atacar al mas cercano ahora")) shooter::AttackNearest();
        if (!s.list.empty() && ImGui::BeginChild("##near", ImVec2(0, 96), true)) {
            for (const auto& m : s.list)
                ImGui::TextColored(m.isTarget ? ImVec4(0.95f, 0.35f, 0.3f, 1) : ImVec4(0.8f, 0.8f, 0.8f, 1),
                                   "%s %-18s %3d%%  dist %d", m.isTarget ? ">" : " ", m.name.c_str(), m.hp,
                                   m.dist);
            ImGui::EndChild();
        } else if (!s.list.empty()) {
            ImGui::EndChild();
        }
    }

    // ---- Opciones ----
    ImGui::Spacing(); ImGui::Separator(); ImGui::TextColored(kAccent, "Opciones");
    bool at = sh.autoTarget;
    if (ImGui::Checkbox("Atacar automaticamente al mas cercano", &at)) { sh.autoTarget = at; g_dirty = true; }
    bool rt = sh.requireTarget;
    if (ImGui::Checkbox("Lanzar hechizos solo con objetivo", &rt)) { sh.requireTarget = rt; g_dirty = true; }
    int md = sh.maxDistance;
    ImGui::SetNextItemWidth(160);
    if (ImGui::SliderInt("Distancia maxima", &md, 1, 7, "%d casillas")) { sh.maxDistance = md; g_dirty = true; }
    SyncText("Solo atacar##only", "vacio = todos (ej: Werelion, Dog)", sh.onlyNames, -100);
    SyncText("Ignorar##ign", "ej: Dog, Cat", sh.ignoreNames, -100);

    // ---- Hechizos ----
    ImGui::Spacing(); ImGui::Separator(); ImGui::TextColored(kAccent, "Hechizos de ataque (prioridad de arriba a abajo)");
    if (ImGui::BeginTable("##spells", 7, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 26);
        ImGui::TableSetupColumn("Palabras", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Mana", ImGuiTableColumnFlags_WidthFixed, 58);
        ImGui::TableSetupColumn("CD ms", ImGuiTableColumnFlags_WidthFixed, 58);
        ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 38);
        ImGui::TableSetupColumn("Rango", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableSetupColumn("ID / CD", ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableHeadersRow();
        for (int i = 0; i < kShooterSpells; ++i) {
            auto& r = sh.spells[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool e = r.enabled;
            if (ImGui::Checkbox("##en", &e)) { r.enabled = e; g_dirty = true; }
            ImGui::TableSetColumnIndex(1); SyncText("##words", "ej: exori min", r.words, -1);
            ImGui::TableSetColumnIndex(2); IntCell("##mana", r.minMana, 0, 1000000);
            ImGui::TableSetColumnIndex(3); IntCell("##cd", r.cooldownMs, 200, 60000);
            ImGui::TableSetColumnIndex(4); IntCell("##min", r.minMonsters, 1, 20);
            ImGui::TableSetColumnIndex(5); IntCell("##rng", r.range, 1, 8);
            ImGui::TableSetColumnIndex(6);
            if (!r.enabled || r.words.Get().empty() || !s.ok || (size_t)i >= s.ruleId.size()) {
                ImGui::TextDisabled("-");
            } else if (s.ruleId[i] == 0) {
                ImGui::TextDisabled("buscando...");
            } else if (s.ruleCdMs[i] > 0) {
                ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1), "%d  %.1fs", s.ruleId[i], s.ruleCdMs[i] / 1000.0f);
            } else {
                ImGui::TextColored(kAccent, "%d  listo", s.ruleId[i]);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Min = monstruos minimos a distancia <= Rango (1 para ataques a un objetivo; 2-3+ para hechizos de area). \"CD ms\" solo se usa mientras no se conozca el ID del hechizo; con ID se usa el cooldown real del juego.");
    ImGui::PopTextWrapPos();
}

static void TabPlaceholder(const char* what) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", what);
    ImGui::TextDisabled("En construccion.");
    ImGui::PopTextWrapPos();
}

static void TabAjustes() {
    ImGui::Text("VTBot v0.1");
    ImGui::TextDisabled("Cliente Tibia 15.03 (x64)");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    {
        auto& t = GetConfig().tools;
        bool hb = t.hudButton;
        if (ImGui::Checkbox("Boton VT dentro del cliente", &hb)) { t.hudButton = hb; g_dirty = true; }
        ImGui::TextDisabled("Clic = ocultar/mostrar este HUD. Arrastralo para moverlo.");
        ImGui::TextDisabled("Minimizar (_) y cerrar (X) tambien lo ocultan.");
        if (ImGui::Button("Restaurar posicion del boton")) { t.btnRight = 9; t.btnTop = 215; g_dirty = true; }
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("Guardar configuracion", ImVec2(-1, 0))) { SaveConfig(); g_dirty = false; }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1));
    if (ImGui::Button("Descargar VTBot", ImVec2(-1, 0))) UnloadRequested() = true;
    ImGui::PopStyleColor();
}

static const char* kUseKinds[] = {"0 - Usar", "1 - Sobre ti", "2 - Sobre objetivo", "3 - Con mira"};

static void KindCombo(const char* label, std::atomic<int>& v) {
    int k = v;
    ImGui::SetNextItemWidth(170);
    if (ImGui::Combo(label, &k, kUseKinds, IM_ARRAYSIZE(kUseKinds))) { v = k; g_dirty = true; }
}

static void AgoText(uint32_t s) {
    if (s == 0xFFFFFFFFu) ImGui::TextDisabled("Ultima vez: nunca");
    else ImGui::TextDisabled("Ultima vez: hace %us", s);
}

static void TabHerramientas() {
    auto& t = GetConfig().tools;
    auto st = tools::GetStatus();
    ImGui::TextDisabled("Todas requieren el interruptor general (abajo a la izquierda).");

    // ---- Comer ----
    ImGui::SeparatorText("Comer");
    bool eat = t.eat;
    if (ImGui::Checkbox("Comer automaticamente", &eat)) { t.eat = eat; Log("ui: comer %s", eat ? "ON" : "OFF"); }
    int item = t.eatItem;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("ID de la comida##eat", &item, 1, 100)) { t.eatItem = item < 0 ? 0 : item; g_dirty = true; }
    KindCombo("Tipo de uso##eat", t.eatKind);
    bool onH = t.eatOnHunger;
    if (ImGui::Checkbox("Solo cuando tenga hambre (icono)", &onH)) { t.eatOnHunger = onH; g_dirty = true; }
    ImGui::SameLine();
    if (!st.regenKnown) ImGui::TextDisabled("(estado no disponible)");
    else if (st.regenSec == 0) ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "HAMBRE");
    else ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Comida: %us", st.regenSec);
    if (onH) {
        int et = t.eatTimes;
        ImGui::SetNextItemWidth(170);
        if (ImGui::InputInt("Veces seguidas##eat", &et, 1, 1)) { t.eatTimes = et < 1 ? 1 : (et > 5 ? 5 : et); g_dirty = true; }
        int eg = t.eatGapMs;
        ImGui::SetNextItemWidth(170);
        if (ImGui::InputInt("Pausa entre comidas (ms)##eat", &eg, 100, 500)) { t.eatGapMs = eg < 500 ? 500 : eg; g_dirty = true; }
        int eh = t.eatHungerSec;
        ImGui::SetNextItemWidth(170);
        if (ImGui::InputInt("Comer si quedan <= (s)##eat", &eh, 5, 30)) { t.eatHungerSec = eh < 0 ? 0 : eh; g_dirty = true; }
    } else {
        int iv = t.eatIntervalSec;
        ImGui::SetNextItemWidth(170);
        if (ImGui::InputInt("Cada (s)##eat", &iv, 10, 60)) { t.eatIntervalSec = iv < 5 ? 5 : iv; g_dirty = true; }
    }
    if (ImGui::Button("Comer ahora")) {
        Log("ui: comer ahora item %d kind %d -> %s", item, t.eatKind.load(),
            actions::UseItem((uint32_t)item, t.eatKind) ? "encolado" : "FALLO");
    }
    ImGui::SameLine(); AgoText(st.lastEatAgoSec);
    // ---- Convertir monedas ----
    ImGui::SeparatorText("Convertir monedas");
    bool co = t.coins;
    if (ImGui::Checkbox("Convertir monedas automaticamente", &co)) { t.coins = co; Log("ui: monedas %s", co ? "ON" : "OFF"); }
    bool g = t.coinsGold, p = t.coinsPlat;
    if (ImGui::Checkbox("Gold -> Platinum", &g)) { t.coinsGold = g; g_dirty = true; }
    ImGui::SameLine();
    if (ImGui::Checkbox("Platinum -> Crystal", &p)) { t.coinsPlat = p; g_dirty = true; }
    int civ = t.coinsIntervalSec;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("Revisar cada (s)##coins", &civ, 1, 5)) { t.coinsIntervalSec = civ < 1 ? 1 : civ; g_dirty = true; }
    if (ImGui::Button("Convertir ahora")) {
        Log("ui: convertir monedas -> %s", actions::ConvertCoins(g, p) ? "encolado" : "FALLO");
    }
    ImGui::SameLine();
    if (ImGui::Button("Ver contenedores (log)")) containers::LogDump();
    ImGui::SameLine(); AgoText(st.lastCoinsAgoSec);
    ImGui::TextDisabled("Usa (click derecho > Usar) cada stack de exactamente 100 que haya en un contenedor abierto.");

    // ---- Condiciones ----
    ImGui::SeparatorText("Condiciones");
    bool hs = t.haste;
    if (ImGui::Checkbox("Auto haste", &hs)) { t.haste = hs; Log("ui: auto haste %s", hs ? "ON" : "OFF"); }
    ImGui::SameLine();
    if (!st.condKnown) ImGui::TextDisabled("(estado no disponible)");
    else if (st.conditions & game::kCondHaste) ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Haste: ACTIVO");
    else ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "Haste: no");
    {
        char hb[64];
        snprintf(hb, sizeof(hb), "%s", t.hasteWords.Get().c_str());
        ImGui::SetNextItemWidth(170);
        if (ImGui::InputTextWithHint("Palabras##haste", "ej: utani hur", hb, sizeof(hb))) { t.hasteWords.Set(hb); g_dirty = true; }
    }
    int hmm = t.hasteMinMana;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("Mana minima##haste", &hmm, 10, 100)) { t.hasteMinMana = hmm < 0 ? 0 : hmm; g_dirty = true; }
    int hrt = t.hasteRetryMs;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("Reintento (ms)##haste", &hrt, 100, 500)) { t.hasteRetryMs = hrt < 500 ? 500 : hrt; g_dirty = true; }
    if (ImGui::Button("Lanzar haste ahora")) {
        auto w = t.hasteWords.Get();
        if (!w.empty()) Log("ui: haste ahora '%s' -> %s", w.c_str(), actions::Say(w) ? "encolado" : "FALLO");
    }
    ImGui::SameLine(); AgoText(st.lastHasteAgoSec);
    if (st.condKnown) ImGui::TextDisabled("Estados (mascara): 0x%x", st.conditions);

    // ---- Anti AFK ----
    ImGui::SeparatorText("Anti AFK");
    bool afk = t.antiAfk;
    if (ImGui::Checkbox("Anti AFK (girar el personaje)", &afk)) { t.antiAfk = afk; Log("ui: antiafk %s", afk ? "ON" : "OFF"); }
    int aiv = t.afkIntervalSec;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("Cada (s)##afk", &aiv, 10, 60)) { t.afkIntervalSec = aiv < 5 ? 5 : aiv; g_dirty = true; }
    ImGui::Text("Probar giro:");
    static const char* kDirs[] = {"Norte", "Este", "Sur", "Oeste"};
    for (int d = 0; d < 4; ++d) {
        ImGui::SameLine();
        if (ImGui::Button(kDirs[d])) tools::TurnDir(d);
    }
    ImGui::SameLine();
    if (ImGui::Button("Aleatorio")) tools::TurnRandom();
    AgoText(st.lastAfkAgoSec);

    // ---- Look: ID del item ----
    auto si = sniff::Get();
    ImGui::SeparatorText("ID de items (Look)");
    if (!si.installed) ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f), "Observador no instalado (ver log).");
    {
        bool li = t.lookId;
        if (ImGui::Checkbox("Mostrar el ID en pantalla al dar Look", &li)) { t.lookId = li; g_dirty = true; }
        ImGui::SameLine();
        int lms = t.lookIdMs;
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputInt("ms##lookms", &lms, 500, 1000)) { t.lookIdMs = lms < 500 ? 500 : (lms > 15000 ? 15000 : lms); g_dirty = true; }
        int dx = t.lookDx, dy = t.lookDy;
        ImGui::SetNextItemWidth(110);
        bool c1 = ImGui::InputInt("X##lookdx", &dx, 10, 50);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        bool c2 = ImGui::InputInt("Y##lookdy", &dy, 10, 50);
        if (c1 || c2) { t.lookDx = dx; t.lookDy = dy; overlay::SetOffset(dx, dy); g_dirty = true; }
        if (ImGui::SmallButton("Probar mensaje")) {
            overlay::SetOffset(t.lookDx, t.lookDy);
            overlay::Show("ID: 12345", (unsigned)t.lookIdMs.load());
        }
        ImGui::SameLine(); ImGui::TextDisabled("X/Y: posicion del mensaje (px, desde el centro de la ventana)");
    }
    if (!si.look.valid) {
        ImGui::TextDisabled("Da Look (click derecho > Look, o Shift+click) a un item y su ID aparece aqui y en el log.");
    } else {
        ImGui::Text("Ultimo Look: ID");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.6f, 1.f), "%u", si.look.itemId);
        ImGui::SameLine();
        if (ImGui::SmallButton("Copiar##lookid")) {
            char b[32]; snprintf(b, sizeof(b), "%u", si.look.itemId); ImGui::SetClipboardText(b);
        }
        if (si.look.where == sniff::Where::World)
            ImGui::TextDisabled("en el mapa (%d, %d, %d) pila %u", si.look.x, si.look.y, si.look.z, si.look.stack);
        else if (si.look.where == sniff::Where::Container)
            ImGui::TextDisabled("en el contenedor %u, slot %u", si.look.cid, si.look.slot);
        else
            ImGui::TextDisabled("en el inventario / equipo");
    }

    // ---- Dummy ----
    ImGui::SeparatorText("Entrenar con dummy");
    bool dm = t.dummy;
    if (ImGui::Checkbox("Entrenar con dummy (arma de ejercicio)", &dm)) { t.dummy = dm; Log("ui: dummy %s", dm ? "ON" : "OFF"); }
    int dw = t.dummyItem;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("ID arma de ejercicio##dummy", &dw, 0, 0)) { t.dummyItem = dw < 0 ? 0 : dw; g_dirty = true; }
    ImGui::Text("Dummy: ID %d  casilla (%d, %d, %d)  pila %d", t.dummyTarget.load(), t.dummyX.load(), t.dummyY.load(),
                t.dummyZ.load(), t.dummyStack.load());
    int div = t.dummyIntervalSec;
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputInt("Cada (s)##dummy", &div, 1, 5)) { t.dummyIntervalSec = div < 1 ? 1 : div; g_dirty = true; }
    if (ImGui::Button("Tomar del ultimo Use-with")) {
        if (si.useWithTarget.valid && si.useWithTarget.where == sniff::Where::World) {
            t.dummyItem = (int)si.useWithItemId;
            t.dummyTarget = (int)si.useWithTarget.itemId;
            t.dummyX = si.useWithTarget.x; t.dummyY = si.useWithTarget.y; t.dummyZ = si.useWithTarget.z;
            t.dummyStack = (int)si.useWithTarget.stack;
            g_dirty = true;
            Log("ui: dummy tomado del use-with: arma %d, objeto %d en (%d,%d,%d) pila %d", t.dummyItem.load(),
                t.dummyTarget.load(), t.dummyX.load(), t.dummyY.load(), t.dummyZ.load(), t.dummyStack.load());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Tomar del ultimo Look")) {
        if (si.look.valid && si.look.where == sniff::Where::World) {
            t.dummyTarget = (int)si.look.itemId;
            t.dummyX = si.look.x; t.dummyY = si.look.y; t.dummyZ = si.look.z;
            t.dummyStack = (int)si.look.stack;
            g_dirty = true;
            Log("ui: dummy tomado del look: objeto %d en (%d,%d,%d) pila %d", t.dummyTarget.load(), t.dummyX.load(),
                t.dummyY.load(), t.dummyZ.load(), t.dummyStack.load());
        }
    }
    if (ImGui::Button("Usar arma en el dummy ahora")) {
        Log("ui: dummy ahora -> %s",
            actions::UseItemOnMap((uint32_t)t.dummyItem.load(), t.dummyX, t.dummyY, t.dummyZ,
                                  (uint32_t)t.dummyStack.load(), (uint32_t)t.dummyTarget.load())
                ? "encolado" : "FALLO");
    }
    ImGui::SameLine(); AgoText(st.lastDummyAgoSec);
    ImGui::TextDisabled("Uso: usa tu arma de ejercicio sobre el dummy UNA vez a mano y pulsa 'Tomar del ultimo Use-with'");
    ImGui::TextDisabled("(o da Look al dummy y 'Tomar del ultimo Look' + escribe el ID del arma). El arma debe estar");
    ImGui::TextDisabled("en un contenedor abierto. Quedate quieto y a distancia del dummy.");
}

static void DrawUI(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)(rc.right - rc.left), (float)(rc.bottom - rc.top)));
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::BeginChild("##side", ImVec2(130, 0), true);
    for (int i = 0; i < IM_ARRAYSIZE(kTabs); ++i)
        if (ImGui::Selectable(kTabs[i], g_tab == i, 0, ImVec2(0, 26))) g_tab = i;

    // Interruptor general SIEMPRE visible (abajo de la barra lateral)
    {
        auto& cfg = GetConfig();
        float bottom = ImGui::GetWindowHeight() - 62.0f;
        if (ImGui::GetCursorPosY() < bottom) ImGui::SetCursorPosY(bottom);
        ImGui::Separator();
        bool a = cfg.active;
        ImGui::PushStyleColor(ImGuiCol_Text, a ? kAccent : ImVec4(0.9f, 0.45f, 0.35f, 1));
        if (ImGui::Checkbox(a ? "BOT ACTIVO" : "BOT APAGADO", &a)) {
            cfg.active = a;
            Log("ui: bot %s", a ? "ACTIVADO" : "DESACTIVADO");
        }
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##content", ImVec2(0, 0), true);
    switch (g_tab) {
    case 0: TabResumen(); break;
    case 1: TabHealing(); break;
    case 2: TabTargeting(); break;
    case 3: TabHerramientas(); break;
    case 4: TabPlaceholder("Scripting: motor Lua embebido."); break;
    default: TabAjustes(); break;
    }
    ImGui::EndChild();
    ImGui::End();

    // autoguardado cuando se suelta el raton tras editar
    if (g_dirty && !ImGui::IsMouseDown(0)) { SaveConfig(); g_dirty = false; }
}

// --- Hilo ----------------------------------------------------------------------------------------
static DWORD WINAPI UiThread(LPVOID) {
    g_startTick = GetTickCount();
    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"VTBotWnd";
    RegisterClassExW(&wc);

    // esperar a que exista la ventana del cliente (hasta 15 s)
    HWND client = nullptr;
    for (int i = 0; i < 150 && !g_stop && !(client = FindClientWindow()); ++i) Sleep(100);

    int x = 100, y = 100, w = 580, h = 640;
    if (client) {
        RECT r; GetWindowRect(client, &r);
        x = r.right + 6; y = r.top;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        if (x + w > sw) x = r.right - w - 20;     // si no cabe al lado, dentro del cliente
    }
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT adj{0, 0, w, h};
    AdjustWindowRect(&adj, style, FALSE);
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"VTBotWnd", L"VTBot", style, x, y, adj.right - adj.left,
                                adj.bottom - adj.top, client /* owner: siempre encima del cliente */, nullptr, hi,
                                nullptr);
    if (!hwnd || !InitD3D(hwnd)) {
        Log("ui: no se pudo crear la ventana (hwnd=%p)", (void*)hwnd);
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(L"VTBotWnd", hi);
        return 1;
    }
    g_hwnd = hwnd;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ApplyTheme();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    Log("ui: lista (owner=%p)", (void*)client);

    MSG msg{};
    while (!g_stop) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_stop = true;
        }
        if (g_stop) break;
        if (IsIconic(hwnd) || !IsWindowVisible(hwnd)) { Sleep(50); continue; }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawUI(hwnd);
        ImGui::Render();

        const float clear[4] = {0.12f, 0.12f, 0.12f, 1.f};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D();
    g_hwnd = nullptr;
    DestroyWindow(hwnd);
    UnregisterClassW(L"VTBotWnd", hi);
    Log("ui: cerrada");
    return 0;
}

void Start() {
    g_stop = false;
    g_thread = CreateThread(nullptr, 0, UiThread, nullptr, 0, nullptr);
}

void Stop() {
    g_stop = true;
    if (g_thread) {
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}
}
