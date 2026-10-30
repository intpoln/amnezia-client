# Заметки и память агентов

Этот файл читает каждый агент перед работой и дополняет, когда находит что-то неочевидное. Пишем кратко, по делу, со ссылками на код `путь:строка`. Устаревшее удаляем.

## Правила работы в форке
1. **Только Windows.** Android-код (`client/android/`, `client/platforms/android/`) не меняем.
2. **На основном ПК не запускаем VPN**: ни собранный клиент, ни службу, ни драйвер. Подробности в [TESTING.md](TESTING.md). Агент собирает проект и гоняет unit-тесты. Реальное подключение проверяет пользователь: выключает официальный клиент и ставит сборку форка. ВМ нет.
3. **Стиль upstream:** правила из `AGENTS.md` действуют (без комментариев в коде, DRY, Controls2, `qsTr`, `.clang-format`, без абсолютных локальных путей и секретов в репозитории).
4. **Коммиты** делаются в `feat/*`, `fix/*` или `custom` (не в `dev`), с префиксом `feat(...)`/`fix(...)`/`docs(fork)`/`ci(fork)`. **Push** только по просьбе пользователя.
5. **После каждой задачи обновлять** [STATUS.md](STATUS.md), чекбоксы в [PLAN.md](PLAN.md), а при новых решениях и [DECISIONS.md](DECISIONS.md).
6. **Переводы.** Новые строки идут через `qsTr`. Для форка нужен как минимум русский перевод в `client/translations/amneziavpn_ru_RU.ts`. Остальные языки можно оставить без перевода: правило upstream «все языки заполнены» в форке ослаблено.

## Карта кода: раздельное туннелирование приложений (Windows)
- **UI:** `client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml`
  - около строки 25: страница заблокирована при активном подключении;
  - около строки 224: на Windows кнопка «+» вызывает `SystemController.getFileName(...)`.
- **Диалог выбора файла:** `client/ui/qml/main2.qml` около строки 329 (`FileDialog`), `client/ui/controllers/systemController.cpp` около строки 110. Ждёт результат во вложенном `QEventLoop`.
- **UI-контроллер:** `client/ui/controllers/appSplitTunnelingUiController.cpp`. `addApp(path)` создаёт `InstalledAppInfo{appName, packageName="", appPath}`.
- **Core-контроллер:** `client/core/controllers/appSplitTunnelingController.cpp`, проверка дублей и сохранение.
- **Структура:** `client/core/utils/commonStructs.h` (`InstalledAppInfo`). `operator==` сравнивает `packageName`, иначе `appPath`, с учётом регистра.
- **Модель списка:** `client/ui/models/appSplitTunnelingModel.cpp`. Роль `appPath` на самом деле отдаёт `appName`.
- **Хранение:** `client/core/repositories/secureAppSettingsRepository.cpp` около строк 211–270. Ключи `Conf/appsRouteMode`, `Conf/ExceptApps`, `Conf/appsSplitTunnelingEnabled`, значение — JSON-массив `{appName, packageName, appPath}`.
- **Сборка конфигурации подключения:** `client/vpnConnection.cpp`, `appendSplitTunnelingConfig()`, около строк 500–516. Пишет `splitTunnelApps` и `appSplitTunnelType`, работает в потоке VPN при каждом подключении. **Сюда встраивается разворот папок в список exe.**
- **Доставка в службу:**
  - WG/AWG: `client/mozilla/localsocketcontroller.cpp`, JSON `vpnDisabledApps` по `\\.\pipe\amneziavpn`, далее `client/daemon/daemon.cpp`;
  - OpenVPN/Xray: QtRO `enablePeerTraffic` → `service/server/killswitch.cpp` около строк 229–297.
- **Применение:** `client/platforms/windows/daemon/windowsdaemon.cpp` (`activateSplitTunnel`, около строки 64) → `client/platforms/windows/daemon/windowssplittunnel.cpp`. Там `excludeApps()` (около строки 293), перевод путей в `\Device\HarddiskVolumeN\...` в `convertPath()`, IOCTL драйвера.
- **Тесты:** `client/tests/testUiAppSTModelAndController.cpp`.

## Драйвер Mullvad: что важно знать
- Подписанный бинарник v1.2.5.0 скачивается рецептом `recipes/win-split-tunnel/conanfile.py`. Исходники: https://github.com/mullvad/win-split-tunnel
- Сравнивает полный путь образа процесса. Папок, масок и regex нет.
- Дочерние процессы исключённого процесса тоже исключаются: если исключить лаунчер, исключаются и игры из него.
- Только режим исключения.
- IP адаптеров передаются один раз в `start()` (`IOCTL_REGISTER_IP_ADDRESSES`, около строки 370 в `windowssplittunnel.cpp`). Документация Mullvad требует обновлять их при смене сети.
- Уже запущенные приложения нужно перезапустить.
- DNS исключённых приложений идёт через `dnscache`, то есть через туннель.
- Не работает рядом с установленным Mullvad VPN (`detectConflict`).

## Известные проблемы стабильности (кандидаты на этап 2)
- `client/vpnConnection.cpp` около строки 126: на Connected вызывается `resetIpStack()`, а служба выполняет `netsh int ip reset` и `netsh winsock reset` (`service/server/router_win.cpp` около строки 291). Синхронно, при каждом подключении.
- `VpnProtocol::startTimeoutTimer()` (`client/core/protocols/vpnProtocol.cpp:66`) нигде не вызывается, поэтому таймаута подключения нет.
- `client/mozilla/localsocketcontroller.cpp` около строк 466–497: ошибки split tunnel только пишутся в лог (`REPORTERROR` закомментирован).
- `windowstunnelservice.cpp:287`, `windowsservicemanager.cpp:70`: циклы ожидания со `Sleep(1000)`.
- `ConnectionController::lastConnectionError()` читает `m_vpnProtocol` из другого потока. Гонка, возможен вылет.
- `vpnConnection.cpp` около строки 386: `connect` на `networkChanged`/`wakeup` повторяется при каждом подключении, без `Qt::UniqueConnection`.
- Служба однопоточная: одна медленная операция блокирует все остальные.
- Полезные открытые PR в upstream: #3344, #3099 (драйвер), #3114 (большие списки маршрутов), #3313 (переподключение), #2431 (блокировки UI).

## Локальная сборка
Всё нужное для сборки, кроме VS Build Tools 2022, лежит в папке `tools/` **рядом с репозиторием** (не в нём). В систему ничего не ставится:
- `tools/venv`: Python venv с Conan 2.28.0, CMake, Ninja, aqtinstall 3.3.0;
- `tools/Qt/6.10.1/msvc2022_64` и `tools/Qt/Tools/QtInstallerFramework`;
- `tools/conan_home`: кэш Conan (`CONAN_HOME`), туда же собираются зависимости.

Сборка (из корня репозитория, обычный `cmd`, окружение MSVC `deploy\build.bat` настраивает сам):
```bat
fork\scripts\local-build.bat
fork\scripts\local-build.bat --installer ifw
```
- Скрипт выставляет `PATH`, `CONAN_HOME`, `QT_INSTALL_DIR`, `AMNEZIA_BUILD_TESTS=ON` и вызывает `deploy\build.bat` со всеми аргументами. Другая папка инструментов задаётся переменной `AMNEZIA_TOOLS`.
- Восстановить `tools/` с нуля:
  ```bash
  python -m venv tools/venv
  tools/venv/Scripts/pip install --no-cache-dir "conan==2.28.0" cmake ninja "aqtinstall==3.3.0"
  tools/venv/Scripts/aqt install-qt windows desktop 6.10.1 win64_msvc2022_64 -m qtremoteobjects qt5compat qtshadertools qthttpserver qtwebsockets -O tools/Qt
  tools/venv/Scripts/aqt install-tool windows desktop tools_ifw -O tools/Qt
  ```
- WiX локально не ставим, `.msi` собирает только CI.
- Unit-тесты: `fork\scripts\local-test.bat` (см. [TESTING.md](TESTING.md)).
- Первая сборка долгая (около часа): готовые пакеты на `artifactory.amnezia.org` собраны под MSVC 19.51 (`compiler.version=195`, как в CI), а локально стоит VS Build Tools 2022 с MSVC 19.44 (`194`). Поэтому OpenSSL и OpenVPN собираются из исходников. Результат кэшируется в `tools/conan_home`, следующие сборки быстрые.
- Баг upstream в `deploy\build.bat`: `VS_INSTALLER_PATH` задаётся и раскрывается через `%...%` в одном блоке `( )`, поэтому без заранее заданной переменной vcvars не вызывается. Тогда сборка OpenVPN падает с `No message compiler found` (`mc.exe` из Windows SDK не в `PATH`). `local-build.bat` задаёт `VS_INSTALLER_PATH` заранее. В CI баг не виден, там окружение MSVC настроено до сборки.
- Конфигурация собирается в `deploy\build`, тесты лежат в `deploy\build\client\tests`.
- Готовые зависимости Conan скачиваются с `artifactory.amnezia.org` без авторизации.
- Без `PROD_AGW_PUBLIC_KEY` не работает только Premium/Free, это нормально.

## Журнал находок
Сюда добавляются неочевидные вещи, найденные в ходе работы: дата и одна-две строки.
- 2026-10-30: unit-тесты пишут настройки под отдельной тестовой организацией (`SecureQSettings(testOrg, ...)`), поэтому реальные настройки установленного AmneziaVPN не трогают. Тест `ConnectionTest` и тесты из `testUtils.cpp` берут секреты `THIRD_PARTY_*` из окружения, в форке их не запускаем.
- 2026-10-30: `deploy.yml` из upstream в форке отключён: его jobs `android-runner` и `Bake-Prebuilts` с секретами в форке не работают. Вместо него `fork-windows.yml`.
- 2026-10-30: `.gitignore` upstream игнорирует `*build-*`, поэтому скрипты с именами вида `build-xxx` не попадут в git. Скрипт сборки назван `local-build.bat`.
- 2026-10-31: тесты экспорта (`UiIpModelAndControllerTest`, `UiAllowedDnsModelAndControllerTest`) через `SystemController::saveFile` открывают папку файла в Проводнике (`systemController.cpp`, `QDesktopServices::openUrl`). Временная папка к этому моменту удалена, и у пользователя всплывает окно «Расположение недоступно». Поэтому `local-test.bat` запускает тесты с `QT_QPA_PLATFORM=offscreen`. Тесты не запускать напрямую без этой переменной.
