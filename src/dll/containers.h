#pragma once
#include <cstdint>
#include <vector>

// Lectura de los contenedores abiertos (mochilas). TContainerStorage guarda un std::map<u8 id, shared_ptr<TContainer>>;
// cada TContainer tiene un vector<shared_ptr<Item>> (un item por slot, null = vacio).
namespace vt::containers {

struct Slot {
    uint8_t  cid = 0;       // id de la ventana de contenedor (el de los paquetes: 0,1,2...)
    uint32_t slot = 0;      // indice dentro del contenedor
    uint32_t itemId = 0;    // id de apariencia (3031 = gold coin...)
    uint32_t count = 0;     // cantidad si es apilable (0 = no apilable)
};

// Lee todos los contenedores abiertos. Pensado para llamarse desde el hilo de Qt (hilo GUI) para no competir con el
// cliente; en otro hilo funciona pero puede ver estados a medias. Devuelve false si no encuentra el almacen.
bool Snapshot(std::vector<Slot>& out);

// Escribe en el log el contenido de los contenedores (diagnostico).
void LogDump();
}
