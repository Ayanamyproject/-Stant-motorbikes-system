@echo off
chcp 65001 >nul
title Stunt Bikes - local test
rem Local MP test: DayZDiag dedicated server + DayZDiag client (no BattlEye).
rem Builds the mod, signs it, starts a separate server instance (own storage + profile)
rem and joins it. Adjust the paths below once.

rem ===================== settings =====================
set "GAME=D:\SteamLibrary\steamapps\common\DayZ"
set "SERVER=D:\SteamLibrary\steamapps\common\DayZServer"
set "DSUTILS=D:\SteamLibrary\steamapps\common\DayZ Tools\Bin\DsUtils"
set "KEY=%~dp0..\keys"
set "PORT=2302"
set "MISSION=dayzOffline.chernarusplus"
rem ====================================================

set "REPO=%~dp0.."
set "DIST=%REPO%\dist"

taskkill /im DayZDiag_x64.exe /f >nul 2>nul
timeout /t 4 /nobreak >nul

python "%REPO%\tools\build.py" --test --sign "%KEY%" --dsutils "%DSUTILS%" || (pause & exit /b 1)

rem separate server config: own storage (instanceId 77) so your normal server data is never touched
if not exist "%SERVER%\serverDZ_stuntbikes.cfg" (
  powershell -NoProfile -Command "(Get-Content '%SERVER%\serverDZ.cfg') -replace 'instanceId\s*=\s*\d+;','instanceId = 77;' | Set-Content '%SERVER%\serverDZ_stuntbikes.cfg'; Add-Content '%SERVER%\serverDZ_stuntbikes.cfg' 'BattlEye = 0;'"
)
copy /y "%DIST%\@StuntBikes\keys\*.bikey" "%SERVER%\keys\" >nul

if not exist "%SERVER%\profiles_stuntbikes\StuntBikes" mkdir "%SERVER%\profiles_stuntbikes\StuntBikes"
if not exist "%SERVER%\profiles_stuntbikes\StuntBikes\config.json" (
  copy /y "%REPO%\config.example.json" "%SERVER%\profiles_stuntbikes\StuntBikes\config.json" >nul
)

start "Stunt Bikes server" /min "%GAME%\DayZDiag_x64.exe" -server "-config=%SERVER%\serverDZ_stuntbikes.cfg" "-mission=%SERVER%\mpmissions\%MISSION%" -port=%PORT% "-profiles=%SERVER%\profiles_stuntbikes" "-mod=%DIST%\@StuntBikes" "-serverMod=%DIST%\@StuntBikesTest" -dologs -adminlog -freezecheck
timeout /t 40 /nobreak >nul

if not exist "%SERVER%\client_profile_stuntbikes" mkdir "%SERVER%\client_profile_stuntbikes"
start "" /d "%GAME%" "%GAME%\DayZDiag_x64.exe" "-mod=%DIST%\@StuntBikes" "-profiles=%SERVER%\client_profile_stuntbikes" -connect=127.0.0.1 -port=%PORT% -nosplash -skipIntro -window -name=StuntTester
