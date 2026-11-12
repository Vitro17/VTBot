@echo off
title VTBot - Iniciar
cd /d "%~dp0"
tasklist /fi "imagename eq client.exe" 2>nul | find /i "client.exe" >nul
if errorlevel 1 (
    echo No se encontro client.exe. Abre Tibia, entra con tu personaje y vuelve a ejecutar esto.
    echo.
    pause
    exit /b 1
)
del /q vtbot_*.dll >nul 2>nul
vtloader.exe
echo.
if errorlevel 1 (
    echo Si dice "OpenProcess fallo", haz clic derecho en este archivo y elige "Ejecutar como administrador".
) else (
    echo Listo: en el cliente aparece el boton VT debajo del boton PvP, y la ventana del bot.
)
echo.
pause
