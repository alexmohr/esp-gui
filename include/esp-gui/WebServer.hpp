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
#include <utility>

namespace esp_gui {

using std::chrono_literals::operator""s;

enum class InputElementType { STRING, PASSWORD, INT, DOUBLE };
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

class InputElement : public Element {
 public:
  InputElement(
    InputElementType type,
    String label,
    String configName,
    bool isReadOnly = false) :
      Element(convert(type), std::move(label), std::move(configName), isReadOnly) {
  }

 private:
  static ElementType convert(const InputElementType& t) {
    switch (t) {
      case InputElementType::PASSWORD:
        return ElementType::PASSWORD;
      case InputElementType::INT:
        return ElementType::INT;
      case InputElementType::DOUBLE:
        return ElementType::DOUBLE;
      case InputElementType::STRING:
      default:
        return ElementType::STRING;
    }
  }
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

  ~ListElement() override = default;

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

 private:
  std::vector<String> m_options;
};

class ButtonElement : public Element {
 public:
  using OnClick = std::function<void()>;

  ButtonElement(
    String label,
    String configName,
    OnClick&& onClick,
    std::chrono::seconds delayBeforeRedirect = 0s) :
      Element(ElementType::BUTTON, std::move(label), std::move(configName), true),
      m_onClick(std::move(onClick)),
      m_delay(delayBeforeRedirect) {
  }

  ~ButtonElement() override = default;

  void click() const {
    if (m_onClick) m_onClick();
  }

  [[nodiscard]] std::chrono::seconds delay() const {
    return m_delay;
  }

 private:
  OnClick m_onClick;
  std::chrono::seconds m_delay;
};

class UploadElement : public Element {
 public:
  // using OnClick = std::function<void()>;
  using OnUpload = ArUploadHandlerFunction;
  using OnPost = std::function<void(AsyncWebServerRequest* request)>;

  UploadElement(
    String browseLabel,
    String buttonLabel,
    String configName,
    String acceptedFiles,
    OnUpload&& onUpload,
    OnPost&& onPost) :
      Element(ElementType::UPLOAD, std::move(buttonLabel), std::move(configName), true),
      m_browseLabel(std::move(browseLabel)),
      m_acceptedFiles(std::move(acceptedFiles)),
      m_onUpload(std::move(onUpload)),
      m_onPost(std::move(onPost)) {
  }

  ~UploadElement() override = default;

  void onUpload(
    AsyncWebServerRequest* request,
    const String& filename,
    size_t index,
    uint8_t* data,
    size_t len,
    bool final) const {
    m_onUpload(request, filename, index, data, len, final);
  }

  void onPost(AsyncWebServerRequest* request) const {
    m_onPost(request);
  }

  [[nodiscard]] const String& acceptedFiles() const {
    return m_acceptedFiles;
  }

  [[nodiscard]] const String& browseLabel() const {
    return m_browseLabel;
  }

 private:
  String m_browseLabel;
  String m_acceptedFiles;
  OnUpload m_onUpload;
  OnPost m_onPost;
};

class Container {
 public:
  explicit Container(String title) : m_title(std::move(title)), m_elements({}) {
  }

  [[nodiscard]] const String& title() const {
    return m_title;
  }

  [[nodiscard]] std::vector<std::any>& elements() {
    return m_elements;
  }

  void addList(
    std::vector<String> options,
    String label,
    String configName,
    bool isReadOnly = false) {
    m_elements.emplace_back(ListElement(
      std::move(options), std::move(label), std::move(configName), isReadOnly));
  }

  void addButton(
    String label,
    String configName,
    ButtonElement::OnClick&& onClick,
    std::chrono::seconds delayBeforeRedirect = 0s) {
    m_elements.emplace_back(ButtonElement(
      std::move(label), std::move(configName), std::move(onClick), delayBeforeRedirect));
  }

  void addInput(
    InputElementType type,
    String label,
    String configName,
    bool isReadOnly = false) {
    m_elements.emplace_back(
      InputElement(type, std::move(label), std::move(configName), isReadOnly));
  }

  void addUpload(
    String browseLabel,
    String buttonLabel,
    String configName,
    String acceptedFiles,
    UploadElement::OnUpload&& onUpload,
    UploadElement::OnPost&& onPost) {
    m_elements.emplace_back(UploadElement(
      std::move(browseLabel),
      std::move(buttonLabel),
      std::move(configName),
      std::move(acceptedFiles),
      std::move(onUpload),
      std::move(onPost)));
  }

 private:
  const String m_title;
  std::vector<std::any> m_elements;
};

class WebServer {
 public:
  WebServer(int port, const char* const hostname, Configuration& config) :
      m_asyncWebServer(AsyncWebServer(port)), m_hostname(hostname), m_config(config) {
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

  void redirectBackToHome(
    AsyncWebServerRequest* request,
    const std::chrono::seconds& delay);

  void reset(AsyncWebServerRequest* request, const char* reason);

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
  void makeUpload(const Element* element, std::stringstream& ss);

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

  [[nodiscard]] static bool isIp(const String& str);

  [[nodiscard]] Element* anyToElement(std::any& any);
};
}  // namespace esp_gui

#endif  // WEBSERVER_HPP_
