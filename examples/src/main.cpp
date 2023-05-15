//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT License
//

#if ESP_GUI_BUILD_MAIN
#include <Arduino.h>
#include <esp-gui/Configuration.hpp>
#include <esp-gui/UpdateManager.hpp>
#include <esp-gui/WebServer.hpp>
#include <esp-gui/WifiManager.hpp>
#include <yal/appender/ArduinoSerial.hpp>

yal::Logger m_logger;
yal::appender::ArduinoSerial<HardwareSerial> m_serialAppender(&m_logger, &Serial, true);

esp_gui::Configuration m_config;
esp_gui::WebServer m_server(80, "demo", m_config);
// optional: enable configure of wifi
esp_gui::WifiManager m_wifiMgr(m_config, m_server);

// optional: enable firmware upload
esp_gui::UpdateManager m_updateManager(m_server);
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

  // This will overwrite the value from the configuration.
  m_config.setValue(m_demoInt, 42);
  m_config.setValue(m_demoString, "ESP-GUI");

  esp_gui::Container demoContainer("Demo");
  demoContainer.addInput(esp_gui::InputElementType::INT, String("Demo int"), m_demoInt);
  demoContainer.addInput(
    esp_gui::InputElementType::STRING, String("Demo String"), m_demoString);
  demoContainer.addList({"option 1", "hello", "world"}, String("Demo List"), m_demoList);

  demoContainer.addButton(String("Append list item"), m_demoButton, [] {
    const auto list = m_server.findElement<esp_gui::ListElement>(m_demoList);
    if (list == nullptr) {
      return;
    }
    list->addOption("dynamic list item" + String(++m_listIdx));
  });

  m_server.addContainer(std::move(demoContainer));
  m_wifiMgr.setup(false);
  m_updateManager.setup();
  m_server.setup(m_config.value<String>("wifi_hostname"));
}

void loop() {
  m_wifiMgr.loop();
  delay(1000);
  int currentUsage = m_config.value<int>(m_demoInt);
  m_config.setValue(m_demoInt, currentUsage + 1);
  // optional: this persists the value in eeprom.
  // should not be done on a regular basis because eeprom has only 10k write cycles
  // m_config.setValue(m_demoInt, currentUsage + 1, true);
}
#endif
