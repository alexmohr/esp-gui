//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#ifndef WEBSERVER_HPP_
#define WEBSERVER_HPP_

#include <Arduino.h>

#include "Configuration.hpp"
#include <ESPAsyncWebServer.h>
#include <esp-gui/Util.hpp>
#include <yal/yal.hpp>
#include <any>

namespace esp_gui {

enum class ElementType { STRING, STRING_PASSWORD, INT, DOUBLE };

class Element {
 public:
  Element(ElementType type, String label, String configName, bool isReadOnly = false) :
      m_type(type), m_label(std::move(label)), m_configName(std::move(configName)), m_readOnly(isReadOnly) {
  }

  [[nodiscard]] const String& label() const {
    return m_label;
  }

  [[nodiscard]] const String& configName() const {
    return m_configName;
  }

  [[nodiscard]] const ElementType& type() const {
    return m_type;
  }

  [[nodiscard]] bool readOnly() const {
    return m_readOnly;
  }

 private:
  const ElementType m_type;
  const String m_label;
  const String m_configName;
  const bool m_readOnly;
};

class Container {
 public:
  Container(String title, std::vector<Element>&& elements) :
      m_title(std::move(title)), m_elements(elements) {
  }

  [[nodiscard]] const String& title() const {
    return m_title;
  }

  [[nodiscard]] const std::vector<Element>& elements() const {
    return m_elements;
  }

 private:
  const String m_title;
  const std::vector<Element> m_elements;
};

class WebServer {
 public:
  WebServer(int port, const char* const hostname, Configuration& config) :
      m_asyncWebServer(AsyncWebServer(port)), m_hostname("default"), m_config(config) {
  }

  WebServer(const WebServer&) = delete;

  void setup(const String& hostname);

  void setApList(std::vector<String>&& apList) {
    m_accessPointList = std::move(apList);
  }

  void setPageTitle(const String& title) {
    m_config.setValue("page_title", title);
  }

  void addContainer(Container&& container);
  [[nodiscard]] bool containerSetupDone();

 private:
  AsyncWebServer m_asyncWebServer;

  String m_hostname;
  std::vector<String> m_accessPointList;
  yal::Logger m_logger = yal::Logger("WEB");
  Configuration& m_config;

  std::vector<Container> m_container;
  size_t m_containerDataUsed = 0U;

  const String m_htmlIndex = "/index.html";

  static constexpr const char* PROGMEM CONTENT_TYPE_HTML = "text/html";
  enum HtmlReturnCode {
    HTTP_OK = 200,
    HTTP_FOUND = 302,
    HTTP_DENIED = 403,
    HTTP_NOT_FOUND = 404
  };

  // void addToContainerData(const char* const data);
  void rootHandleGet(AsyncWebServerRequest* request);
  void rootHandlePost(AsyncWebServerRequest* request);
  void updateHandlePost(AsyncWebServerRequest* request);
  void updateHandleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
  void eraseConfig(AsyncWebServerRequest* request);
  [[nodiscard]] String templateCallback(const String& templateString);

  enum class WriteAndCheckResult {
    SUCCESS,
    CHECKSUM_MISSMATCH,
    WRITE_FAILED,
  };

  [[nodiscard]] WriteAndCheckResult checkAndWriteHTML(bool writeFS);
  [[nodiscard]] bool fileSystemAndDataChunksEqual(
    unsigned int offset,
    const uint8_t* data,
    int size) const;
  [[nodiscard]] bool fileSystemWriteChunk(
    unsigned int offset,
    const uint8_t* data,
    unsigned int size,
    bool clearFile) const;

  size_t chunkedResponseCopy(
    size_t index,
    size_t maxLen,
    uint8_t* dst,
    const char* const source,
    size_t sourceLength);

  [[nodiscard]] bool isCaptivePortal(AsyncWebServerRequest* pRequest);
  void onNotFound(AsyncWebServerRequest* request);

  void reset(AsyncWebServerRequest* request, AsyncResponseStream* response);
  [[nodiscard]] static bool isIp(const String& str);
};
}  // namespace esp_gui

#endif  // WEBSERVER_HPP_
