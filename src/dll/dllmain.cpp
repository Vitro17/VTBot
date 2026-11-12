#include <windows.h>
#include "log.h"
#include "offsets.h"
#include "game.h"
#include "memory.h"
#include "config.h"
#include "ui.h"
#include "healer.h"
#include "shooter.h"
#include "tools.h"
#include "spells.h"
#include "actions.h"
#include "sniff.h"
#include "overlay.h"

static HMODULE g_self = nullptr;
static volatile bool g_run = true;

// Los offsets son RVAs de un client.exe concreto: con otra version el bot tocaria memoria equivocada y el juego
// se cerraria. Se compara la huella (TimeDateStamp + SizeOfImage) con la de offsets.ini.
static bool ClientMatches(uint32_t* ts, uint32_t* size) {
    uintptr_t base = vt::mem::ModuleBase();
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!vt::mem::SafeRead(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (!vt::mem::SafeRead(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE) return false;
    *ts = nt.FileHeader.TimeDateStamp;
    *size = nt.OptionalHeader.SizeOfImage;
    const auto& o = vt::GetOffsets();
    if (o.clientTimestamp && o.clientTimestamp != *ts) return false;
    if (o.clientImageSize && o.clientImageSize != *size) return false;
    return true;
}

static DWORD WINAPI BotThread(LPVOID) {
    vt::LogInit();
    vt::Log("VTBot v0.1 cargado. client base=0x%llx size=0x%zx",
            (unsigned long long)vt::mem::ModuleBase(), vt::mem::ModuleSize());
    vt::LoadOffsets();
    {
        uint32_t ts = 0, sz = 0;
        if (!ClientMatches(&ts, &sz)) {
            vt::Log("client.exe no coincide con offsets.ini (timestamp=%x size=%x, esperado %x/%x). No se carga.", ts, sz,
                    vt::GetOffsets().clientTimestamp, vt::GetOffsets().clientImageSize);
            MessageBoxW(nullptr,
                        L"VTBot: este client.exe no es la version para la que fue hecho el bot (los offsets no "
                        L"coinciden).\n\nPara evitar que se cierre el juego, el bot no se carga.",
                        L"VTBot", MB_OK | MB_ICONERROR | MB_TOPMOST);
            FreeLibraryAndExitThread(g_self, 0);
        }
        vt::Log("client.exe verificado (timestamp=%x size=%x)", ts, sz);
    }
    vt::LoadConfig();
    vt::overlay::SetOffset(vt::GetConfig().tools.lookDx, vt::GetConfig().tools.lookDy);
    vt::overlay::Start();           // mensajes flotantes sobre el cliente
    vt::sniff::Install();           // observa Look / Use-with (ID de items, captura del dummy)
    vt::ui::Start();

    // Evento de descarga cooperativa: `vtloader.exe eject` lo senala
    wchar_t evName[64];
    swprintf(evName, 64, L"Local\\VTBot.Unload.%lu", GetCurrentProcessId());
    HANDLE unloadEv = CreateEventW(nullptr, TRUE, FALSE, evName);

    auto& st = vt::GetState();
    vt::game::PlayerState last{};
    bool haveLast = false;
    while (g_run) {
        if (unloadEv && WaitForSingleObject(unloadEv, 0) == WAIT_OBJECT_0) break;
        if (vt::UnloadRequested()) break;

        vt::game::PlayerState p;
        if (vt::game::ReadPlayer(p)) {
            st.hp = p.hp; st.maxHp = p.maxHp; st.mana = p.mana; st.maxMana = p.maxMana;
            st.inGame = true;
            if (!haveLast || p.hp != last.hp || p.mana != last.mana || p.maxHp != last.maxHp ||
                p.maxMana != last.maxMana) {
                vt::Log("HP %u/%u  MP %u/%u", p.hp, p.maxHp, p.mana, p.maxMana);
                last = p; haveLast = true;
            }
        } else {
            if (st.inGame) vt::Log("jugador no encontrado (estas en el juego?)");
            st.inGame = false;
            haveLast = false;
        }
        // diagnostico (sin invocar nada): localizar el controlador de chat una vez en juego
        static bool chatOk = false;
        static DWORD lastProbe = 0;
        if (st.inGame && !chatOk && GetTickCount() - lastProbe > 3000) {
            lastProbe = GetTickCount();
            chatOk = vt::actions::Probe();
        }
        vt::healer::Tick();
        vt::shooter::Tick();
        vt::tools::Tick();
        Sleep(100);
    }

    vt::Log("VTBot descargando...");
    vt::sniff::Remove();            // restaura la vtable del cliente antes de descargar el codigo del hook
    vt::overlay::Stop();
    vt::spells::Shutdown();         // busquedas de ID en curso (hilos aparte)
    vt::ui::Stop();                 // espera a que el hilo de UI termine antes de descargar la DLL
    Sleep(500);                     // deja drenar callbacks encolados en Qt que viven en esta DLL
    vt::Log("VTBot descargado.");
    if (unloadEv) CloseHandle(unloadEv);
    FreeLibraryAndExitThread(g_self, 0);
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = mod;
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, BotThread, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_run = false;
    }
    return TRUE;
}
