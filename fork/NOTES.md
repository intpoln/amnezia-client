# Заметки и память агентов

Этот файл читает каждый агент перед работой и дополняет, когда находит что-то неочевидное. Пишем кратко, по делу, со ссылками на код `путь:строка`. Устаревшее удаляем.

## Правила работы в форке
1. **Только Windows.** Android-код (`client/android/`, `client/platforms/android/`) не меняем.
2. **На основном ПК не запускаем VPN**: ни собранный клиент, ни службу, ни драйвер. Подробности в [TESTING.md](TESTING.md). Агент собирает проект и гоняет unit-тесты. Реальное подключение проверяет пользователь: выключает официальный клиент и ставит сборку форка. ВМ нет.
3. **Стиль upstream:** правила из `AGENTS.md` действуют (без комментариев в коде, DRY, Controls2, `qsTr`, `.clang-format`, без абсолютных локальных путей и секретов в репозитории).
4. **Коммиты** делаются в `feat/*`, `fix/*` или `custom` (не в `dev`), с префиксом `feat(...)`/`fix(...)`/`docs(fork)`/`ci(fork)`. Пользователь просит коммитить по ходу работы и пушить ветку задачи в `origin` (на push запускается CI).
5. **После каждой задачи обновлять** [STATUS.md](STATUS.md), чекбоксы в [PLAN.md](PLAN.md), а при новых решениях и [DECISIONS.md](DECISIONS.md).
6. **Переводы.** Новые строки идут через `qsTr`. Для форка нужен как минимум русский перевод в `client/translations/amneziavpn_ru_RU.ts`. Остальные языки можно оставить без перевода: правило upstream «все языки заполнены» в форке ослаблено.

## Карта кода: раздельное туннелирование приложений (Windows)
- **UI:** `client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml`
  - около строки 25: страница заблокирована при активном подключении;
  - около строки 253: на Windows кнопка «+» вызывает `SystemController.getFileName(...)`;
  - кнопка «Добавить папку» в шапке списка открывает `FolderDialog`.
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
- **Папки (форк):** сканер `client/core/utils/folderExecutables.cpp` (`find`, `expand`). Хранение в `Conf/ExceptAppsFolders` (`vpnAppFolders`/`setVpnAppFolders` в `secureAppSettingsRepository.cpp`). В модели папки идут строками после приложений (роли `isFolder`, `fullPath`), поэтому `removeApp(index)` в UI-контроллере удаляет папку при `index >= числа приложений`. Разворот в exe в `appendAppSplitTunnelingConfig()` (вызывается из `appendSplitTunnelingConfig()`), в лог пишется число папок и exe. При автопереподключении (`reconnectToVpn`, сон и смена сети) список пересобирается и передаётся протоколу через `VpnProtocol::setAppSplitTunnelingConfig`, остальной конфиг протокола (с уже разрешённым адресом сервера) не трогается. Сетевые папки (`\\server\share`) не принимаются: служба (`convertPath` в `windowssplittunnel.cpp`) отбрасывает пути без буквы диска. При восстановлении копии без ключа папок папки очищаются (`restoreAppConfig` в репозитории).
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

## Карта кода: подключение и стабильность (этап 2)
- **Потоки.** `VpnConnection` живёт в своём `QThread` (`amneziaApplication.cpp`, `m_vpnConnectionThread`), `ConnectionController` и UI — в главном. Всё, что UI читает у `VpnConnection`, передаётся сигналом, а не чтением полей из другого потока.
- **Состояние.** `VpnConnection::setConnectionState` сначала отправляет состояние в UI (`connectionStateChanged`), потом делает вызовы службы (`onConnectionStateChanged`: DNS, маршруты). Disconnected во время Reconnecting в UI не уходит, но вызовы службы для него выполняются.
- **Вызовы службы без ожидания.** `VpnConnection::watchIpcReply(reply, message)` вешает `QRemoteObjectPendingCallWatcher` и пишет предупреждение при ошибке. `waitForFinished()` в `vpnConnection.cpp` не использовать: вложенный цикл событий приводил к переполнению стека (см. комментарий в `addSitesRoutes`). Синхронные вызовы остались в `xrayProtocol.cpp` и `ipcClient.cpp`.
- **Таймаут подключения.** `connectionTimeoutMsec = 40000` в `vpnConnection.cpp`. Таймер `m_connectionTimeoutTimer` идёт в Connecting и Reconnecting (только desktop), по таймауту `onConnectionTimeout` ставит ошибку `ConnectionTimeoutError` и останавливает протокол.
- **Ошибки форка** в `errorCodes.h` с номера 750: `ConnectionTimeoutError` (750), `TunnelError` (751), `SplitTunnelDriverError` (752). Тексты в `errorStrings.cpp`, перевод в `amneziavpn_ru_RU.ts`.
- **Ошибка до UI.** При переходе в Error `VpnConnection` шлёт `lastErrorChanged(error)` перед `connectionStateChanged`, `ConnectionController` хранит её в `m_lastConnectionError`, `lastConnectionError()` отдаёт сохранённое значение.
- **Ошибки службы WG/AWG.** Служба шлёт JSON `backendFailure` с `DaemonError` (`daemonlocalserverconnection.cpp`, в том числе при неверном конфиге и неудачной активации). `LocalSocketController` превращает его в сигнал `ControllerImpl::backendFailure(DaemonError)`, `WireguardProtocol` ставит `SplitTunnelDriverError` для ошибок split tunnel и `TunnelError` для остальных.
- **Замеры.** В логе клиента строки `Connection timing: <этап> at N ms` (от начала подключения, переподключения или отключения) и `state <S>, service calls took N ms`. В логе службы `Interface activated in N ms`, `The service reached the status in N ms`.
- **Смена сети и сон (служба).** `client/platforms/windows/windowsnetworkwatcher.cpp`:
  - окно `PowerMonitorClass` ловит `PBT_APMRESUMESUSPEND` и шлёт `wakeup`; создаётся до WLAN, поэтому работает и на ПК без Wi-Fi;
  - `NotifyRouteChange2` следит за маршрутами `0.0.0.0/0`. Через 2 с после изменений (`checkNetwork`) сравнивается подпись «LUID + шлюз» маршрутов по умолчанию на физических адаптерах (`HardwareInterface`, `IfOperStatusUp`). Адаптеры VPN (wintun, TAP) не физические, поэтому подключение VPN переподключения не вызывает. Пустая подпись (сеть пропала) не запоминается;
  - смена BSSID Wi-Fi идёт через ту же задержку, поэтому одно событие сети даёт один `networkChanged`.
  Сигналы уходят в клиент через `IpcServer` (`service/server/localserver.cpp`), клиент вызывает `reconnectToVpn` (`Qt::UniqueConnection`).
- **IP адаптеров для драйвера (2.7).** Переподключение заново выполняет `activate` в службе, а он заново определяет физический адаптер и передаёт драйверу его IP. Отдельно обновлять IP не нужно. Не покрыт случай, когда DHCP меняет IP того же адаптера без смены маршрута по умолчанию.
- **Служба однопоточная:** одна медленная операция блокирует все остальные.
- **Перенесено из upstream (2.9):** PR #3344, #3099, #3114 взяты коммитами с `-x` (в сообщении ссылка на исходный коммит), из #3313 часть (`windowstunnelpipe.h`, `LocalServer::shutdown`). Когда upstream их примет, при merge `dev` конфликты в этих местах разрешать в пользу upstream. Подробности в [DECISIONS.md](DECISIONS.md).

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
- `local-build.bat` вызывает `tools/agw/gateway-env.bat`, если он есть. Без него сборка проходит, но Premium/Free не работают (см. ниже).

## Ключ шлюза Premium
- Клиент берёт три значения из окружения сборки (`client/CMakeLists.txt`): `PROD_AGW_PUBLIC_KEY` (PEM), `PROD_S3_ENDPOINT` и `FALLBACK_S3_ENDPOINT` (списки адресов, `GatewayController` разбирает их через `splitEndpoints`). Остальные (`DEV_*`, `FREE_V2_ENDPOINT`, `PREM_V1_ENDPOINT`) в прод-режиме не нужны. Адрес шлюза по умолчанию прописан в коде.
- **Откуда:** из официального `AmneziaVPN.exe`. В `.rdata` два PEM-ключа, dev и prod. Выбор в `GatewayController` скомпилирован в `lea rax,[dev]; lea rdx,[prod]; test bl,bl; cmovne rdx,rax`, то есть prod грузится в `rdx` по умолчанию. Списки S3 — две строки, на которые ссылается ветка не-dev после этого выбора: первая — основной список, вторая — запасной. Искать через `dumpbin /disasm:nobytes`: `cmovne` рядом со ссылками на оба PEM.
- **Локально:** `tools/agw/gateway-env.bat`, три строки `set "ИМЯ=значение"`. Переводы строк в PEM записаны как `\n`: значение уходит в `-D` define, и компилятор превращает их в переводы строк.
- **CI:** в репозитории форка на GitHub: Settings → Secrets and variables → Actions → New repository secret. Три секрета с теми же именами и значениями из `gateway-env.bat` (ключ — одной строкой с `\n`, как в файле). `fork-windows.yml` передаёт их в шаг сборки. Без секретов CI собирает клиент без Premium.
- Значения не коммитить и не выводить в чат или логи.

## Журнал находок
Сюда добавляются неочевидные вещи, найденные в ходе работы: дата и одна-две строки.
- 2026-10-30: unit-тесты пишут настройки под отдельной тестовой организацией (`SecureQSettings(testOrg, ...)`), поэтому реальные настройки установленного AmneziaVPN не трогают. Тест `ConnectionTest` и тесты из `testUtils.cpp` берут секреты `THIRD_PARTY_*` из окружения, в форке их не запускаем.
- 2026-10-30: `deploy.yml` из upstream в форке отключён: его jobs `android-runner` и `Bake-Prebuilts` с секретами в форке не работают. Вместо него `fork-windows.yml`.
- 2026-10-30: `.gitignore` upstream игнорирует `*build-*`, поэтому скрипты с именами вида `build-xxx` не попадут в git. Скрипт сборки назван `local-build.bat`.
- 2026-10-31: код из Mozilla (`client/mozilla/`, `client/daemon/`, `client/platforms/windows/`) написан в стиле с отступом 2 пробела. `.clang-format` репозитория к нему не применять: он переформатирует весь блок. Правки в этих файлах пишем в их стиле вручную.
- 2026-10-31: `errorStrings.cpp` держит каждый `case` в одну строку, `.clang-format` их разбивает. Не форматировать этот файл.
- 2026-10-31: тесты экспорта (`UiIpModelAndControllerTest`, `UiAllowedDnsModelAndControllerTest`) через `SystemController::saveFile` открывают папку файла в Проводнике (`systemController.cpp`, `QDesktopServices::openUrl`). Временная папка к этому моменту удалена, и у пользователя всплывает окно «Расположение недоступно». Поэтому `local-test.bat` запускает тесты с `QT_QPA_PLATFORM=offscreen`. Тесты не запускать напрямую без этой переменной.
