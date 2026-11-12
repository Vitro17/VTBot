#pragma once
#include <string>

// Mensaje flotante sobre la ventana del cliente (texto blanco con borde negro, centrado, estilo "mensaje de estado"
// de Tibia). Ventana propia transparente y "click-through" (no roba foco ni clics), dueña = ventana del cliente.
namespace vt::overlay {

void Start();   // lanza el hilo (llamar una vez)
void Stop();    // cierra el hilo y la ventana (antes de descargar la DLL)

// Desplazamiento en pixeles respecto al centro de la ventana del cliente (la zona de juego no suele estar centrada).
void SetOffset(int dx, int dy);

// Muestra `text` durante `ms` milisegundos (se desvanece al final). Barato y seguro desde cualquier hilo.
void Show(const std::string& text, unsigned ms = 3000);

}
