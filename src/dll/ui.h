#pragma once

namespace vt::ui {
// Arranca el hilo de la interfaz (ventana propia + Dear ImGui sobre D3D11).
void Start();
// Pide parar y espera a que el hilo termine y libere todo.
void Stop();
// Oculta / muestra la ventana del HUD (lo usa el boton "VT" dentro del cliente). Seguro desde cualquier hilo.
void ToggleVisible();
bool IsVisible();
}
