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
#include <chrono>

namespace esp_gui {

using std::chrono_literals::operator""s;

enum class ElementType { STRING, PASSWORD, INT, DOUBLE, LIST, BUTTON, UPLOAD };

class Element {
 public:
  Element(ElementType type, String label, String configName, bool isReadOnly = false) :
      m_type(type),
      m_label(std::move(label)),
      m_configName(std::move(configName)),
      m_readOnly(isReadOnly) {
  }

  virtual ~Element() = default;

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

class ListElement : public Element {
 public:
  ListElement(
    std::vector<String> options,
    String label,
    String configName,
    bool isReadOnly = false) :
      Element(ElementType::LIST, std::move(label), std::move(configName), isReadOnly),
      m_options(std::move(options)) {
  }

  ~ListElement() override  = default;

  void addOption(const String& option) {
    m_options.push_back(option);
  }

  void clearOptions() {
    m_options.clear();
  }

  void setOptions(std::vector<String>&& options) {
    m_options = options;
  }

  [[nodiscard]] const std::vector<String>& options() const {
    return m_options;
  }

  [[nodiscard]] int selectedIdx() const {
    return m_selectedIdx;
  }

  void setSelectedIdx(int val) {
    m_selectedIdx = val;
  }

 private:
  std::vector<String> m_options;
  int m_selectedIdx;
};

class ButtonElement : public Element {
 public:
  using OnClick = std::function<void()>;

  ButtonElement(String label, String configName, OnClick&& onClick, std::chrono::seconds delayBeforeRedirect = 0s) :
      Element(ElementType::BUTTON, std::move(label), std::move(configName), true),
      m_onClick(std::move(onClick)), m_delay(delayBeforeRedirect) {
  }

  ~ButtonElement() override = default;

  void click() {
    if (m_onClick) m_onClick();
  }

  [[nodiscard]] std::chrono::seconds delay() {
    return m_delay;
  }

 private:
  OnClick m_onClick;
  std::chrono::seconds m_delay;
};

class UploadElement : public Element {
 public:
  using OnClick = std::function<void()>;

  UploadElement(String label, String configName, OnClick&& onClick) :
      Element(ElementType::BUTTON, std::move(label), std::move(configName), true),
      m_onClick(std::move(onClick)) {
  }

  virtual ~UploadElement() = default;

  void click() {
    if (m_onClick) m_onClick();
  }

 private:
  OnClick m_onClick;
};

class Container {
 public:
  Container(String title, std::vector<std::any>&& elements) :
      m_title(std::move(title)), m_elements(elements) {
  }

  [[nodiscard]] const String& title() const {
    return m_title;
  }

  [[nodiscard]] std::vector<std::any>& elements() {
    return m_elements;
  }

  // todo add safe method to add elements so unsupported types can't be passed

 private:
  const String m_title;
  std::vector<std::any> m_elements;
};

class WebServer {
 public:
  WebServer(int port, const char* const hostname, Configuration& config) :
      m_asyncWebServer(AsyncWebServer(port)), m_hostname("default"), m_config(config) {
  }

  WebServer(const WebServer&) = delete;

  void setup(const String& hostname);

  void setPageTitle(const String& title) {
    m_config.setValue("page_title", title);
  }

  void addContainer(Container&& container);

  template<typename T>
  T* findElement(const String& key) {
    const auto iter = m_elementMap.find(key);
    if (iter == m_elementMap.end()) {
      return nullptr;
    }
    return std::any_cast<T>(iter->second);
  }

 private:
  AsyncWebServer m_asyncWebServer;

  String m_hostname;
  yal::Logger m_logger = yal::Logger("WEB");
  Configuration& m_config;

  std::vector<Container> m_container;
  std::map<String, std::any*> m_elementMap;

  static inline const String m_listSuffix = "___list";

  const String m_htmlIndex = "/index.html";
  static inline const char* const s_redirectDelayedURL = "/delay";

  std::chrono::seconds m_redirectDelay = 15s;

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
  void updateHandleUpload(
    AsyncWebServerRequest* request,
    const String& filename,
    size_t index,
    uint8_t* data,
    size_t len,
    bool final);
  void eraseConfig(AsyncWebServerRequest* request);
  void onClick(AsyncWebServerRequest* request);
  [[nodiscard]] String templateCallback(const String& templateString);
  [[nodiscard]] String listTemplate(
    const String& templ,
    const ListElement* listValue,
    bool isListValue);

  enum class WriteAndCheckResult {
    SUCCESS,
    CHECKSUM_MISSMATCH,
    WRITE_FAILED,
  };

  [[nodiscard]] bool containerSetupDone();
  [[nodiscard]] WriteAndCheckResult checkAndWriteHTML(bool writeFS);
  static void makeInput(
    const Element* element,
    const String& elementValue,
    const String& inputType,
    std::stringstream& ss);

  static void makeSelect(
    const Element* element,
    const String& elementValue,
    const String& inputType,
    std::stringstream& ss);

  static void makeButton(const Element* element, std::stringstream& ss);

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

  [[nodiscard]] Element* anyToElement(std::any& any);
};
}  // namespace esp_gui

#endif  // WEBSERVER_HPP_
