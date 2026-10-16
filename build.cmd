@echo off
rem Builds ClaudeProfileSwitcher.exe with the Visual Studio C++ tools.
rem Static runtime: the exe needs nothing installed, not even the VC++ redistributable.
rem
rem   build.cmd                     x64   -> build\x64\ClaudeProfileSwitcher.exe
rem   build.cmd arm64               ARM64 -> build\arm64\ClaudeProfileSwitcher.exe
rem   build.cmd x64 <out.exe>       to another path
setlocal
cd /d "%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
if /i not "%ARCH%"=="x64" if /i not "%ARCH%"=="arm64" (echo Unknown architecture "%ARCH%", use x64 or arm64. & exit /b 1)
set "OUT=%~2"
if "%OUT%"=="" set "OUT=build\%ARCH%\ClaudeProfileSwitcher.exe"

rem A Developer Command Prompt (or CI with msvc-dev-cmd) already has cl on PATH for its target.
if defined VSCMD_ARG_TGT_ARCH if /i "%VSCMD_ARG_TGT_ARCH%"=="%ARCH%" goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio with the "Desktop development with C++" workload is required. & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo The MSVC build tools were not found. & exit /b 1)
set "HOST=x64"
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "HOST=arm64"
set "TARGET=%HOST%_%ARCH%"
if /i "%HOST%"=="%ARCH%" set "TARGET=%ARCH%"
call "%VS%\VC\Auxiliary\Build\vcvarsall.bat" %TARGET% >nul 2>nul || (echo Could not set up the %ARCH% tools. For ARM64 install "MSVC ARM64 build tools". & exit /b 1)

:build
set "OBJ=build\obj\%ARCH%"
if not exist "%OBJ%" mkdir "%OBJ%"
for %%d in ("%OUT%") do if not exist "%%~dpd" mkdir "%%~dpd"
rc /nologo /fo "%OBJ%\app.res" src\app.rc || exit /b 1
cl /nologo /std:c++17 /O1 /GL /MT /EHsc /utf-8 /W3 /DNDEBUG /Fo"%OBJ%\\" ^
   src\main.cpp src\app.cpp src\core.cpp src\shell.cpp src\gfx.cpp src\picker.cpp src\flyout.cpp src\settings.cpp src\dialogs.cpp "%OBJ%\app.res" ^
   /link /OPT:REF /OPT:ICF /LTCG /SUBSYSTEM:WINDOWS /MANIFEST:NO /out:"%OUT%" ^
   user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib dwmapi.lib d2d1.lib dwrite.lib windowscodecs.lib ^
   advapi32.lib comctl32.lib comdlg32.lib propsys.lib shlwapi.lib || exit /b 1
echo Built %OUT% (%ARCH%)
