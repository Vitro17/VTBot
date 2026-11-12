#include "offsets.h"
#include "log.h"
#include <windows.h>
#include <string>

namespace vt {
static Offsets g_off;
const Offsets& GetOffsets() { return g_off; }

static uint64_t Get(const wchar_t* ini, const wchar_t* key, uint64_t def) {
    wchar_t buf[64] = {};
    wchar_t d[32]; swprintf(d, 32, L"%llx", (unsigned long long)def);
    GetPrivateProfileStringW(L"offsets", key, d, buf, 64, ini);
    return wcstoull(buf, nullptr, 16);
}

bool LoadOffsets() {
    wchar_t path[MAX_PATH];
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&LoadOffsets, &self);
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring ini(path);
    ini = ini.substr(0, ini.find_last_of(L'\\') + 1) + L"offsets.ini";

    g_off.playerVtable = (uintptr_t)Get(ini.c_str(), L"playerVtable", 0);
    g_off.hp      = (uint32_t)Get(ini.c_str(), L"hp", g_off.hp);
    g_off.maxHp   = (uint32_t)Get(ini.c_str(), L"maxHp", g_off.maxHp);
    g_off.mana    = (uint32_t)Get(ini.c_str(), L"mana", g_off.mana);
    g_off.maxMana = (uint32_t)Get(ini.c_str(), L"maxMana", g_off.maxMana);
    g_off.chatVtable    = (uintptr_t)Get(ini.c_str(), L"chatVtable", 0);
    g_off.chatMetacall  = (uintptr_t)Get(ini.c_str(), L"chatMetacall", 0);
    g_off.chatSayIndex  = (int)Get(ini.c_str(), L"chatSayIndex", (uint64_t)g_off.chatSayIndex);
    g_off.chatSayVolume = (int)Get(ini.c_str(), L"chatSayVolume", (uint64_t)g_off.chatSayVolume);
    g_off.useHandlerVtable = (uintptr_t)Get(ini.c_str(), L"useHandlerVtable", 0);
    g_off.useFactoryField  = (uint32_t)Get(ini.c_str(), L"useFactoryField", g_off.useFactoryField);
    g_off.useFactoryVtable = (uintptr_t)Get(ini.c_str(), L"useFactoryVtable", 0);
    g_off.useFactoryFn     = (uintptr_t)Get(ini.c_str(), L"useFactoryFn", 0);
    g_off.usePublishMeta   = (uintptr_t)Get(ini.c_str(), L"usePublishMeta", 0);
    g_off.usePublishIndex  = (int)Get(ini.c_str(), L"usePublishIndex", (uint64_t)g_off.usePublishIndex);
    g_off.useSettingsKind  = (uint32_t)Get(ini.c_str(), L"useSettingsKind", g_off.useSettingsKind);
    g_off.useSettingsItem  = (uint32_t)Get(ini.c_str(), L"useSettingsItem", g_off.useSettingsItem);
    g_off.useKindSelf      = (int)Get(ini.c_str(), L"useKindSelf", (uint64_t)g_off.useKindSelf);
    g_off.storageVtable       = (uintptr_t)Get(ini.c_str(), L"storageVtable", 0);
    g_off.attackHandlerVtable = (uintptr_t)Get(ini.c_str(), L"attackHandlerVtable", 0);
    g_off.attackFn            = (uintptr_t)Get(ini.c_str(), L"attackFn", 0);
    g_off.cooldownVtable      = (uintptr_t)Get(ini.c_str(), L"cooldownVtable", 0);
    g_off.moveHandlerVtable   = (uintptr_t)Get(ini.c_str(), L"moveHandlerVtable", 0);
    g_off.moveDispatchFn      = (uintptr_t)Get(ini.c_str(), L"moveDispatchFn", 0);
    g_off.intentActionVtable  = (uintptr_t)Get(ini.c_str(), L"intentActionVtable", 0);
    g_off.intentRotateVtable  = (uintptr_t)Get(ini.c_str(), L"intentRotateVtable", 0);
    g_off.intentRotateCtrl    = (uintptr_t)Get(ini.c_str(), L"intentRotateCtrl", 0);
    g_off.actionCtrl          = (uintptr_t)Get(ini.c_str(), L"actionCtrl", 0);
    g_off.opNew               = (uintptr_t)Get(ini.c_str(), L"opNew", 0);
    g_off.containerStorageVtable = (uintptr_t)Get(ini.c_str(), L"containerStorageVtable", 0);
    g_off.containerVtable     = (uintptr_t)Get(ini.c_str(), L"containerVtable", 0);
    g_off.containerMapOff     = (uint32_t)Get(ini.c_str(), L"containerMapOff", 0x28);
    g_off.cumulativeVtable    = (uintptr_t)Get(ini.c_str(), L"cumulativeVtable", 0);
    g_off.useActionVtable     = (uintptr_t)Get(ini.c_str(), L"useActionVtable", 0);
    g_off.useActionCtrl       = (uintptr_t)Get(ini.c_str(), L"useActionCtrl", 0);
    g_off.ctxSlotVtable       = (uintptr_t)Get(ini.c_str(), L"ctxSlotVtable", 0);
    g_off.ctxSlotCtrl         = (uintptr_t)Get(ini.c_str(), L"ctxSlotCtrl", 0);
    g_off.ctxTypeVtable       = (uintptr_t)Get(ini.c_str(), L"ctxTypeVtable", 0);
    g_off.ctxTypeCtrl         = (uintptr_t)Get(ini.c_str(), L"ctxTypeCtrl", 0);
    g_off.playerDataVtable    = (uintptr_t)Get(ini.c_str(), L"playerDataVtable", 0);
    g_off.conditionsOff       = (uint32_t)Get(ini.c_str(), L"conditionsOff", 0x94);
    g_off.regenOff            = (uint32_t)Get(ini.c_str(), L"regenOff", 0x88);
    g_off.clientTimestamp      = (uint32_t)Get(ini.c_str(), L"clientTimestamp", 0);
    g_off.clientImageSize      = (uint32_t)Get(ini.c_str(), L"clientImageSize", 0);
    g_off.genericHandlerVtable = (uintptr_t)Get(ini.c_str(), L"genericHandlerVtable", 0);
    g_off.genericDispatchSlot  = (uint32_t)Get(ini.c_str(), L"genericDispatchSlot", 11);
    g_off.genericDispatchFn    = (uintptr_t)Get(ini.c_str(), L"genericDispatchFn", 0);
    g_off.lookVtable           = (uintptr_t)Get(ini.c_str(), L"lookVtable", 0);
    g_off.useWithVtable        = (uintptr_t)Get(ini.c_str(), L"useWithVtable", 0);
    g_off.useWithCtrl          = (uintptr_t)Get(ini.c_str(), L"useWithCtrl", 0);
    g_off.ctxWorldVtable       = (uintptr_t)Get(ini.c_str(), L"ctxWorldVtable", 0);
    g_off.ctxWorldCtrl         = (uintptr_t)Get(ini.c_str(), L"ctxWorldCtrl", 0);
    g_off.ctxWorldSubVtable    = (uintptr_t)Get(ini.c_str(), L"ctxWorldSubVtable", 0);
    Log("offsets.ini: chatVtable=0x%llx chatMetacall=0x%llx sayIdx=%d vol=%d",
        (unsigned long long)g_off.chatVtable, (unsigned long long)g_off.chatMetacall, g_off.chatSayIndex,
        g_off.chatSayVolume);
    g_off.valid   = g_off.playerVtable != 0;
    Log("offsets.ini: valid=%d playerVtable=0x%llx hp=+0x%x maxHp=+0x%x mana=+0x%x maxMana=+0x%x", g_off.valid,
        (unsigned long long)g_off.playerVtable, g_off.hp, g_off.maxHp, g_off.mana, g_off.maxMana);
    return g_off.valid;
}
}
