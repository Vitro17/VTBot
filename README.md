# VTBot

Bot para **Tibia 15.03 (x64, Qt6)** hecho para el servidor **ModukOT**. Se inyecta en `client.exe` y actúa llamando
a las funciones internas del cliente (no simula teclado ni ratón). Interfaz propia con Dear ImGui en una ventana
aparte y un botón **VT** dentro del cliente (debajo del botón PvP) para ocultarla/mostrarla.

## Uso rápido (sin compilar)

1. Clona el repo: `git clone <url-del-repo>`
2. Abre Tibia (`client.exe`) y entra con tu personaje.
3. Ejecuta **`release\VTBot\Iniciar VTBot.bat`** (si el cliente corre como administrador, ejecútalo también como
   administrador: clic derecho > Ejecutar como administrador).
4. Activa **BOT ACTIVO** (abajo a la izquierda en la ventana del bot) y configura cada pestaña.

Para quitarlo: *Ajustes > Descargar VTBot* o `release\VTBot\Descargar VTBot.bat`.

> Si Windows Defender lo marca es un falso positivo típico de cualquier programa que se inyecta en otro; añade la
> carpeta a las exclusiones.

## Funciones

- **Healing:** hechizo de curación, poción de vida y de mana por porcentaje.
- **Targeting / Magic Shooter:** ataca al monstruo más cercano y lanza hechizos por prioridad con combo.
- **Herramientas:** comer (solo con hambre, por el icono), convertir monedas, auto haste, anti-AFK (girar),
  entrenar con dummy (arma de ejercicio), ID del item al dar *Look* en un mensaje en pantalla.
- Botón **VT** dentro del cliente para ocultar/mostrar el HUD (arrastrable).

## Compatibilidad (importante)

Los *offsets* de `offsets.ini` son direcciones internas de **un `client.exe` concreto** (huella en
`clientTimestamp` / `clientImageSize`). Con otra versión del cliente el bot muestra un aviso y **no se carga**, para
no cerrar el juego. Si el servidor actualiza el cliente hay que volver a obtener los offsets (ver `tools/`).

## Compilar

Requisitos: Visual Studio 2022 Build Tools (MSVC), CMake y Ninja (los que trae VS Build Tools).

```bat
build.bat      :: compila a build\bin (vtloader.exe, vtcore.dll)
package.bat    :: compila y deja el paquete en release\VTBot y release\VTBot.zip
```

`vtloader.exe` busca `client.exe` y carga `vtcore.dll` en él (`vtloader.exe eject` lo descarga).

## Estructura

| Carpeta | Contenido |
|---|---|
| `src/dll` | el bot (se ejecuta dentro del cliente): acciones, lectura de estado, UI, overlay, herramientas |
| `src/loader` | inyector |
| `third_party/imgui` | Dear ImGui (Win32 + DX11) |
| `tools` | scripts de ingeniería inversa en Python (puntos de ruptura por hardware, RTTI, vtables, capturas) |
| `release/VTBot` | binarios ya compilados + scripts de inicio |
| `offsets.ini` | direcciones del cliente (RVAs en hexadecimal) |

## Aviso

Uso bajo tu propia responsabilidad. Proyecto independiente, sin relación con Cipsoft ni con ZeroBot.
