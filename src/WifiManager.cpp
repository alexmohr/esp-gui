//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <DNSServer.h>
#include <esp-gui/WifiManager.hpp>

namespace esp_gui {

void WifiManager::setup(bool showConfigPortal) {
  if (
    showConfigPortal || !loadAPsFromConfig() || connectMultiWiFi(true) != WL_CONNECTED) {
    // Starts access point
    while (!showConfigurationPortal()) {
      m_logger.log(
        yal::Level::WARNING, "Configuration did not yield valid wifi, retrying");
    }
  }

  checkWifi();
}

void WifiManager::loop() {
  if (m_shouldScan) {
    m_shouldScan = false;
    setApList();
    connectMultiWiFi(true);
  }
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

  setApList();

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

void WifiManager::setApList() const {
  m_logger.log(yal::Level::DEBUG, "Searching for available networks");

  const auto ssidElement = m_webServer.findElement<ListElement>(m_cfgWifiSsid);
  if (nullptr == ssidElement) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int8_t networksFound = WiFi.scanNetworks();
  for (int8_t i = 0; i < networksFound; i++) {
    const auto ssid = WiFi.SSID(i);
    m_logger.log(yal::Level::DEBUG, "Found SSID '%'", ssid.c_str());

    ssidElement->addOption(ssid);
  }
}

bool WifiManager::checkWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  m_logger.log(yal::Level::WARNING, "WIFi disconnected, reconnecting...");
  if (connectMultiWiFi(false) == WL_CONNECTED) {
    m_reconnectCount = 0;
    return true;
  }

  m_logger.log(
    yal::Level::WARNING, "WiFi reconnection failed, % times", ++m_reconnectCount);
  return false;
}

wl_status_t WifiManager::connectMultiWiFi(bool useFastConfig) {
  WiFi.forceSleepWake();
  m_logger.log(yal::Level::INFO, "Connecting WiFi...");

  // STA = client mode
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(m_config.value<std::string>("wifi_hostname").c_str());

  const auto ssid = m_config.value<std::string>("wifi_ssid");
  const auto password = m_config.value<std::string>("wifi_password");

  fastConfig connectConfig{};
  const auto hasFastConfig =
    useFastConfig && getFastConnectConfig(ssid.c_str(), connectConfig);
  wl_status_t status;
  uint8_t connectTimeout = 60;
  if (hasFastConfig) {
    connectTimeout = 30;
    m_logger.log(yal::Level::DEBUG, "Using fast connect");
    status = WiFi.begin(
      ssid.c_str(), password.c_str(), connectConfig.channel, connectConfig.bssid, true);
  } else {
    m_logger.log(yal::Level::DEBUG, "Using standard connect");
    status = WiFi.begin(ssid.c_str(), password.c_str());
  }

  int i = 0;
  while ((i++ < connectTimeout) && (status != WL_CONNECTED)) {
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
      "Wifi connected:"
      "SSID: %, "
      "RSSI=%, "
      "Channel: %, "
      "IP address: %, ",
      WiFi.SSID().c_str(),
      static_cast<int>(WiFi.RSSI()),
      WiFi.channel(),
      WiFi.localIP().toString().c_str());
    //@formatter:on
  } else {
    m_logger.log(yal::Level::WARNING, "WiFi connect timeout");
    if (hasFastConfig) {
      m_logger.log(yal::Level::WARNING, "Fast config failed, trying slow path");
      return connectMultiWiFi(false);
    }
  }

  return status;
}

bool WifiManager::getFastConnectConfig(const String& ssid, fastConfig& config) {
  // adopted from
  // https://github.com/roberttidey/WiFiManager/blob/feature_fastconnect/WiFiManager.cpp
  int8_t networksFound = WiFi.scanNetworks();
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

void WifiManager::addWifiContainers() {
  std::vector<std::any> elements;
  elements.emplace_back(esp_gui::ListElement({}, String("SSID"), m_cfgWifiSsid));

  esp_gui::ButtonElement::OnClick onScanClick = [&]() {
    m_shouldScan = true;
  };

  elements.emplace_back(esp_gui::ButtonElement(
    String("Scan Wifi (will disconnect Wifi)"), m_scanWifiButton, std::move(onScanClick)));

  elements.emplace_back(esp_gui::Element(
    esp_gui::ElementType::PASSWORD, String("Password"), m_cfgWifiPassword));
  elements.emplace_back(esp_gui::Element(
    esp_gui::ElementType::STRING, String("Hostname"), m_cfgWifiHostname));

  esp_gui::Container wifiSettings("WIFI Settings", std::move(elements));
  m_webServer.addContainer(std::move(wifiSettings));
}

}  // namespace esp_gui
