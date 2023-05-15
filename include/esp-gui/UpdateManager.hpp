//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#ifndef ESP_GUI_UPDATEMANAGER_H
#define ESP_GUI_UPDATEMANAGER_H
#include "ESPAsyncWebServer.h"
#include <esp-gui/WebServer.hpp>
#include <yal/yal.hpp>
namespace esp_gui {
class UpdateManager {
 public:
  UpdateManager(WebServer& webServer) :
      m_logger(yal::Logger("UPDATE")), m_webServer(webServer) {
  }
  void setup();

 private:
  void onPost(AsyncWebServerRequest* request);

  void onUpload(
    AsyncWebServerRequest* request,
    const String& filename,
    size_t index,
    uint8_t* data,
    size_t len,
    bool final);

  yal::Logger m_logger;
  WebServer& m_webServer;

  static inline const String m_uploadConfigName = "updateFirmware";
};

}  // namespace esp_gui

#endif  // ESP_GUI_UPDATEMANAGER_H
