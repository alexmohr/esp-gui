//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <DNSServer.h>
#include <esp-gui/WifiManager.hpp>

namespace esp_gui {

void WifiManager::setup(bool showConfigPortal) {
  if (showConfigPortal || !loadAPsFromConfig() || connectMultiWiFi() != WL_CONNECTED) {
    // Starts access point
    while (!showConfigurationPortal()) {
      m_logger.log(
        yal::Level::WARNING, "Configuration did not yield valid wifi, retrying");
    }
  }

  checkWifi();
}

bool WifiManager::loadAPsFromConfig() {
  // Don't permit NULL SSID and password len < // MIN_AP_PASSWORD_SIZE (8)
  const auto ssid = m_config.value<std::string>("wifi_ssid");
  if (ssid.empty() || ssid == "null") {
    m_logger.log(yal::Level::DEBUG, "SSID is invalid");
    return false;
  }

  const auto password = m_config.value<std::string>("wifi_password");
  if (password.empty()) {
    m_logger.log(yal::Level::DEBUG, "Password is invalid");
    return false;
  }

  m_logger.log(
    yal::Level::TRACE,
    "Wifi config is valid: SSID: %, PW: %",
    ssid.c_str(),
    password.c_str());

  return true;
}

[[noreturn]] bool WifiManager::showConfigurationPortal() {
  auto accessPoints = getApList();
  m_logger.log(yal::Level::DEBUG, "Starting access point");

  DNSServer dnsServer;
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

  // SSID and PW for Config Portal
  const String ssid = "ESP-Config-AP-" + String(EspClass::getChipId(), HEX);
  const char* password = "ESPConfigAccessPoint";
  WiFi.softAP(ssid, password);

  const auto hostname = "ESP";
  WiFi.setHostname(hostname);

  const auto ip = WiFi.softAPIP();

  const auto dnsPort = 53;
  if (!dnsServer.start(dnsPort, "*", WiFi.softAPIP())) {
    // No socket available
    m_logger.log(yal::Level::ERROR, "Can't start dns server");
  }

  m_logger.log(yal::Level::DEBUG, "Starting Config Portal");
  m_webServer.setup(hostname);
  m_webServer.setApList(std::move(accessPoints));

  m_logger.log(
    yal::Level::INFO,
    "Configuration portal ready at %, ssid %, password %",
    ip.toString().c_str(),
    ssid.c_str(),
    password);

  logMemory(m_logger);

  // WebServer will restart ESP after configuration is done
  while (true) {
    dnsServer.processNextRequest();
    delay(100);
  }
}

std::vector<String> WifiManager::getApList() const {
  m_logger.log(yal::Level::DEBUG, "Searching for available networks");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  const auto apCount = WiFi.scanNetworks();
  std::vector<String> accessPoints;
  for (auto i = 0; i < apCount; ++i) {
    const auto ssid = WiFi.SSID(i);
    m_logger.log(yal::Level::DEBUG, "Found SSID '%'", ssid.c_str());
    accessPoints.push_back(ssid);
  }

  return accessPoints;
}

bool WifiManager::checkWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  m_logger.log(yal::Level::WARNING, "WIFi disconnected, reconnecting...");
  if (connectMultiWiFi() == WL_CONNECTED) {
    m_reconnectCount = 0;
    return true;
  }

  m_logger.log(
    yal::Level::WARNING, "WiFi reconnection failed, % times", ++m_reconnectCount);
  return false;
}

wl_status_t WifiManager::connectMultiWiFi() {
  WiFi.forceSleepWake();
  m_logger.log(yal::Level::INFO, "Connecting WiFi...");

  // STA = client mode
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(m_config.value<std::string>("wifi_hostname").c_str());

  const auto ssid = m_config.value<std::string>("wifi_ssid");
  const auto password = m_config.value<std::string>("wifi_password");

  fastConfig connectConfig{};
  const auto hasFastConfig = getFastConnectConfig(ssid.c_str(), connectConfig);
  wl_status_t status;
  if (hasFastConfig) {
    m_logger.log(yal::Level::DEBUG, "Using fast connect");
    status = WiFi.begin(
      ssid.c_str(), password.c_str(), connectConfig.channel, connectConfig.bssid, true);
  } else {
    m_logger.log(yal::Level::DEBUG, "Using standard connect");
    status = WiFi.begin(ssid.c_str(), password.c_str());
  }

  int i = 0;
  while ((i++ < 60) && (status != WL_CONNECTED)) {
    delay(100);
    status = WiFi.status();
    if (status == WL_CONNECTED) {
      break;
    }
  }

  if (status == WL_CONNECTED) {
    //@formatter:off
    m_logger.log(
      yal::Level::INFO,
      "Wifi connected:\n"
      "\tSSID: %\n"
      "\tRSSI=%\n"
      "\tChannel: %\n"
      "\tIP address: %",
      WiFi.SSID().c_str(),
      static_cast<int>(WiFi.RSSI()),
      WiFi.channel(),
      WiFi.localIP().toString().c_str());
    //@formatter:on
  } else {
    m_logger.log(yal::Level::WARNING, "WiFi connect timeout");
  }

  return status;
}

bool WifiManager::getFastConnectConfig(const String& ssid, fastConfig& config) {
  // adopted from
  // https://github.com/roberttidey/WiFiManager/blob/feature_fastconnect/WiFiManager.cpp
  int networksFound = WiFi.scanNetworks();
  int32_t scan_rssi = -200;
  for (auto i = 0; i < networksFound; i++) {
    if (ssid == WiFi.SSID(i)) {
      if (WiFi.RSSI(i) > scan_rssi) {
        config.bssid = WiFi.BSSID(i);
        config.channel = WiFi.channel(i);
        return true;
      }
    }
  }
  return false;
}

}  // namespace esp_gui
