//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT License
//

#include <Arduino.h>
#include <esp-gui/Configuration.hpp>
#include <esp-gui/WebServer.hpp>
#include <esp-gui/WifiManager.hpp>
#include <yal/appender/ArduinoSerial.hpp>

yal::Logger m_logger;
yal::appender::ArduinoSerial<HardwareSerial> m_serialAppender(&m_logger, &Serial, true);

esp_gui::Configuration m_config;
esp_gui::WebServer m_server(80, "demo", m_config);
std::any m_powerValue = 42;
std::any m_name = String("foobar");

void setup() {
  m_serialAppender.begin(115200);
  m_logger.log(yal::Level::INFO, "Running setup");
  m_config.setup();

  m_server.setPageTitle("ESP-GUI Demo");

  std::vector<esp_gui::Element<std::any>> elements;
  elements.emplace_back(esp_gui::Element<std::any>(
    esp_gui::ElementType::INT, String("Power Usage"), m_powerValue));
  elements.emplace_back(
    esp_gui::Element<std::any>(esp_gui::ElementType::STRING, String("Foobar"), m_name));
  esp_gui::Container powerUsage("Power usage", std::move(elements));

  m_server.addContainer(std::move(powerUsage));
  m_server.containerSetupDone();

  esp_gui::WifiManager wifiMgr(m_config, m_server);
  wifiMgr.setup(false);

  m_server.setup(m_config.value<String>("wifi_hostname"));
}

void loop() {
  delay(1000);
  m_logger.log(yal::Level::INFO, "Updating power value");
  auto* val = std::any_cast<int>(&m_powerValue);
  (*val)++;
}
