//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#ifndef ESP_GUI_CONFIGURATION_HPP
#define ESP_GUI_CONFIGURATION_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <yal/yal.hpp>
#include <any>
#include <map>

namespace esp_gui {
class Configuration {
 public:
  Configuration() : m_config(DynamicJsonDocument(m_jsonData.size())) {
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
    //m_logger.log(yal::Level::DEBUG, "Retrieving configuration key %", key.c_str());
    return m_config[key];
  }

  template<typename T>
  void setValue(const String& key, T value, bool persist = false) {
    logKV(key, value);

    if (m_config[key] == value) {
      m_logger.log(yal::Level::DEBUG, "skipping set, value already in config");
      return;
    }
    m_config[key] = value;
    if (persist) {
      store();
    }
  }

  void setup();
  void store();
  void reset(bool persist);

  struct FileSystemHandle {
    FileSystemHandle() = default;
    bool begin() {
      if (!LittleFS.begin()) {
        m_logger.log(yal::Level::ERROR, "failed to init littlefs");
        return false;
      }
      return true;
    }

    ~FileSystemHandle() {
      LittleFS.end();
    }

   private:
    yal::Logger m_logger = yal::Logger("CONFIG");
  };

  struct FileHandle {
    FileHandle(const char* const fileName, bool openFS = true) : m_fileName(fileName) {
      if (openFS) {
        m_fs = std::make_unique<FileSystemHandle>();
      }
    }

    ~FileHandle() {
      close();
    }

    bool open(const char* const mode) {
      if (m_fs) {
        if (!m_fs->begin()) {
          return false;
        }
      }
      m_file = LittleFS.open(m_fileName, mode);
      return m_file;
    }

    void close() {
      if (m_file) {
        m_file.close();
      }
    }

    File& file() {
      return m_file;
    }

   private:
    const char* const m_fileName;
    File m_file;
    std::unique_ptr<FileSystemHandle> m_fs;
  };

 private:
  template<typename T>
  void logKV(const String& key, T val) {
    m_logger.log(yal::Level::DEBUG, "Set '%' to '%'", key.c_str(), val);
  }

  void logKV(const String& key, String val) {
    m_logger.log(yal::Level::DEBUG, "Set '%' to '%'", key.c_str(), val.c_str());
  }

  yal::Logger m_logger = yal::Logger("CONFIG");
  DynamicJsonDocument m_config;
  static constexpr const char* m_configFile = "/esp-gui-config.dat";

  std::array<uint8_t, 2048> m_jsonData;  // todo move outside
};
}  // namespace esp_gui
#endif  // ESP_GUI_CONFIGURATION_HPP
