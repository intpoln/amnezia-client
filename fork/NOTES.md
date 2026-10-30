# Заметки и память агентов

Этот файл читает каждый агент перед работой и дополняет, когда находит что-то неочевидное. Пишем кратко, по делу, со ссылками на код `путь:строка`. Устаревшее удаляем.

## Правила работы в форке
1. **Только Windows.** Android-код (`client/android/`, `client/platforms/android/`) не меняем.
2. **На основном ПК не запускаем VPN**: ни собранный клиент, ни службу, ни драйвер. Подробности в [TESTING.md](TESTING.md). Агент собирает проект и гоняет unit-тесты, а реальное подключение проверяет пользователь в ВМ.
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
Состояние инструментов на ПК пользователя (2026-10-30):
- есть VS Build Tools 2022 и Python 3.13;
- нет Qt, CMake, Conan, Ninja.

Порядок установки (выполнять после согласия пользователя на скачивание):
```bash
pip install "conan==2.28.0" cmake ninja "aqtinstall==3.3.0"
aqt install-qt windows desktop 6.10.1 win64_msvc2022_64 -m qtremoteobjects qt5compat qtshadertools qthttpserver qtwebsockets -O <QT_DIR>
```
Затем в «x64 Native Tools Command Prompt for VS 2022»:
```bat
set QT_INSTALL_DIR=<QT_DIR>
set AMNEZIA_BUILD_TESTS=ON
deploy\build.bat
```
- Конфигурация собирается в `deploy\build`, тесты лежат в `deploy\build\client\tests`.
- Готовые зависимости Conan скачиваются с `artifactory.amnezia.org` без авторизации.
- Без `PROD_AGW_PUBLIC_KEY` не работает только Premium/Free, это нормально.

## Журнал находок
Сюда добавляются неочевидные вещи, найденные в ходе работы: дата и одна-две строки.
- 2026-10-30: unit-тесты пишут настройки под отдельной тестовой организацией (`SecureQSettings(testOrg, ...)`), поэтому реальные настройки установленного AmneziaVPN не трогают. Тест `ConnectionTest` и тесты из `testUtils.cpp` берут секреты `THIRD_PARTY_*` из окружения, в форке их не запускаем.
- 2026-10-30: `deploy.yml` из upstream в форке отключён: его jobs `android-runner` и `Bake-Prebuilts` с секретами в форке не работают. Вместо него `fork-windows.yml`.
