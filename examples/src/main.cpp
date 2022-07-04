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
esp_gui::WifiManager m_wifiMgr(m_config, m_server);
String m_demoString = "demo_string";
String m_demoInt = "demo_int";
String m_demoList = "demo_list";
String m_demoButton = "demo_button";
int m_listIdx = 0;

void setup() {
  m_serialAppender.begin(115200);
  m_logger.log(yal::Level::INFO, "Running setup");
  yal::Logger::setLevel(yal::Level::TRACE);
  m_config.setup();

  m_server.setPageTitle("ESP-GUI Demo");

  m_config.setValue(m_demoString, "foobar");
  m_config.setValue(m_demoInt, 42);
  m_config.setValue(m_demoString, "ESP-GUI <3");


  std::vector<std::any> elements;
  elements.emplace_back(
    esp_gui::Element(esp_gui::ElementType::INT, String("Demo int"), m_demoInt));
  elements.emplace_back(
    esp_gui::Element(esp_gui::ElementType::STRING, String("Demo String"), m_demoString));

  elements.emplace_back(
    esp_gui::ListElement({"option 1", "hello", "world"}, String("Demo String"), m_demoList));

  elements.emplace_back(
    esp_gui::ButtonElement(String("Append list item"), m_demoButton, []{
      const auto list = m_server.findElement<esp_gui::ListElement>(m_demoList);
      if (list == nullptr) {
        return;
      }
      list->addOption("dynamic list item" + String(++m_listIdx));
    }));


  esp_gui::Container demoContainer("Demo", std::move(elements));


  m_server.addContainer(std::move(demoContainer));

  m_wifiMgr.setup(false);

  m_server.setup(m_config.value<String>("wifi_hostname"));
}

void loop() {
  m_wifiMgr.loop();
  delay(1000);
  int currentUsage = m_config.value<int>(m_demoInt);
  m_config.setValue(m_demoInt, currentUsage + 1);
}
