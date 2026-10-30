**Личный форк имеет смысл: основные неудобства можно исправить без переписывания VPN-ядра.** Выбор папок на Windows реализуем поверх существующего драйвера. Главная сложность — автоматическое применение новых файлов и надёжность переходов между состояниями подключения.

Исследована **локальная рабочая копия**, а не только публичный upstream. Например, здесь Cloak уже помечен неподдерживаемым. Ниже ссылки относятся к текущим локальным файлам; сведения о других клиентах проверены по публичной документации и исходникам.

**Ограничение исследования:** приложение не запускалось, задержки и утечки трафика не измерялись. Подтверждённые конструкции в коде отделены от предполагаемых последствий. Файлы не изменены.

## Архитектура репозитория

### Интерфейс и управление подключением

Основная цепочка:

```text
QML → UI-контроллер → core-контроллер
                         ↓ очередь сигналов
                  VpnConnection — отдельный поток
                         ↓
                  реализация протокола
                         ↓
           Windows service / Android VpnService
```

- **QML** отвечает за страницы и элементы интерфейса.
- **`client/ui/controllers/`** связывает QML с логикой приложения.
- **`client/core/controllers/`** готовит конфигурации, управляет настройками, серверами и импортом.
- **`VpnConnection`** создаёт реализацию протокола, подключает сигналы состояния, запускает и останавливает соединение.

Вынос VPN-логики в поток **уже сделан**: `AmneziaApplication::init()` вызывает `moveToThread()` и запускает поток. `ConnectionController` отправляет команды подключения через `Qt::QueuedConnection`. Поэтому предложение «просто вынести VPN из UI-потока» здесь недостаточно.  
Код: [amneziaApplication.cpp:166](../../client/amneziaApplication.cpp:166), [connectionController.cpp:31](../../client/core/controllers/connectionController.cpp:31).

**Подготовка подключения остаётся отдельным этапом:** `ConnectionUiController::openConnection()` напрямую вызывает core-контроллер, и только затем тот отправляет команду рабочему потоку. Именно в этой подготовке тоже нужно искать задержки.  
Код: [connectionUiController.cpp:28](../../client/ui/controllers/connectionUiController.cpp:28).

### Windows: служба и два механизма IPC

Здесь есть **два канала взаимодействия**, а не только Qt Remote Objects:

| Канал | Назначение |
|---|---|
| **Qt Remote Objects поверх локальных сокетов** | Привилегированные процессы, маршруты, DNS, kill switch, управление Xray |
| **Отдельный локальный сокет с JSON-командами** | Mozilla-derived backend для WireGuard/AmneziaWG: activate/deactivate/status, события и ошибки |

`LocalServer` запускает QtRO и отдельно инициализирует WireGuard daemon server. `IpcClient` использует отдельный экземпляр на поток через `thread_local`.  
Код: [service/server/localserver.cpp:24](../../service/server/localserver.cpp:24), [ipcClient.cpp:6](../../client/core/utils/ipcClient.cpp:6), [localsocketcontroller.cpp:267](../../client/mozilla/localsocketcontroller.cpp:267).

Это важно для исправлений: обработать ошибку QtRO недостаточно — ошибки второго канала тоже должны доходить до общего состояния и интерфейса.

### Реализации протоколов

| Протокол | Реализация в этой копии |
|---|---|
| **WireGuard / AmneziaWG / AWG2** | Общий `WireguardProtocol`, далее `LocalSocketController`, daemon и Windows tunnel service |
| **OpenVPN** | `OpenVpnProtocol`, привилегированный процесс и management interface; отдельная настройка маршрутов/firewall |
| **Xray / VLESS / Shadowsocks через Xray** | `XrayProtocol`, TUN → tun2socks → Xray; Xray запускается через библиотечный API в службе |
| **IKEv2/IPsec** | Отдельная Windows-реализация |
| **Cloak / прежний OpenVPN over Shadowsocks** | В этой копии признаны неподдерживаемыми; нельзя считать их действующими backend |

Фабрика протоколов: [vpnProtocol.cpp:124](../../client/core/protocols/vpnProtocol.cpp:124). Статус старых контейнеров: [containerUtils.cpp:385](../../client/core/utils/containers/containerUtils.cpp:385).

Для Xray существенная деталь: это не просто запуск отдельного `xray.exe`. `service/server/xray.cpp` использует `amnezia_xray.h`, а `Xray::start()` управляет владельцем движка через token.  
Код: [service/server/xray.cpp:34](../../service/server/xray.cpp:34), [xrayProtocol.cpp:95](../../client/core/protocols/xrayProtocol.cpp:95).

### Android

Qt/C++ взаимодействует с Android-частью, затем команды передаются VPN-службе через Android `Messenger`. `AmneziaVpnService` использует:

- `MutableStateFlow` для состояния протокола;
- `Dispatchers.IO` для подключения;
- основной поток для управления службой и уведомлений;
- foreground service;
- отслеживание сети и вызов переподключения;
- запуск через системный Always-on VPN.

В `VpnProto` перечислены отдельные службы/процессы для AWG/WireGuard, OpenVPN и Xray.  
Код: [AmneziaVpnService.kt:137](../../client/android/src/org/amnezia/vpn/AmneziaVpnService.kt:137), [AmneziaVpnService.kt:207](../../client/android/src/org/amnezia/vpn/AmneziaVpnService.kt:207), [VpnProto.kt:14](../../client/android/src/org/amnezia/vpn/VpnProto.kt:14).

### Состояния и восстановление

Состояние проходит через несколько уровней: backend → `VpnProtocol` → `VpnConnection` → UI. Это набор enum, обработчиков и сигналов; единого явно описанного автомата со всеми допустимыми переходами здесь недостаточно.

На desktop смена сети и пробуждение вызывают `reconnectToVpn()`. Он принимает событие только в состоянии `Connected`, затем вызывает `stop()` и сразу `start()`. Android аналогично реагирует на сеть через `NetworkState`, но выполняет переподключение в coroutine.

Код: [vpnConnection.cpp:380](../../client/vpnConnection.cpp:380), [vpnConnection.cpp:557](../../client/vpnConnection.cpp:557), [AmneziaVpnService.kt:560](../../client/android/src/org/amnezia/vpn/AmneziaVpnService.kt:560).

## Как работает раздельное туннелирование сейчас — Windows/Android

### Windows: приложения

**Используется kernel driver Mullvad, работающий с Windows Filtering Platform — WFP.** Это не обычная таблица IP-маршрутов «для каждого процесса».

Прямые подтверждения:

- устройство `\\.\MULLVADSPLITTUNNEL`;
- файл `mullvad-split-tunnel.sys`;
- служба `AmneziaVPNSplitTunnel`;
- передача правил через `DeviceIoControl`;
- правила содержат пути executable, преобразованные в device paths.

Код: [windowssplittunnel.cpp:121](../../client/platforms/windows/daemon/windowssplittunnel.cpp:121), [windowssplittunnel.cpp:293](../../client/platforms/windows/daemon/windowssplittunnel.cpp:293), [windowssplittunnel.cpp:428](../../client/platforms/windows/daemon/windowssplittunnel.cpp:428).

По документации драйвера, он отслеживает процессы, перенаправляет привязку сокетов, блокирует нежелательный проход через туннель и согласует исключения с WFP-фильтрами. Он также поддерживает динамическую конфигурацию и наследование исключения дочерними процессами. **Применимость последнего к конкретной установленной версии бинарника нужно проверить отдельно.** [Архитектура драйвера Mullvad](https://github.com/mullvad/win-split-tunnel).

Текущий путь настройки:

1. Пользователь выбирает один файл.
2. `AppSplitTunnelingUiController::addApp()` сохраняет путь.
3. `VpnConnection::appendSplitTunnelingConfig()` формирует `splitTunnelApps`.
4. Служба передаёт список в `WindowsSplitTunnel::excludeApps()`.

UI прямо ограничивает Windows режимом **«выбранные приложения напрямую»**. Режим **«только выбранные через VPN»** доступен в переключателе лишь Android. Изменения при активном подключении запрещены.  
Код: [PageSettingsAppSplitTunneling.qml:25](../../client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml:25), [PageSettingsAppSplitTunneling.qml:116](../../client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml:116), [PageSettingsAppSplitTunneling.qml:220](../../client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml:220).

Механизм применяется не только к WG: OpenVPN и Xray передают данные через `KillSwitch::enablePeerTraffic()`, который вызывает `WindowsDaemon::activateSplitTunnel()`.  
Код: [killswitch.cpp:229](../../service/server/killswitch.cpp:229), [openVpnProtocol.cpp:362](../../client/core/protocols/openVpnProtocol.cpp:362), [xrayProtocol.cpp:308](../../client/core/protocols/xrayProtocol.cpp:308).

**Почему одной игре всё равно могут требоваться несколько файлов:** дочерний процесс лаунчера и отдельно запущенная служба/античит — разные случаи. Наследование исключений не означает «все программы из папки» и может, наоборот, исключить больше дочерних программ, чем ожидалось.

### Windows: сайты

В общей системе split tunneling домен превращается в IP-адреса, после чего применяются маршруты/диапазоны адресов.

- `IpSplitTunnelingController::addSite()` использует асинхронный `QHostInfo::lookupHost()`.
- Сохраняется соответствие домена и IP.
- `VpnConnection::addSitesRoutes()` добавляет сохранённые маршруты и повторно разрешает домены.
- В показанном callback учитываются IPv4-адреса.
- Для WG/AWG настройка передаётся через конфигурацию daemon; для других desktop-протоколов есть отдельная работа с маршрутами.

Код: [ipSplitTunnelingController.cpp:76](../../client/core/controllers/ipSplitTunnelingController.cpp:76), [vpnConnection.cpp:208](../../client/vpnConnection.cpp:208), [vpnConnection.cpp:405](../../client/vpnConnection.cpp:405).

**Следствие:** это не полноценное правило «любой запрос к этому домену». CDN, смена IP, сторонние домены ресурсов, браузерный DoH и общий IP нескольких сайтов могут давать неожиданный результат. Асинхронный DNS уже присутствует; проблема шире, чем просто «заменить синхронный DNS».

### Android: приложения

Здесь применяется стандартный механизм Android:

- `addAllowedApplication(packageName)` — выбранные пакеты через VPN;
- `addDisallowedApplication(packageName)` — выбранные пакеты исключены.

Код: [Protocol.kt:70](../../client/android/protocolApi/src/main/kotlin/Protocol.kt:70), [Protocol.kt:125](../../client/android/protocolApi/src/main/kotlin/Protocol.kt:125).

ОС принимает имена установленных пакетов, а не пути к файлам. Списки allow/disallow взаимоисключающие; изменение требует создания нового VPN-соединения. При системном режиме «блокировать соединения без VPN» исключённые приложения могут потерять интернет вместо прямого доступа. [Документация Android VPN](https://developer.android.com/develop/connectivity/vpn).

**Иконки, поиск и массовый выбор уже реализованы:**

- `AppListProvider` получает приложения с разрешением INTERNET;
- текущий VPN-клиент исключается из списка;
- приложения с запуском из launcher сортируются выше;
- QML показывает иконки, поиск и кнопку добавления выбранных.

Код: [AppListProvider.kt:16](../../client/android/src/org/amnezia/vpn/AppListProvider.kt:16), [InstalledAppsDrawer.qml:45](../../client/ui/qml/Components/InstalledAppsDrawer.qml:45).

### Android: сайты

Списки преобразуются в включаемые и исключаемые IP-подсети. На Android 13+ используется `excludeRoute()`. На старых версиях исключения реализованы вычислением оставшихся подсетей — соответствующий fallback уже есть.

Код: [Protocol.kt:54](../../client/android/protocolApi/src/main/kotlin/Protocol.kt:54), [ProtocolConfig.kt:116](../../client/android/protocolApi/src/main/kotlin/ProtocolConfig.kt:116).

## Папки вместо .exe — возможно ли и как

**Да. Для первого полезного варианта менять драйвер не требуется.** Но есть три разных уровня обещаний.

| Вариант | Что получает пользователь | Ограничение |
|---|---|---|
| **Разовое сканирование папки** | Все найденные `.exe`, включая подпапки, добавляются одним действием | Будущие файлы не учитываются |
| **Сохранённое правило папки + наблюдение** | Новые/перемещённые `.exe` автоматически попадают в список драйвера | Между появлением файла и обновлением правил существует окно |
| **Нативное правило префикса пути в драйвере** | Новый процесс классифицируется по принадлежности папке при запуске | Изменение kernel driver, подпись и существенно более сложное тестирование |

### Рекомендуемый вариант для личного форка

**Сохранённая папка, фоновое сканирование и автоматическое обновление списка executable.**

Что потребуется:

1. **Модель правил:** различать `Executable` и `Directory`, хранить флаг рекурсивности. Не подменять папку навсегда развёрнутым списком файлов.
2. **UI:** «Добавить приложение», «Добавить папку», предпросмотр найденных файлов и число включённых executable.
3. **Фоновое сканирование:** дедупликация путей, обработка недоступных каталогов, ограничение обхода junction/symlink.
4. **Обновление:** первоначальное сканирование, проверка перед подключением, наблюдение за изменениями и периодическая сверка.
5. **Применение:** сформировать полный набор путей и передать существующему драйверу; корректно обработать пустой набор, удаление папки и отказ драйвера.
6. **Работа без окна GUI:** наблюдение за активными правилами лучше размещать в службе, с ограниченной и проверяемой конфигурацией.

Точки изменения: `AppSplitTunnelingController`, репозиторий настроек, `appendSplitTunnelingConfig()`, IPC для обновления активной политики и Windows daemon. Сейчас правила применяются преимущественно в ходе активации, а UI запрещает редактирование на активном соединении.

**Честное ограничение:** watcher не гарантирует исключение нового `.exe` с первого пакета. До применения правила процесс может идти по общей политике — обычно через VPN. Обновление правил также не обещает бесшовно перенести уже открытые TCP-соединения.

Если нужна строгая семантика «любой будущий executable в этой папке, сразу при старте», необходима классификация пути в драйвере либо иной движок перехвата с подходящими гарантиями.

### Что делать на Android

Аналог папки здесь — **группа package names**, например «Игры» или «Работа». Полезны:

- сохранённые пользовательские наборы;
- «показать выбранные»;
- поиск по названию и package name;
- скрытие системных компонентов по умолчанию;
- отметки удалённых приложений;
- импорт/экспорт списка пакетов;
- понятная команда «Применить и переподключить».

Автоматически угадывать, какие приложения нужно исключить, лучше не делать: набор должен быть видимым и редактируемым.

## Причины неотзывчивости/глюков — с конкретными местами в коде

### 1. Синхронная подготовка до передачи управления VPN-потоку

`ConnectionUiController::openConnection()` напрямую вызывает `ConnectionController::openConnection()`. Для API-конфигураций с `sendPayload` тот синхронно запускает `PayloadSender::sendAll()`.

Внутри есть:

- синхронный DNS;
- `QTcpSocket::waitForConnected(timeoutMs)`;
- вложенный event loop ожидания ответа.

Это **конкретный потенциально блокирующий путь из UI**, но он относится к конфигурациям с таким payload, а не ко всем подключениям.  
Код: [connectionController.cpp:209](../../client/core/controllers/connectionController.cpp:209), [payloadSender.cpp:135](../../client/core/utils/payloadSender.cpp:135).

**Исправление:** асинхронная подготовка с отменой и отдельным состоянием «Подготовка соединения».

### 2. Вложенные event loop усложняют реакцию интерфейса

`GatewayController::post()` ждёт future через `ExcludeUserInputEvents`: при вызове на GUI-потоке обработка пользовательского ввода исключается. Сам сетевой запрос может работать в фоне, но для пользователя кнопки временно не реагируют.

`InstalledAppsModel::updateModel()` запускает фоновую работу, затем ждёт её через вложенный `QEventLoop`. Здесь события продолжают обрабатываться, поэтому это **не равнозначно полной заморозке**, однако остаются модальность, повторный вход в обработчики и задержка открытия списка.

Код: [gatewayController.cpp:130](../../client/core/controllers/gatewayController.cpp:130), [installedAppsModel.cpp:75](../../client/ui/models/installedAppsModel.cpp:75).

**Исправление:** возвращаться из вызова немедленно; завершение доставлять сигналом, список кэшировать.

### 3. Состояние обновляется после служебных операций

`VpnConnection::setConnectionState()` сначала выполняет `onConnectionStateChanged()`, и только потом сохраняет состояние и уведомляет UI. В обработчике есть ожидания IPC для DNS и очистки маршрутов.

Это преимущественно задерживает **VPN-поток и доставку нового состояния**, а не обязательно отрисовку окна. Пользователь может видеть устаревшее «Подключение…».

Код: [vpnConnection.cpp:614](../../client/vpnConnection.cpp:614), [vpnConnection.cpp:124](../../client/vpnConnection.cpp:124).

**Исправление:** явные асинхронные этапы подключения. При этом нельзя показывать «Защищено», пока маршруты и firewall ещё не применены.

### 4. Завершение приложения может ждать дольше заявленного таймаута

Деструктор `AmneziaApplication` использует `BlockingQueuedConnection` для остановки VPN. Ограниченный `wait(3000)` выполняется **после** этих блокирующих вызовов и не ограничивает время ожидания самих вызовов.

Код: [amneziaApplication.cpp:71](../../client/amneziaApplication.cpp:71).

**Риск:** долгое закрытие приложения, если рабочий поток занят. Нужен асинхронный shutdown с общим deadline.

### 5. Некоторые ошибки backend не доходят до UI

В обработке `backendFailure` у `LocalSocketController` ошибки split tunneling записываются в лог; соответствующие `REPORTERROR` закомментированы.

Дополнительно `KillSwitch::enablePeerTraffic()` возвращает `true`, хотя вызываемый `activateSplitTunnel()` сообщает об ошибках отдельно через сигналы.

Код: [localsocketcontroller.cpp:466](../../client/mozilla/localsocketcontroller.cpp:466), [killswitch.cpp:289](../../service/server/killswitch.cpp:289).

**Это подтверждённый пробел в передаче результата.** Он способен объяснить ситуацию «настройка включена, но поведение не соответствует ожиданиям». Нужны единый результат применения политики и заметная ошибка.

### 6. Переподключение требует более строгой последовательности

`reconnectToVpn()` вызывает `stop()` и сразу `start()`. У WG `LocalSocketController::deactivate()` отправляет команду службе и немедленно выдаёт `disconnected()`, не ожидая подтверждения завершения.

Код: [vpnConnection.cpp:557](../../client/vpnConnection.cpp:557), [localsocketcontroller.cpp:267](../../client/mozilla/localsocketcontroller.cpp:267).

**Подтверждён порядок операций; конкретная гонка пока не воспроизведена.** Исправлять следует через подтверждённое завершение предыдущего соединения, идентификатор попытки, обработку устаревших событий и задержку повторных попыток.

### 7. На Android есть блокирующие lifecycle-вызовы

- `AmneziaActivity.onCreate()` использует `runBlocking { proto.await() }`.
- `AmneziaVpnService.onDestroy()` использует `runBlocking` и ждёт завершения disconnection job.

Это реальные блокировки основных потоков соответствующих Android-компонентов. VPN-служба может находиться в отдельном процессе, поэтому не вся такая задержка замораживает QML, но она влияет на запуск/остановку и создаёт риск ANR.

Код: [AmneziaActivity.kt:189](../../client/android/src/org/amnezia/vpn/AmneziaActivity.kt:189), [AmneziaVpnService.kt:274](../../client/android/src/org/amnezia/vpn/AmneziaVpnService.kt:274).

### Health checks и kill switch

`PingHelper` существует: ICMP с DNS fallback, сбор потерь и задержек. Однако обнаруженный сигнал `connectionLose` прокинут в IPC; его подписку в клиентской логике при поиске не обнаружил. Это не доказательство отсутствия всех внутренних проверок протоколов, но **общий recovery-контур выглядит неполным**.  
Код: [pinghelper.cpp:37](../../client/mozilla/pinghelper.cpp:37), [ipcserver.cpp:27](../../ipc/ipcserver.cpp:27).

Kill switch тоже нельзя считать гарантированно строгим во всех переходах: например, `WindowsDaemon` отключает его при `activationFailure`, а OpenVPN cleanup отключает его при остановке. Нужно отдельно определить поведение при ошибке, ручном отключении и переподключении.  
Код: [windowsdaemon.cpp:42](../../client/platforms/windows/daemon/windowsdaemon.cpp:42), [openVpnProtocol.cpp:42](../../client/core/protocols/openVpnProtocol.cpp:42).

## Что делают другие клиенты

Это сравнение архитектур и доступных механизмов, **не рейтинг фактической стабильности**: одинаковых нагрузочных тестов этих клиентов здесь не проводилось.

| Клиент | Техника и UX | Что полезно для форка |
|---|---|---|
| **WireGuard Windows / Android** | Windows: tunnel service и IP-маршруты из `AllowedIPs`; Android: allow/disallow приложений через `VpnService`. Windows-клиент не является образцом удобного выбора приложений | Небольшой интерфейс, явные параметры туннеля, разделение GUI и службы. [Windows](https://git.zx2c4.com/wireguard-windows/about/docs/netquirk.md), [Android backend](https://git.zx2c4.com/wireguard-android/tree/tunnel/src/main/java/com/wireguard/android/backend/GoBackend.java) |
| **Mullvad** | Rust daemon/Talpid, отдельный GUI, явный автомат состояний; ошибки переводят систему в блокирующую политику. Windows split tunneling — отдельный драйвер | Лучший архитектурный ориентир для состояний, offline/reconnect и безопасности переходов. Сам драйвер уже используется Amnezia. [Архитектура](https://github.com/mullvad/mullvadvpn-app/blob/main/docs/architecture.md) |
| **Proton VPN** | Windows GUI + служба + kernel callout driver. Выбор из списка приложений, ручное добавление EXE, include/exclude для приложений и IP | Готовый список программ и ясное разделение режимов. Текущая документация допускает Windows split tunneling вместе с Standard/Advanced kill switch. [Устройство приложения](https://github.com/ProtonVPN/win-app/blob/master/README.md), [интерфейс настройки](https://protonvpn.com/support/protonvpn-split-tunneling) |
| **Outline** | Простой клиент с отдельным Manager; семейство tun2socks. Запрос desktop split tunneling по процессам остаётся открытым | Брать простоту подключения и ошибок; не считать образцом Windows app-routing. [Репозиторий](https://github.com/OutlineFoundation/outline-apps), [запрос функции](https://github.com/OutlineFoundation/outline-apps/issues/1206) |
| **Hiddify** | Мультиплатформенная оболочка на базе sing-box, TUN, удалённые профили, автоматический выбор узла по задержке | Обычный режим с минимумом настроек, понятный выбор профиля и проверка узлов. Возможности sing-box не обязательно целиком доступны через UI. [README](https://github.com/hiddify/hiddify-app) |
| **v2rayN** | Desktop GUI для Xray/sing-box и других ядер | Разделение профиля сервера и политики маршрутизации. Возможность process-routing зависит от выбранного ядра и режима захвата. [Репозиторий](https://github.com/2dust/v2rayN) |
| **v2rayNG** | Android-клиент; per-app режим с прямым/обратным выбором, автоматическим выбором и импортом/экспортом списка | Переносимые наборы приложений и массовые действия. [Исходные строки интерфейса](https://github.com/2dust/v2rayNG/blob/master/V2rayNG/app/src/main/res/values/strings.xml) |
| **NekoBox / Nekoray** | NekoBox Android использует sing-box; Nekoray — Qt desktop, исходный проект помечен как неподдерживаемый | Изучать настройки маршрутов; не брать заброшенный Nekoray как новую основу. [NekoBox](https://github.com/MatsuriDayo/NekoBoxForAndroid), [Nekoray](https://github.com/MatsuriDayo/nekoray) |
| **Clash Verge Rev / mihomo** | Tauri GUI, отдельное ядро/служба. Mihomo поддерживает `PROCESS-NAME`, `PROCESS-PATH`, wildcard/regex, домены, GEOIP и rule sets | Удобная модель правил и групп, проверка узлов и fallback. [Клиент](https://github.com/clash-verge-rev/clash-verge-rev), [правила](https://wiki.metacubex.one/en/config/rules/), [fallback](https://wiki.metacubex.one/en/config/proxy-groups/fallback/) |
| **sing-box** | Движок: TUN, process name/path/regex на Windows, package name на Android, доменные/IP-правила, URLTest | Полезен для расширенного routing. Для новой интеграции ориентироваться на rule sets вместо старых полей geoip/geosite. [Правила](https://sing-box.sagernet.org/configuration/route/rule/), [URLTest](https://sing-box.sagernet.org/configuration/outbound/urltest/) |
| **Throne** | Qt-клиент на базе sing-box; есть headless/service режим. В changelog — дерево соединений по процессам и добавление адресов/доменов из соединений в правила | Особенно полезен UX «увидел соединение → создал правило». Доказывает, что для такого интерфейса необязательно менять Qt. [README](https://github.com/throneproj/Throne), [changelog](https://github.com/throneproj/Throne/blob/dev/CHANGELOG.md) |

**Важное техническое различие:** `PROCESS-PATH` в TUN-прокси и исключение через WFP-драйвер — разные реализации. У TUN-движка трафик сначала попадает в виртуальный интерфейс, затем классифицируется и отправляется через proxy/direct. Это не автоматически обеспечивает ту же семантику, что исключение процесса из VPN на уровне Windows.

Аналогично:

- **автовыбор узла** не равен восстановлению VPN-службы после сбоя;
- **TUN/strict route** не равны проверенному постоянному kill switch;
- **успешный ping** не гарантирует работоспособность DNS и HTTPS;
- запуск ядра в отдельном процессе сам по себе не гарантирует отзывчивый GUI.

## Список предложений — UX и техника — с оценкой сложности

Оценки — **рабочее время одного разработчика, знакомого с Qt/C++ и Android**, при уже работающей сборке. Это диапазоны, включая целевые проверки, а не обещание устранить любые проблемы.

| Приоритет | Изменение | Сложность | Оценка |
|---|---|---:|---:|
| **P0** | Замер задержки UI, длительности этапов подключения и времени IPC; обезличенные диагностические события | Средняя | 2–4 дня |
| **P0** | Довести ошибки split driver/службы до интерфейса; показывать, применены ли правила | Средняя | 3–6 дней |
| **P1** | Windows: добавить папку с рекурсивным разовым поиском `.exe`, предпросмотром и массовым добавлением | Низкая–средняя | 2–4 дня |
| **P1** | Windows: сохраняемые правила папок, watcher, повторное сканирование и применение изменений | Средняя | 1–3 недели |
| **P1** | Windows: список установленных/запущенных программ с иконками, поддержкой ярлыков и поиском | Средняя | 1–2 недели |
| **P1** | Android: кэш списка, полностью асинхронная загрузка, выбранные сверху, поиск по package name | Низкая–средняя | 3–6 дней |
| **P1** | Устранить конкретные блокирующие операции подготовки подключения и shutdown | Средняя | 1–2 недели |
| **P2** | Автомат подключения: подтверждение stop, отмена, идентификаторы попыток, backoff и реакция на offline | Высокая | 3–6 недель |
| **P2** | Health checks DNS/HTTPS через нужный маршрут, состояние «туннель есть, интернет недоступен» | Средняя–высокая | 1–3 недели |
| **P2** | Пользовательские наборы приложений/папок, импорт и экспорт | Средняя | 4–8 дней |
| **P2** | Определить и проверить строгую политику kill switch при сбоях/переподключениях | Высокая | 3–6 недель |
| **P3** | Windows include-mode: только выбранные приложения через VPN | Высокая | 4–8+ недель |
| **P3** | Единый движок доменных/process/geo-правил для разных протоколов | Высокая | 1–3+ месяца |
| **P3** | Нативные правила папок внутри Windows kernel driver | Очень высокая | 1–3+ месяца, отдельно подпись и выпуск |

Оценки не складываются механически: часть работ пересекается.

**Первый практический выпуск я ограничил бы четырьмя вещами:** добавление папки, нормальный список программ Windows, видимые ошибки применения правил и исправление измеренных задержек. При готовой сборке это реалистичный объём примерно **2–4 недели**. Постоянное наблюдение за папками и новая логика восстановления — следующий этап.

Для Android новый красивый app picker с нуля не нужен: уже имеющийся компонент можно улучшить существенно дешевле.

## Риски и рекомендации по ведению форка

### Сохранять существующие ядра и небольшой набор патчей

Переписывание интерфейса на Electron/Tauri или замена всего транспорта на sing-box резко расширят задачу. Для твоих жалоб это пока не обосновано.

Разделить изменения на независимые серии:

- выбор приложений и папок;
- хранение и применение routing policy;
- передача ошибок;
- асинхронные операции;
- восстановление соединения.

Это снизит конфликты при обновлении upstream. Особо конфликтными будут `VpnConnection`, конфигураторы и общая конфигурация протоколов.

### Проверять три источника конфигураций

Изменения общей логики должны сохранять:

1. self-hosted admin/user;
2. Premium/Free;
3. native-конфигурации.

В частности, native WG может содержать намеренно ограниченные `AllowedIPs`. Нельзя молча заменить их полным туннелем ради удобства split tunneling. В текущем `appendSplitTunnelingConfig()` уже есть отдельная обработка этого случая.

### Windows: драйвер, конфликты и подпись

Для разворачивания папок в список executable можно сохранить существующий подписанный драйвер. Для модифицированного `.sys` потребуется собственный процесс сборки, тестирования и получения допустимой подписи. Подпись GUI/installer не заменяет подпись kernel driver. [Требования Microsoft](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-reqs).

Также код явно проверяет конфликт с установленной службой Mullvad. Изменение только имени GUI эту проблему не устраняет.  
Код: [windowssplittunnel.cpp:729](../../client/platforms/windows/daemon/windowssplittunnel.cpp:729).

### Android: сборка и установка рядом с официальной версией

Это связка Qt/C++, Gradle/Kotlin, Android SDK/NDK и нативных протокольных библиотек. В текущем Gradle используется JDK 17, есть варианты `oss` и `play`. Для личного APK разумнее начинать с `oss`.  
Код: [build.gradle.kts:10](../../client/android/build.gradle.kts:10), [build.gradle.kts:76](../../client/android/build.gradle.kts:76).

Для параллельной установки нужен отдельный application ID и собственный ключ подписи. Здесь нельзя ограничиться одним полем Gradle: в Kotlin встречаются привязанные к пакету строки процессов и actions.

### Лицензии

Корень проекта лицензирован под **GPLv3**; у зависимостей свои лицензии. Личная модификация без распространения не требует публикации. При передаче сборок другим пользователям нужно соблюдать GPL и предоставлять соответствующий исходный код; одного указания на неизменённый upstream недостаточно. [GNU FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#GPLRequireSourcePostedPublic).

Локальные основания: [LICENSE:1](../../LICENSE:1), [THIRD_PARTY_LICENSES.md:3](../../THIRD_PARTY_LICENSES.md:3).

### Что считать успешной проверкой

Для такого форка недостаточно увидеть надпись «Подключено». Нужны сценарии:

- выбранное и невыбранное приложение одновременно;
- лаунчер, дочерний процесс, отдельно запущенный executable и фоновая служба;
- новый `.exe` после обновления игры, в том числе при активном VPN;
- TCP, UDP, DNS и IPv6;
- смена Wi-Fi, потеря сети, сон/пробуждение;
- падение GUI, службы и протокольного backend;
- kill switch с исключениями;
- Android: блокировка экрана, Always-on, смена сети, удаление выбранного приложения.

**Рекомендация для твоего случая:** сохранить Amnezia как основу, сначала улучшить Windows app picker и правила папок, затем исправить передачу ошибок и измеренные задержки. Это значительно меньше и надёжнее по объёму, чем создание нового универсального VPN-клиента.