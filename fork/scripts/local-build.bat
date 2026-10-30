@echo off
setlocal

for %%I in ("%~dp0..\..") do set "REPO_DIR=%%~fI"
if not defined AMNEZIA_TOOLS for %%I in ("%REPO_DIR%\..\tools") do set "AMNEZIA_TOOLS=%%~fI"

if not exist "%AMNEZIA_TOOLS%\venv\Scripts\conan.exe" (
    echo ERROR: build tools not found in "%AMNEZIA_TOOLS%". See fork\NOTES.md for setup.
    exit /b 1
)

set "PATH=%AMNEZIA_TOOLS%\venv\Scripts;%PATH%"
set "CONAN_HOME=%AMNEZIA_TOOLS%\conan_home"
set "QT_INSTALL_DIR=%AMNEZIA_TOOLS%"
set "QT_VERSION=6.10.1"
set "AMNEZIA_BUILD_TESTS=ON"
:: deploy\build.bat expands VS_INSTALLER_PATH in the same block that sets it, so vcvars is skipped unless it is preset
if not defined VS_INSTALLER_PATH set "VS_INSTALLER_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

cd /d "%REPO_DIR%"
call deploy\build.bat %*
