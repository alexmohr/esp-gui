//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <Arduino.h>
#include <LittleFS.h>
#include <esp-gui/Configuration.hpp>
#include <sstream>

namespace esp_gui {

void Configuration::setup() {
  if (!LittleFS.begin()) {
    m_logger.log(yal::Level::ERROR, "failed to init littlefs");
    return;
  }
  m_logger.log(yal::Level::DEBUG, "Loading config");
  auto file = LittleFS.open(m_configFile, "r");
  if (!file) {
    m_logger.log(yal::Level::ERROR, "Failed to read config from FS");
    return;
  }

  auto readSize = file.read(data.data(), data.size());
  file.close();
  m_logger.log(yal::Level::DEBUG, "Read % bytes from FS, data: ", readSize, data.data());
  LittleFS.end();

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(m_config, data);
  if (error == DeserializationError::Ok) {
    m_logger.log(yal::Level::INFO, "Successfully loaded config");
    logConfig();
  } else {
    m_logger.log(yal::Level::ERROR, "Config is not valid %", error);
  }
}

void Configuration::store() {
  m_logger.log(yal::Level::DEBUG, "Storing config");

  File file = LittleFS.open(m_configFile, "w");
  if (!file) {
    m_logger.log(yal::Level::ERROR, "Failed to open config for writing");
    return;
  }
  std::stringstream cfg;
  ArduinoJson6194_F1::serializeJson(m_config, cfg);

  const auto cfgStr = cfg.str();
  const auto writtenBytes = file.write(cfgStr.c_str(), cfgStr.size());
  file.close();
  if (writtenBytes != cfgStr.size()) {
    m_logger.log(
      yal::Level::ERROR,
      "Failed to write configuration, wrote % of % bytes",
      writtenBytes,
      cfgStr.size());
    return;
  }

  m_logger.log(
    yal::Level::INFO, "Successfully updated config", writtenBytes, cfgStr.size());
}

void Configuration::reset() {
  m_logger.log(yal::Level::WARNING, "Resetting configuration!");
  m_config = DynamicJsonDocument(data.size());
  store();
}

}  // namespace esp_gui
