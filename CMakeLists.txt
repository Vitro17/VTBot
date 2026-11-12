cmake_minimum_required(VERSION 3.20)
project(VTBot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Runtime de C++ enlazado de forma estatica: los amigos no necesitan instalar el VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
add_compile_options(/utf-8)

# Loader: inyecta vtcore.dll en client.exe
add_executable(vtloader src/loader/main.cpp)
target_link_libraries(vtloader PRIVATE psapi)

# Dear ImGui (Win32 + DX11)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# DLL del bot (se ejecuta dentro del cliente)
add_library(vtbot SHARED
    src/dll/dllmain.cpp
    src/dll/log.cpp
    src/dll/memory.cpp
    src/dll/offsets.cpp
    src/dll/game.cpp
    src/dll/config.cpp
    src/dll/qt.cpp
    src/dll/actions.cpp
    src/dll/healer.cpp
    src/dll/creatures.cpp
    src/dll/cooldowns.cpp
    src/dll/spells.cpp
    src/dll/shooter.cpp
    src/dll/tools.cpp
    src/dll/containers.cpp
    src/dll/sniff.cpp
    src/dll/overlay.cpp
    src/dll/ui.cpp
)
target_include_directories(vtbot PRIVATE src/dll)
target_link_libraries(vtbot PRIVATE psapi imgui d3d11 dxgi d3dcompiler dwmapi)
set_target_properties(vtbot PROPERTIES PREFIX "" OUTPUT_NAME "vtcore")

# offsets.ini junto a los binarios
add_custom_command(TARGET vtbot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/offsets.ini $<TARGET_FILE_DIR:vtbot>/offsets.ini)
