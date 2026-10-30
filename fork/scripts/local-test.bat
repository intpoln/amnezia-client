@echo off
setlocal

for %%I in ("%~dp0..\..") do set "REPO_DIR=%%~fI"
if not defined AMNEZIA_TOOLS for %%I in ("%REPO_DIR%\..\tools") do set "AMNEZIA_TOOLS=%%~fI"

set "TESTS=%~1"
if not defined TESTS set "TESTS=^(SettingsSignalsTest|UiServersModelAndControllerTest|XrayServerInboundTest|XrayGeneratorMatrixTest|XrayConfigModelTest|ConfigFormatVersionTest|UiLanguageModelAndControllerTest|UiIpModelAndControllerTest|UiAppSTModelAndControllerTest|UiAllowedDnsModelAndControllerTest|FavoriteLocationsRestoreTest|OpenSSLLibraryTest|LogsControllerTest)$"

set "QT_FORCE_STDERR_LOGGING=1"
set "PATH=%AMNEZIA_TOOLS%\venv\Scripts;%PATH%;%REPO_DIR%\client\3rd-prebuilt\deploy-prebuilt\windows\x64;%AMNEZIA_TOOLS%\Qt\6.10.1\msvc2022_64\bin"

cd /d "%REPO_DIR%"
ctest --test-dir deploy\build\client\tests -C Release --output-on-failure --timeout 600 -R "%TESTS%"
