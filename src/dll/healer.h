#pragma once

namespace vt::healer {
// Evalua las reglas de curacion con el estado actual. Llamar periodicamente desde el hilo del bot.
void Tick();
}
