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
      m_webServer(webServer), m_config(config), m_logger(yal::Logger("WIFI")){};

  /**
   * Setup wifi by loading configuration from config or showing cfg portal
   * @param showConfigPortal set to true to force showing config portal
   */
  void setup(bool showConfigPortal);
  bool checkWifi();

 private:
  struct fastConfig {
    uint8_t *bssid = nullptr;
    int32_t channel = 0;
  };

  [[noreturn]] bool showConfigurationPortal();
  bool loadAPsFromConfig();
  [[nodiscard]] std::vector<String> getApList() const;
  static bool getFastConnectConfig(const String &ssid, fastConfig &config);

  wl_status_t connectMultiWiFi();

  unsigned char m_reconnectCount = 0;

  WebServer &m_webServer;
  Configuration &m_config;
  yal::Logger m_logger;
};

}  // namespace esp_gui

#endif  // WIFIMANAGER_HPP_
