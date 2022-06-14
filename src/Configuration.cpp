//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <Arduino.h>
#include <LittleFS.h>
#include <esp-gui/Configuration.hpp>
#include <sstream>

namespace esp_gui {

struct FileSystemHandle {
  FileSystemHandle() {
    if (!LittleFS.begin()) {
      m_logger.log(yal::Level::ERROR, "failed to init littlefs");
      return;
    }
  }
  ~FileSystemHandle() {
    LittleFS.end();
  }
 private:
  yal::Logger m_logger = yal::Logger("CONFIG");
};

void Configuration::setup() {
  {
    FileSystemHandle fsHandle;
    m_logger.log(yal::Level::DEBUG, "Loading config");
    auto file = LittleFS.open(m_configFile, "r");
    if (!file) {
      m_logger.log(yal::Level::ERROR, "Failed to read config from FS");
      return;
    }

    auto readSize = file.read(m_jsonData.data(), m_jsonData.size());
    file.close();
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
  m_logger.log(yal::Level::DEBUG, "Storing config");
  FileSystemHandle fsHandle;
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

  m_logger.log(yal::Level::INFO, "Config RAM usage % of % bytes (%)",
               m_config.memoryUsage(),
               m_jsonData.size(),
               (static_cast<float>( m_config.memoryUsage()) / static_cast<float>(m_jsonData.size())) * 100.0F);

  m_logger.log(
    yal::Level::INFO, "Successfully updated config", writtenBytes, cfgStr.size());
}

void Configuration::reset(bool persist) {
  m_logger.log(yal::Level::WARNING, "Resetting configuration!");
  m_config = DynamicJsonDocument(m_jsonData.size());
  if (persist) {
    store();
  }

}

}  // namespace esp_gui
