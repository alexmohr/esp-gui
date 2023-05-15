//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#ifndef WIFIMANAGER_HPP_
#define WIFIMANAGER_HPP_

#include <chrono>

#include <esp-gui/Configuration.hpp>
#include <esp-gui/WebServer.hpp>
#include <yal/yal.hpp>

namespace esp_gui {
class WifiManager {
 public:
  WifiManager(Configuration &config, WebServer &webServer) :
      m_webServer(webServer), m_config(config), m_logger(yal::Logger("WIFI")) {
    addWifiContainers();
  };

  /**
   * Setup wifi by loading configuration from config or showing cfg portal
   * @param showConfigPortal set to true to force showing config portal
   */
  void setup(bool showConfigPortal);
  bool checkWifi();

  void loop();

 private:
  struct fastConfig {
    uint8_t *bssid = nullptr;
    int32_t channel = 0;
  };

  [[noreturn]] bool showConfigurationPortal();
  bool loadAPsFromConfig();
  void setApList() const;
  static bool getFastConnectConfig(const String &ssid, fastConfig &config);

  wl_status_t connectMultiWiFi(bool useFastConfig);

  void addWifiContainers();

  unsigned char m_reconnectCount = 0;

  WebServer &m_webServer;
  Configuration &m_config;
  yal::Logger m_logger;
  bool m_shouldScan = false;

  static inline const String m_cfgWifiSsid = "wifi_ssid";
  static inline const String m_cfgWifiPassword = "wifi_password";
  static inline const String m_cfgWifiHostname = "wifi_hostname";
  static inline const String m_scanWifiButton = "wifi_button_scan";
};

}  // namespace esp_gui

#endif  // WIFIMANAGER_HPP_
