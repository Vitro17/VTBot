#pragma once
#include <cstdint>

namespace vt {
// Valores de la build 15.03 de Tibia (ver offsets.ini). Los RVAs son relativos a client.exe.
struct Offsets {
    uintptr_t playerVtable = 0;   // RVA de la vtable del QObject de stats del jugador
    uint32_t  hp = 0x78;          // offsets DENTRO del objeto
    uint32_t  maxHp = 0x7C;
    uint32_t  mana = 0x80;
    uint32_t  maxMana = 0x84;
    // TChatController (QObject): slot sendChatMessage(QString, int) invocable via static_metacall
    uintptr_t chatVtable = 0;        // RVA de la vtable
    uintptr_t chatMetacall = 0;      // RVA de qt_static_metacall de TChatController
    int       chatSayIndex = 10;     // indice del metodo sendChatMessage en su QMetaObject
    int       chatSayVolume = 0;     // 2do argumento (int) de sendChatMessage
    // Uso de items: TActionButtonGameActionHandler convierte la config de un boton de accion en una
    // TGameAction (fabrica) y la publica con la senal publishGameAction.
    uintptr_t useHandlerVtable = 0;      // RVA vtable del manejador (QObject emisor de la senal)
    uint32_t  useFactoryField = 0x20;    // handler+0x20 -> objeto fabrica
    uintptr_t useFactoryVtable = 0;      // RVA vtable de la fabrica (validacion del manejador)
    uintptr_t useFactoryFn = 0;          // RVA de: void make(Factory*, shared_ptr<IGameAction>* out, Settings*)
    uintptr_t usePublishMeta = 0;        // RVA del QMetaObject del manejador
    int       usePublishIndex = 0;       // indice local de la senal publishGameAction
    uint32_t  useSettingsKind = 0x38;    // en Settings: int32 (1 = usar sobre uno mismo)
    uint32_t  useSettingsItem = 0x3C;    // en Settings: uint32 itemId
    int       useKindSelf = 1;
    // Criaturas: TCreatureStorage (QObject) con std::map<uint32 id, shared_ptr<TCreature>> en +0x10/+0x18,
    // id propio en +0x20 y id del objetivo atacado en +0x2C (0 = sin objetivo).
    uintptr_t storageVtable = 0;         // RVA vtable de TCreatureStorage
    // Atacar: TCreaturesGameActionHandler::handleAttack(this, shared_ptr<IActionConCreatureId>*)
    // fija el objetivo local y emite el mensaje de red GameclientMessageAttack.
    uintptr_t attackHandlerVtable = 0;   // RVA vtable de TCreaturesGameActionHandler
    uintptr_t attackFn = 0;              // RVA de handleAttack
    // Cooldowns: TCooldownStorage (QObject) con std::map<uint16 spellId, entrada{int64 finEpochMs en +8}>
    // en +0x20 (cabeza) / +0x28 (tamano). Contiene solo los cooldowns activos.
    uintptr_t cooldownVtable = 0;        // RVA vtable de TCooldownStorage
    // Movimiento/giro: TPlayerMovementGameActionHandler::dispatch(this, shared_ptr<IGameAction>*) acepta una
    // TGameActionPlayerMovementIntent {vtable, shared_ptr<TPlayerMovementIntent>}; la intencion de giro es
    // TMovementIntentRotate {vtable, u8 direccion(0=N,1=E,2=S,3=O), u8 flag=1}.
    uintptr_t moveHandlerVtable = 0;     // RVA vtable de TPlayerMovementGameActionHandler
    uintptr_t moveDispatchFn = 0;        // RVA del despachador (slot 14 de esa vtable)
    uintptr_t intentActionVtable = 0;    // RVA vtable de TGameActionPlayerMovementIntent
    uintptr_t intentRotateVtable = 0;    // RVA vtable de TMovementIntentRotate
    uintptr_t intentRotateCtrl = 0;      // RVA vtable del bloque de control _Ref_count_obj2<TMovementIntentRotate>
    uintptr_t actionCtrl = 0;            // RVA vtable de _Ref_count_obj2<TGameActionPlayerMovementIntent>
    uintptr_t opNew = 0;                 // RVA de operator new(size_t) del cliente
    // Contenedores y "usar item de un slot" (click derecho > Usar): TContainerStorage tiene en +0x28 un
    // std::map<u8 id, shared_ptr<TContainer>>. El Use lleva dos contextos (ver actions.cpp).
    uintptr_t containerStorageVtable = 0;
    uintptr_t containerVtable = 0;       // TContainer
    uint32_t  containerMapOff = 0x28;
    uintptr_t cumulativeVtable = 0;      // TCumulativeObjectAppearanceInstance (items apilables; cantidad u8 en +0x48)
    uintptr_t useActionVtable = 0;       // TGameActionUse
    uintptr_t useActionCtrl = 0;         // _Ref_count_obj2<TGameActionUse>
    uintptr_t ctxSlotVtable = 0;         // TLocationContainerWithSlotGameActionContext
    uintptr_t ctxSlotCtrl = 0;
    uintptr_t ctxTypeVtable = 0;         // TObjectApperanceTypeGameActionContext
    uintptr_t ctxTypeCtrl = 0;
    // TPlayerData (QObject): +0x94 = mascara de estados/condiciones (bit 0x40 = haste; verificado en vivo).
    uintptr_t playerDataVtable = 0;
    uint32_t  conditionsOff = 0x94;
    uint32_t  regenOff = 0x88;       // tiempo de comida restante en segundos (0 = hambre; verificado en vivo)
    // Huella del client.exe para el que son estos offsets (0 = no comprobar). Si no coincide, el bot no se carga.
    uint32_t  clientTimestamp = 0;   // IMAGE_FILE_HEADER.TimeDateStamp
    uint32_t  clientImageSize = 0;   // IMAGE_OPTIONAL_HEADER.SizeOfImage
    // Despachador generico de acciones (TGenericGameActionHandler) y acciones/contextos de Look y Use-with.
    uintptr_t genericHandlerVtable = 0;  // vtable de TGenericGameActionHandler
    uint32_t  genericDispatchSlot = 11;  // slot que apunta a genericDispatchFn
    uintptr_t genericDispatchFn = 0;     // funcion (this, shared_ptr<Accion>*) por la que pasan todas las acciones
    uintptr_t lookVtable = 0;            // TGameActionLook
    uintptr_t useWithVtable = 0;         // TGameActionUseWith
    uintptr_t useWithCtrl = 0;           // _Ref_count_obj2<TGameActionUseWith>
    uintptr_t ctxWorldVtable = 0;        // TLocationWorldMapCoordinateWithStackPositionContext
    uintptr_t ctxWorldCtrl = 0;
    uintptr_t ctxWorldSubVtable = 0;     // vtable del subobjeto coordenada (obj+8)
    bool valid = false;
};

const Offsets& GetOffsets();
bool LoadOffsets();   // lee offsets.ini junto a vtbot.dll
}
