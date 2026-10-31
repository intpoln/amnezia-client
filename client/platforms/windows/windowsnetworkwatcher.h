/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WINDOWSNETWORKWATCHER_H
#define WINDOWSNETWORKWATCHER_H

#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <wlanapi.h>

#include <QTimer>

#include "networkwatcherimpl.h"

class WindowsNetworkWatcher final : public NetworkWatcherImpl {
 public:
  WindowsNetworkWatcher(QObject* parent);
  ~WindowsNetworkWatcher();

  void initialize() override;

 private:
  static void wlanCallback(PWLAN_NOTIFICATION_DATA data, PVOID context);
  static LRESULT PowerWndProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static void routeChangeCallback(PVOID context, PMIB_IPFORWARD_ROW2 row,
                                  MIB_NOTIFICATION_TYPE type);
  static void addressChangeCallback(PVOID context,
                                    PMIB_UNICASTIPADDRESS_ROW row,
                                    MIB_NOTIFICATION_TYPE type);
  static QString physicalNetworkSignature();

  void createPowerMonitorWindow();
  void scheduleNetworkCheck(bool bssidChanged);
  void checkNetwork();
  void processWlan(PWLAN_NOTIFICATION_DATA data);

 private:
  // The handle is set during the initialization. Windows calls processWlan()
  // to inform about network changes.
  HANDLE m_wlanHandle = nullptr;
  QString m_lastBSSID;

  HANDLE m_routeHandle = nullptr;
  HANDLE m_addressHandle = nullptr;
  QTimer m_networkCheckTimer;
  QString m_physicalNetwork;
  bool m_bssidChanged = false;
};

#endif  // WINDOWSNETWORKWATCHER_H
