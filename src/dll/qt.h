#pragma once
#include <functional>
#include <string>

// Puente minimo hacia Qt6Core.dll del cliente (sin cabeceras de Qt): ejecutar codigo en el hilo de un QObject
// y construir QStrings.
namespace vt::qt {

bool Init();   // resuelve exportaciones de Qt6Core.dll; false si falta alguna

// Ejecuta fn en el hilo al que pertenece `qobject` (cola de eventos de Qt). No bloquea.
bool PostToObjectThread(void* qobject, std::function<void()> fn);

// QMetaObject::activate(sender, metaObject, localSignalIndex, argv): emite una senal. Llamar SIEMPRE desde el
// hilo del sender (usar PostToObjectThread).
bool Activate(void* sender, const void* metaObject, int localIndex, void** argv);

// QString minimo (layout Qt6: d, ptr, size)
struct QStr {
    void* d = nullptr;
    char16_t* ptr = nullptr;
    long long size = 0;
};
QStr FromUtf8(const std::string& s);
void Free(QStr& s);
}
