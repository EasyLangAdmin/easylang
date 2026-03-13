@echo off
:: ============================================================
::  EasyLang (EL1) Windows Installer
::  Builds and installs the `el` global command
:: ============================================================
setlocal EnableDelayedExpansion

echo.
echo   ███████╗██╗     ██╗
echo   ██╔════╝██║     ╚═╝
echo   █████╗  ██║     ██╗
echo   ██╔══╝  ██║     ██║
echo   ███████╗███████╗██║
echo   ╚══════╝╚══════╝╚═╝  EasyLang Installer v1.0
echo.

:: ── Check for MSVC or MinGW ─────────────────────────────────
where cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    set CXX=cl
    goto :compile_msvc
)
where g++ >nul 2>&1
if %ERRORLEVEL% == 0 (
    set CXX=g++
    goto :compile_gcc
)
echo [ERROR] No C++ compiler found. Install Visual Studio Build Tools or MinGW.
exit /b 1

:compile_gcc
echo [... ] Compiling with g++...
set "SRC=%~dp0src\main.cpp"
g++ -O3 -std=c++17 -o "%TEMP%\el.exe" "%SRC%"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Compilation failed. & exit /b 1 )
goto :install

:compile_msvc
echo [... ] Compiling with MSVC...
set "SRC=%~dp0src\main.cpp"
cl /O2 /EHsc /std:c++17 /Fe:"%TEMP%\el.exe" "%SRC%"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Compilation failed. & exit /b 1 )
goto :install

:install
set "INSTALL_DIR=%SystemRoot%\System32"
echo [... ] Installing to %INSTALL_DIR%\el.exe ...
copy /Y "%TEMP%\el.exe" "%INSTALL_DIR%\el.exe" >nul
if %ERRORLEVEL% NEQ 0 (
    echo [WARN] Need admin rights. Trying with elevation...
    powershell -Command "Start-Process cmd -ArgumentList '/c copy %TEMP%\el.exe %INSTALL_DIR%\el.exe' -Verb RunAs"
)

:: Register .el file association
echo [... ] Registering .el file type...
reg add "HKCU\Software\Classes\.el" /ve /d "EasyLangScript" /f >nul 2>&1
reg add "HKCU\Software\Classes\EasyLangScript" /ve /d "EasyLang Script" /f >nul 2>&1
reg add "HKCU\Software\Classes\EasyLangScript\shell\open\command" /ve /d "\"%INSTALL_DIR%\el.exe\" \"%%1\" %%*" /f >nul 2>&1

echo.
echo [OK] EasyLang EL1 installed!
echo.
echo   Run:   el script.el
echo   Help:  el
echo.
pause
