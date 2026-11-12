#pragma once
#include <string>

// Traduccion palabras de un hechizo -> ID, leyendo los registros de hechizos del cliente.
namespace vt::spells {

// Devuelve el ID del hechizo con esas palabras (p.ej. "exori gran" -> 105), o 0 si aun no se conoce.
// La primera consulta lanza una busqueda en memoria en un hilo aparte (~1 s) y devuelve 0 mientras tanto;
// los resultados (tambien los fallidos, se reintentan cada 15 s) quedan en cache.
int IdForWords(const std::string& words);

void Shutdown();   // espera a que terminen las busquedas en curso (llamar antes de descargar la DLL)
}
