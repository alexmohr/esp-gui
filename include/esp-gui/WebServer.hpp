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

enum class ElementType {
  STRING,
  PASSWORD,
  INT,
  FLOAT,

};

class ElementBase {
 public:
  ElementBase(ElementType type, String id, String label) :
      m_type(type), m_id(std::move(id)), m_label(std::move(label)) {
  }

  [[nodiscard]] const ElementType& type() const {
    return m_type;
  }

  [[nodiscard]] const String& id() const {
    return m_id;
  }

  [[nodiscard]] const String& label() const {
    return m_label;
  }

 private:
  const ElementType m_type;
  const String m_id;
  const String m_label;
};

template<typename T>
class Element : public ElementBase {
 public:

  using GetValueFunc = std::function<T()>;
  using SetValueFunc = std::function<void(const T&)>;

  [[nodiscard]] bool readOnly() const {
    return m_readOnly;
  }

  [[nodiscard]] const T value() const {
    return m_getValue();
  }

  void setValue(const T& value) {
    m_setValue(value);
  }

 protected:
  Element(
    ElementType type,
    String idVal,
    String label,
    Configuration* config,
    bool isReadOnly = false) :
      Element(
        type,
        std::move(idVal),
        std::move(label),
        std::move([this]() {

          return m_config->value<T>(id());
        }),
        std::move([this](const T& value) { m_config->setValue(id(), value); }),
        config,
        isReadOnly) {
  }

  Element(
    ElementType type,
    String id,
    String label,
    GetValueFunc&& getValueFunc,
    SetValueFunc&& setValueFunc,
    bool isReadOnly = false) :
      Element(
        type,
        std::move(id),
        std::move(label),
        std::move(getValueFunc),
        std::move(setValueFunc),
        "",
        nullptr,
        isReadOnly) {
  }

  Element(
    ElementType type,
    String id,
    String label,
    GetValueFunc&& getValueFunc,
    SetValueFunc&& setValueFunc,
    Configuration* config,
    bool isReadOnly = false) :
      ElementBase(type, std::move(id), std::move(label)),
      m_getValue(std::move(getValueFunc)),
      m_setValue(std::move(setValueFunc)),
      m_readOnly(isReadOnly),
      m_config(config) {
  }

 private:

  yal::Logger m_logger = yal::Logger("ELEMENT");

  GetValueFunc m_getValue;
  SetValueFunc m_setValue;

  const bool m_readOnly;
  Configuration* m_config;
};

class StringElement : public Element<String> {
 public:
  StringElement(String id, String label, Configuration* config, bool isReadOnly = false) :
      Element(ElementType::STRING, std::move(id), std::move(label), config, isReadOnly) {
  }

  StringElement(
    String id,
    String label,
    GetValueFunc&& getValueFunc,
    SetValueFunc&& setValueFunc,
    Configuration* config,
    bool isReadOnly = false) :
      Element(
        ElementType::STRING,
        std::move(id),
        std::move(label),
        std::move(getValueFunc),
        std::move(setValueFunc),
        config,
        isReadOnly) {
  }
};

class PasswordElement : public Element<String> {
 public:
  PasswordElement(
    String id,
    String label,
    Configuration* config,
    bool isReadOnly = false) :
      Element(
        ElementType::PASSWORD,
        std::move(id),
        std::move(label),
        config,
        isReadOnly) {
  }

  PasswordElement(
    String id,
    String label,
    GetValueFunc&& getValueFunc,
    SetValueFunc&& setValueFunc,
    Configuration* config,
    bool isReadOnly = false) :
      Element(
        ElementType::PASSWORD,
        std::move(id),
        std::move(label),
        std::move(getValueFunc),
        std::move(setValueFunc),
        config,
        isReadOnly) {
  }
};

class FloatElement : public Element<float> {
 public:
  FloatElement(String id, String label, Configuration* config, bool isReadOnly = false) :
      Element(ElementType::FLOAT, std::move(id), std::move(label), config, isReadOnly) {
  }

  FloatElement(
    String id,
    String label,
    GetValueFunc&& getValueFunc,
    SetValueFunc&& setValueFunc,
    Configuration* config,
    bool isReadOnly = false) :
      Element(
        ElementType::FLOAT,
        std::move(id),
        std::move(label),
        std::move(getValueFunc),
        std::move(setValueFunc),
        config,
        isReadOnly) {
  }
};

class IntElement : public Element<int> {
 public:
  IntElement(String id, String label, Configuration* config, bool isReadOnly = false) :
      Element(ElementType::INT, std::move(id), std::move(label), config, isReadOnly) {
  }

  IntElement(
    String id,
    String label,
    GetValueFunc&& getValueFunc,
    SetValueFunc&& setValueFunc,
    Configuration* config,
    bool isReadOnly = false) :
      Element(
        ElementType::INT,
        std::move(id),
        std::move(label),
        std::move(getValueFunc),
        std::move(setValueFunc),
        config,
        isReadOnly) {
  }
};

//
// using ListElementGetOptionsCallback = std::function<std::vector<String>()>;
// class ListInput : public Element {
//  ListElement(
//    String label,
//    String configName,
//    ListElementGetOptionsCallback getOptionsCallback,
//    bool isReadOnly = false) :
//      m_getOptionsCallback(getOptionsCallback),
//      Element(
//        ElementType::STRING_LIST,
//        std::move(label),
//        std::move(configName),
//        std::move(isReadOnly)) {
//  }
//
// private:
//};

class Container {
 public:
  Container(String title, std::vector<ElementBase>&& elements) :
      m_title(std::move(title)), m_elements(elements) {
  }

  [[nodiscard]] const String& title() const {
    return m_title;
  }

  [[nodiscard]] const std::vector<ElementBase>& elements() const {
    return m_elements;
  }

 private:
  const String m_title;
  const std::vector<ElementBase> m_elements;
};

class WebServer {
 public:
  WebServer(int port, const char* const hostname, Configuration& config) :
      m_asyncWebServer(AsyncWebServer(port)), m_hostname("default"), m_config(config) {
    addWifiContainers();
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

 private:
  // void addToContainerData(const char* const data);
  void rootHandleGet(AsyncWebServerRequest* request);
  void rootHandlePost(AsyncWebServerRequest* request);
  void updateHandlePost(AsyncWebServerRequest* request);
  void updateHandleUpload(
    AsyncWebServerRequest* request,
    const String& filename,
    size_t index,
    uint8_t* data,
    size_t len,
    bool final);
  void eraseConfig(AsyncWebServerRequest* request);
  [[nodiscard]] String templateCallback(const String& templateString);

  enum class WriteAndCheckResult {
    SUCCESS,
    CHECKSUM_MISSMATCH,
    WRITE_FAILED,
  };

  [[nodiscard]] bool containerSetupDone();
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

  [[nodiscard]] bool isCaptivePortal(AsyncWebServerRequest* pRequest);
  void onNotFound(AsyncWebServerRequest* request);

  void reset(AsyncWebServerRequest* request, AsyncResponseStream* response);
  [[nodiscard]] static bool isIp(const String& str);
  void addWifiContainers();

  AsyncWebServer m_asyncWebServer;

  String m_hostname;
  std::vector<String> m_accessPointList;
  yal::Logger m_logger = yal::Logger("WEB");
  Configuration& m_config;

  std::vector<Container> m_container;

  const String m_cfgWifiSsid = "wifi_ssid";
  const String m_cfgWifiPassword = "wifi_password";
  const String m_cfgWifiHostname = "wifi_hostname";

  const String m_htmlIndex = "/index.html";

  static constexpr const char* PROGMEM CONTENT_TYPE_HTML = "text/html";
  enum HtmlReturnCode {
    HTTP_OK = 200,
    HTTP_FOUND = 302,
    HTTP_DENIED = 403,
    HTTP_NOT_FOUND = 404
  };
};
}  // namespace esp_gui

#endif  // WEBSERVER_HPP_
