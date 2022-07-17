//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <esp-gui/UpdateManager.hpp>

namespace esp_gui {

void UpdateManager::setup() {
  Container update("Update");
  update.addUpload(
    "Upload",
    "Update",
    m_uploadConfigName,
    ".bin,.bin.gz",
    [&](
      AsyncWebServerRequest* request,
      const String& filename,
      size_t index,
      uint8_t* data,
      size_t len,
      bool final) { onUpload(request, filename, index, data, len, final); },
    [&](AsyncWebServerRequest* request) { onPost(request); });
  m_webServer.addContainer(std::move(update));
}

void UpdateManager::onUpload(
  AsyncWebServerRequest* request,
  const String& filename,
  size_t index,
  uint8_t* data,
  size_t len,
  bool final) {
  if (!index) {
    m_logger.log(yal::Level::INFO, "Starting update with file: %", filename.c_str());

    Update.runAsync(true);
    if (!Update.begin((EspClass::getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
      Update.printError(Serial);
    }
  }

  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
    }
  }

  if (final) {
    if (Update.end(true)) {
      m_logger.log(yal::Level::INFO, "Update success, filesize: %", index + len);
    } else {
      Update.printError(Serial);
    }
  }
}

void UpdateManager::onPost(AsyncWebServerRequest* request) {
  const bool updateSuccess = !Update.hasError();
  m_webServer.redirectBackToHome(request, 30s);

  m_webServer.reset(
    request, (String("Update ") + (updateSuccess ? "success" : "failed")).c_str());
}

}  // namespace esp_gui
