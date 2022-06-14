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
String m_powerFoobar = "power_foobar";
String m_powerUsage = "power_usage";
String m_myConfig = "power_int";

void setup() {
  m_serialAppender.begin(115200);
  m_logger.log(yal::Level::INFO, "Running setup");
  m_config.setup();

  m_server.setPageTitle("ESP-GUI Demo");

  m_config.setValue(m_powerFoobar, "foobar");
  m_config.setValue(m_powerUsage, 42);

  std::vector<esp_gui::Element> elements;
  elements.emplace_back(esp_gui::Element(
    esp_gui::ElementType::INT, String("Power Usage"), m_powerUsage));
  elements.emplace_back(
    esp_gui::Element(esp_gui::ElementType::STRING, String("Foobar"), m_powerFoobar));
  elements.emplace_back(
    esp_gui::Element(esp_gui::ElementType::INT, String("Config Int"), m_myConfig));
  esp_gui::Container powerUsage("Power usage", std::move(elements));

  m_server.addContainer(std::move(powerUsage));
  m_server.containerSetupDone();

  esp_gui::WifiManager wifiMgr(m_config, m_server);
  wifiMgr.setup(false);

  m_server.setup(m_config.value<String>("wifi_hostname"));
}

void loop() {
  delay(1000);
  int currentUsage = m_config.value<int>(m_powerUsage) ;
  m_config.setValue(m_powerUsage, currentUsage+1);
}
