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
  {
    m_logger.log(yal::Level::DEBUG, "Loading config");
    FileHandle file(m_configFile);
    if (!file.open("r")) {
      m_logger.log(yal::Level::ERROR, "Failed to read config from FS");
    }

    auto readSize = file.file().read(m_jsonData.data(), m_jsonData.size());
    m_logger.log(
      yal::Level::DEBUG, "Read % bytes from FS, data: ", readSize, m_jsonData.data());
  }

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(m_config, m_jsonData);
  if (error == DeserializationError::Ok) {
    m_logger.log(yal::Level::INFO, "Successfully loaded config");
    logConfig();
  } else {
    m_logger.log(yal::Level::ERROR, "Config is not valid %", error);
  }
}

void Configuration::store() {
  FileHandle file(m_configFile);
  if (!file.open("w")) {
    m_logger.log(yal::Level::ERROR, "Failed to open config for writing");
    return;
  }
  std::stringstream cfg;
  ArduinoJson6194_F1::serializeJson(m_config, cfg);

  const auto cfgStr = cfg.str();
  const auto writtenBytes = file.file().write(cfgStr.c_str(), cfgStr.size());
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
    yal::Level::INFO,
    "Config RAM usage % of % bytes (%)",
    m_config.memoryUsage(),
    m_jsonData.size(),
    (static_cast<float>(m_config.memoryUsage()) / static_cast<float>(m_jsonData.size())) *
      100.0F);

  m_logger.log(
    yal::Level::INFO, "Successfully updated config", writtenBytes, cfgStr.size());
  logConfig();
}

void Configuration::reset(bool persist) {
  m_logger.log(yal::Level::WARNING, "Resetting configuration!");
  m_config = DynamicJsonDocument(m_jsonData.size());
  if (persist) {
    store();
  }
}

}  // namespace esp_gui
