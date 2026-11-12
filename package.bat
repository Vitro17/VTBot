@echo off
setlocal
cd /d "%~dp0"
call build.bat || exit /b 1
set OUT=%~dp0release\VTBot
if exist "%~dp0release" rmdir /s /q "%~dp0release"
mkdir "%OUT%" || exit /b 1
copy /y build\bin\vtloader.exe "%OUT%\" >nul || exit /b 1
copy /y build\bin\vtcore.dll "%OUT%\" >nul || exit /b 1
copy /y offsets.ini "%OUT%\" >nul || exit /b 1
copy /y "dist\Iniciar VTBot.bat" "%OUT%\" >nul || exit /b 1
copy /y "dist\Descargar VTBot.bat" "%OUT%\" >nul || exit /b 1
copy /y dist\LEEME.txt "%OUT%\" >nul || exit /b 1
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%' -DestinationPath '%~dp0release\VTBot.zip' -Force" || exit /b 1
echo.
echo PAQUETE LISTO: %~dp0release\VTBot.zip
