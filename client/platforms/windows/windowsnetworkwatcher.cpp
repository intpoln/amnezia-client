/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "windowsnetworkwatcher.h"

#include <QNetworkInformation>
#include <QScopeGuard>
#include <QStringList>

#include "leakdetector.h"
#include "logger.h"
#include "networkwatcherimpl.h"
#include "platforms/windows/windowsutils.h"

#pragma comment(lib, "Wlanapi.lib")
#pragma comment(lib, "windowsapp.lib")

namespace {
Logger logger("WindowsNetworkWatcher");
constexpr int networkCheckDelayMsec = 2000;
}

WindowsNetworkWatcher::WindowsNetworkWatcher(QObject* parent)
    : NetworkWatcherImpl(parent) {
  MZ_COUNT_CTOR(WindowsNetworkWatcher);

  m_networkCheckTimer.setSingleShot(true);
  m_networkCheckTimer.setInterval(networkCheckDelayMsec);
  connect(&m_networkCheckTimer, &QTimer::timeout, this,
          &WindowsNetworkWatcher::checkNetwork);
}

WindowsNetworkWatcher::~WindowsNetworkWatcher() {
  MZ_COUNT_DTOR(WindowsNetworkWatcher);

  if (m_routeHandle) {
    CancelMibChangeNotify2(m_routeHandle);
  }

  if (m_wlanHandle) {
    WlanCloseHandle(m_wlanHandle, nullptr);
  }
}

LRESULT WindowsNetworkWatcher::PowerWndProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  auto obj = reinterpret_cast<WindowsNetworkWatcher*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (!obj){
    logger.debug() << "obj not casted";
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  switch (uMsg) {
  case WM_POWERBROADCAST:
    if (wParam == PBT_APMRESUMESUSPEND) {
        emit obj->wakeup();
    }
    break;
  default:
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}

void WindowsNetworkWatcher::initialize() {
  logger.debug() << "initialize";

  createPowerMonitorWindow();

  m_defaultRoutes = defaultRoutesSignature();
  if (NotifyRouteChange2(AF_INET, routeChangeCallback, this, FALSE,
                         &m_routeHandle) != NO_ERROR) {
    WindowsUtils::windowsLog("Failed to register a route change callback");
  }

  DWORD negotiatedVersion;
  if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &m_wlanHandle) !=
      ERROR_SUCCESS) {
    WindowsUtils::windowsLog("Failed to open the WLAN handle");
    return;
  }

  DWORD prefNotifSource;
  if (WlanRegisterNotification(m_wlanHandle, WLAN_NOTIFICATION_SOURCE_MSM,
                               true /* ignore duplicates */,
                               (WLAN_NOTIFICATION_CALLBACK)wlanCallback, this,
                               nullptr, &prefNotifSource) != ERROR_SUCCESS) {
    WindowsUtils::windowsLog("Failed to register a wlan callback");
    return;
  }

  logger.debug() << "callback registered";
}

void WindowsNetworkWatcher::createPowerMonitorWindow() {
  const wchar_t* className = L"PowerMonitorClass";
  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = &WindowsNetworkWatcher::PowerWndProcCallback;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = className;
  wc.cbWndExtra = sizeof(WindowsNetworkWatcher*);

  if (!RegisterClass(&wc)) {
    logger.debug() << "Failed to register window class in createPowerMonitorWindow.";
    return;
  }

  HWND hwnd = CreateWindowEx(0, className, L"Power Monitor", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), static_cast<LPVOID>(this));
  if (!hwnd) {
    logger.debug() << "Failed to create window in createPowerMonitorWindow.";
    return;
  }
  SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

// static
void WindowsNetworkWatcher::routeChangeCallback(PVOID context,
                                                PMIB_IPFORWARD_ROW2 row,
                                                MIB_NOTIFICATION_TYPE type) {
  Q_UNUSED(type);
  if (!row || row->DestinationPrefix.PrefixLength != 0) {
    return;
  }

  static_cast<WindowsNetworkWatcher*>(context)->scheduleNetworkCheck(false);
}

// static
QString WindowsNetworkWatcher::defaultRoutesSignature() {
  PMIB_IPFORWARD_TABLE2 table = nullptr;
  if (GetIpForwardTable2(AF_INET, &table) != NO_ERROR) {
    return QString();
  }
  auto guard = qScopeGuard([&] { FreeMibTable(table); });

  QStringList routes;
  for (ULONG i = 0; i < table->NumEntries; ++i) {
    const MIB_IPFORWARD_ROW2& route = table->Table[i];
    if (route.DestinationPrefix.PrefixLength != 0) {
      continue;
    }

    MIB_IF_ROW2 adapter = {};
    adapter.InterfaceLuid = route.InterfaceLuid;
    if (GetIfEntry2(&adapter) != NO_ERROR ||
        !adapter.InterfaceAndOperStatusFlags.HardwareInterface ||
        adapter.OperStatus != IfOperStatusUp) {
      continue;
    }

    char nextHop[INET_ADDRSTRLEN] = {};
    InetNtopA(AF_INET, &route.NextHop.Ipv4.sin_addr, nextHop,
              sizeof(nextHop));
    routes.append(QString("%1 %2").arg(route.InterfaceLuid.Value).arg(nextHop));
  }
  routes.sort();
  return routes.join(',');
}

void WindowsNetworkWatcher::scheduleNetworkCheck(bool bssidChanged) {
  QMetaObject::invokeMethod(
      this,
      [this, bssidChanged]() {
        m_bssidChanged = m_bssidChanged || bssidChanged;
        m_networkCheckTimer.start();
      },
      Qt::QueuedConnection);
}

void WindowsNetworkWatcher::checkNetwork() {
  const QString defaultRoutes = defaultRoutesSignature();
  const bool routesChanged =
      !defaultRoutes.isEmpty() && defaultRoutes != m_defaultRoutes;
  if (!defaultRoutes.isEmpty()) {
    m_defaultRoutes = defaultRoutes;
  }

  const bool bssidChanged = m_bssidChanged;
  m_bssidChanged = false;
  if (!isActive() || (!routesChanged && !bssidChanged)) {
    return;
  }

  logger.debug() << "The network has changed, routes changed:" << routesChanged
                 << "wifi changed:" << bssidChanged;
  emit networkChanged(QString());
}

// static
void WindowsNetworkWatcher::wlanCallback(PWLAN_NOTIFICATION_DATA data,
                                         PVOID context) {
  logger.debug() << "Callback";

  WindowsNetworkWatcher* that = static_cast<WindowsNetworkWatcher*>(context);
  Q_ASSERT(that);

  that->processWlan(data);
}

void WindowsNetworkWatcher::processWlan(PWLAN_NOTIFICATION_DATA data) {
  logger.debug() << "Processing wlan data";

  if (!isActive()) {
    logger.debug() << "The watcher is off";
    return;
  }

  if (data->NotificationSource != WLAN_NOTIFICATION_SOURCE_MSM) {
    logger.debug() << "The wlan source is not MSM";
    return;
  }

  if (data->NotificationCode != wlan_notification_msm_connected) {
    logger.debug() << "Wlan unprocessed code: " << data->NotificationCode;
    return;
  }

  PWLAN_CONNECTION_ATTRIBUTES connectionInfo = nullptr;
  DWORD connectionInfoSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
  WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_invalid;

  DWORD result = WlanQueryInterface(
      m_wlanHandle, &data->InterfaceGuid, wlan_intf_opcode_current_connection,
      nullptr, &connectionInfoSize, (PVOID*)&connectionInfo, &opCode);
  if (result != ERROR_SUCCESS) {
    WindowsUtils::windowsLog("Failed to query the interface");
    return;
  }

  auto guard = qScopeGuard([&] { WlanFreeMemory(connectionInfo); });

  QString bssid;
  for (size_t i = 0;
       i < sizeof(connectionInfo->wlanAssociationAttributes.dot11Bssid); ++i) {
    if (i == 5) {
      bssid.append(QString::asprintf(
          "%.2X\n", connectionInfo->wlanAssociationAttributes.dot11Bssid[i]));
    } else {
      bssid.append(QString::asprintf(
          "%.2X-", connectionInfo->wlanAssociationAttributes.dot11Bssid[i]));
    }
  }
  if (bssid != m_lastBSSID) {
    scheduleNetworkCheck(true);
    m_lastBSSID = bssid;
  }

  if (connectionInfo->wlanSecurityAttributes.dot11AuthAlgorithm !=
          DOT11_AUTH_ALGO_80211_OPEN &&
      connectionInfo->wlanSecurityAttributes.dot11CipherAlgorithm !=
          DOT11_CIPHER_ALGO_WEP &&
      connectionInfo->wlanSecurityAttributes.dot11CipherAlgorithm !=
          DOT11_CIPHER_ALGO_WEP40 &&
      connectionInfo->wlanSecurityAttributes.dot11CipherAlgorithm !=
          DOT11_CIPHER_ALGO_WEP104) {
    logger.debug() << "The network is secure enough";
    return;
  }

  QString ssid;
  for (size_t i = 0;
       i < connectionInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength;
       ++i) {
    ssid.append(QString::asprintf(
        "%c",
        (char)connectionInfo->wlanAssociationAttributes.dot11Ssid.ucSSID[i]));
  }

  logger.debug() << "Unsecure network:" << logger.sensitive(ssid)
                 << "id:" << logger.sensitive(bssid);
  emit unsecuredNetwork(ssid, bssid);
}
