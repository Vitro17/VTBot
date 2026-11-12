#pragma once
#include <cstdint>
#include <string>

// Acciones de juego sin simular teclado: se invocan los slots internos del cliente.
namespace vt::actions {

// Dice `text` en el canal de chat activo (equivale a escribirlo y pulsar Enter). Sirve para lanzar hechizos.
// Devuelve false si no se pudo encolar (controlador no encontrado, etc.). Asincrono (hilo de la GUI de Qt).
bool Say(const std::string& text);

// Usa un item (por ID de tipo) sobre el propio personaje, como si pulsaras la hotkey "usar sobre ti".
// Asincrono (hilo de la GUI de Qt). false si no se pudo encolar (offsets/manejador no encontrados).
bool UseItemOnSelf(uint32_t itemId);

// Igual pero eligiendo el tipo de uso (campo "kind" de la accion): 1 = sobre ti. Los demas valores
// (usar normal, con mira...) se descubren probando; ver pestana Herramientas.
bool UseItem(uint32_t itemId, int kind);

// Ataca (fija como objetivo) la criatura con ese ID, igual que hacer clic en ella en la battle list:
// actualiza el objetivo local y envia el paquete Attack al servidor. Asincrono (hilo de la GUI de Qt).
bool Attack(uint32_t creatureId);

// Click derecho > Usar sobre un slot de contenedor (id de la ventana, indice de slot, id del item).
bool UseContainerSlot(uint8_t containerId, uint32_t slot, uint32_t itemId);

// Busca en los contenedores abiertos un stack de exactamente 100 monedas (gold 3031 y/o platinum 3035) y lo usa
// (lo convierte en 1 moneda superior). Uno por llamada. Asincrono (hilo de Qt); el resultado queda en el log.
bool ConvertCoins(bool gold, bool platinum);

// Usa un item que este en un contenedor abierto (p.ej. arma de ejercicio) CON una casilla del mapa (x,y,z absolutas,
// posicion en la pila y id del objeto destino, p.ej. el dummy). Equivale a "Use with" + clic en el dummy.
bool UseItemOnMap(uint32_t itemId, int32_t x, int32_t y, int32_t z, uint32_t stack, uint32_t targetId);

// Gira el personaje sin moverlo: dir 0=N, 1=E, 2=S, 3=O. Asincrono (hilo de la GUI de Qt).
bool Turn(int dir);

// Solo localiza y valida el controlador de chat (no invoca nada). Devuelve true si hay uno valido.
bool Probe();
}
