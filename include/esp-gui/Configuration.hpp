//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#ifndef ESP_GUI_CONFIGURATION_HPP
#define ESP_GUI_CONFIGURATION_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <yal/yal.hpp>
#include <any>
#include <map>

namespace esp_gui {
class Configuration {
 public:
  Configuration() : m_config(DynamicJsonDocument(data.size())) {
  }
  Configuration(Configuration&) = delete;
  Configuration(Configuration&&) = delete;

  void logConfig() {
    std::stringstream cfg;
    ArduinoJson6194_F1::serializeJson(m_config, cfg);
    const auto cfgStr = cfg.str();
    m_logger.log(yal::Level::DEBUG, "complete config %", cfgStr);
  }

  template<typename T>
  T value(const String& key) {
    m_logger.log(yal::Level::DEBUG, "Retriving configuration key %", key.c_str());
    logConfig();

    return m_config[key.c_str()];
    ;
  }

  template<typename T>
  void setValue(const String& key, T value) {
    m_logger.log(yal::Level::DEBUG, "Set %", key.c_str());
    logConfig();

    if (m_config[key] == value) {
      m_logger.log(yal::Level::DEBUG, "skipping set, value already in config");
      return;
    }
    m_config[key] = value;
    store();
  }

  void setup();
  void store();
  void reset();

 private:
  yal::Logger m_logger = yal::Logger("CONFIG");
  DynamicJsonDocument m_config;
  static constexpr const char* m_configFile = "/esp-gui-config.dat";

  std::array<uint8_t, 128> data;  // todo move outside
};
}  // namespace esp_gui
#endif  // ESP_GUI_CONFIGURATION_HPP
