//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <esp-gui/Util.hpp>
#include <esp-gui/WebServer.hpp>
#include <functional>
#include <string>
#include <typeindex>

namespace esp_gui {

static const constexpr char* const s_htmlIndexStart PROGMEM =
  R"(<!DOCTYPE html><html lang=en><title>%page_title%</title><meta charset=utf-8><meta content="width=device-width,user-scalable=no"name=viewport><style>html{background-color:#212121}p{font-weight:500}a:visited{text-decoration:none;color:#E0E0E0}a{text-decoration:none}*{margin:0;padding:0;color:#E0E0E0;overflow-x:hidden}body{font-size:16px;font-family:Roboto,sans-serif;font-weight:300;color:#4a4a4a}input,select{width:120px;background:#121212;border:none;border-radius:4px;padding-left:1rem;padding-right:1rem;height:50px;margin-bottom:.75em;font-size:.85rem;box-shadow:0 10px 20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.inputMedium{width:155px}.inputSmall{width:85px}.inputLarge{width:260px}.otherLarge{width:290px}label{margin-right:1em;font-size:1rem;display:inline-block;width:120px}.break{flex-basis:100%%;height:0}.btn{background:#303F9F;color:#EEE;border-radius:4px}.btnLarge{width:auto}.btnTop{margin-left:8px;margin-right:8px}.btnFlexContainer{width:290px}.flex-container{display:flex;flex-wrap:wrap}.flex-nav{flex-grow:1;flex-shrink:0;background:#303F9F;height:3rem}.featured{background:#3F51B5;color:#fff;padding:1em}.featured h1{font-size:2rem;margin-bottom:1rem;font-weight:300}.flex-card{overflow-y:hidden;flex:1;flex-shrink:0;flex-basis:400px;display:flex;flex-wrap:wrap;background:#212121;margin:.5rem;box-shadow:0 10px 20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.flex-card div{flex:100%%}.fit-content{height:fit-content}.flex-card .hero{position:relative;color:#fff;height:70px;background:linear-gradient(rgba(0,0,0,.5),rgba(0,0,0,.5)) no-repeat;background-size:cover}.flex-card .hero h3{position:absolute;bottom:15px;left:0;padding:0 1rem}.content{min-height:100%%;min-width:400px}.flex-card .content{color:#BDBDBD;padding:1.5rem 1rem 2rem 1rem}</style><div class=flex-container><div class=flex-nav></div></div><div class=featured><h1><a href=/ >%page_title%</a></h1></div><div><div style=margin-top:10px><form action=/eraseConfig enctype=multipart/form-data id=formEraseConfig method=POST></form><form action=/reboot enctype=multipart/form-data id=formReboot method=POST></form><form action=/ enctype=multipart/form-data id=formUpdateConfig method=POST></form><form action=/onClick enctype=multipart/form-data id=formOnClick method=POST></form></div><input class="btn btnLarge btnTop"form=formUpdateConfig type=submit value="Update settings"> <input class="btn btnLarge btnTop"form=formReboot type=submit value=Reboot> <input class="btn btnLarge btnTop"form=formEraseConfig type=submit value="Erase config"><div class="flex-container animated zoomIn">)";
static const constexpr char* const s_htmlIndexEnd = R"(</div></div></body></html>)";
static const constexpr char* const s_htmlRedirectDelayed PROGMEM =
  R"(<html lang=en><style>html{background-color:#424242;font-size:16px;font-family:Roboto,sans-serif;font-weight:300;color:#fefefe;text-align:center}</style><meta content=%redirect_seconds%;/ http-equiv=refresh><h1>Reloading in %redirect_seconds% seconds...</h1>)";
static const constexpr char* const s_htmlRedirectReset PROGMEM =
  R"(<html lang=en><style>html{background-color:#424242;font-size:16px;font-family:Roboto,sans-serif;font-weight:300;color:#fefefe;text-align:center}</style><meta content=%redirect_seconds%;/ http-equiv=refresh><h1>Resetting ESP8266</h1><h2>Reason:<h2><p>%s</p>)";

void WebServer::setup(const String& hostname) {
  m_logger.log(
    yal::Level::DEBUG, "Setting up web server with hostname: %", hostname.c_str());

  if (!containerSetupDone()) {
    m_logger.log(yal::Level::ERROR, "Failed to setup webinterface. reset esp!");
    EspClass::reset();
  }

  m_hostname = hostname;
  MDNS.begin(m_hostname);
  MDNS.addService("http", "tcp", 80);

  const char* rootPath = "/";

  m_asyncWebServer.on(
    rootPath,
    HTTP_POST,
    std::bind(&WebServer::rootHandlePost, this, std::placeholders::_1));

  m_asyncWebServer.on(
    "/eraseConfig",
    HTTP_POST,
    std::bind(&WebServer::eraseConfig, this, std::placeholders::_1));

  m_asyncWebServer.on(
    "/onClick", HTTP_POST, std::bind(&WebServer::onClick, this, std::placeholders::_1));

  m_asyncWebServer.on("/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
    reset(request, "User requested reboot");
  });

  m_asyncWebServer.onNotFound(
    std::bind(&WebServer::onNotFound, this, std::placeholders::_1));

  m_asyncWebServer.on(rootPath, HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isCaptivePortal(request)) {
      rootHandleGet(request);
    }
  });

  m_asyncWebServer.on(rootPath, HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isCaptivePortal(request)) {
      rootHandleGet(request);
    }
  });

  m_asyncWebServer.on(
    s_redirectDelayedURL, HTTP_GET, [this](AsyncWebServerRequest* request) {
      request->send_P(
        HTTP_OK,
        CONTENT_TYPE_HTML,
        s_htmlRedirectDelayed,
        [&](const String& templateString) { return String(m_redirectDelay.count()); });
    });

  m_asyncWebServer.begin();
  m_logger.log(yal::Level::DEBUG, "Web server ready");
}

void WebServer::addContainer(Container&& container) {
  m_container.push_back(container);
}

void WebServer::redirectBackToHome(
  AsyncWebServerRequest* request,
  const std::chrono::seconds& delay) {
  if (delay > 0s) {
    m_redirectDelay = delay;
    request->redirect(s_redirectDelayedURL);
  } else {
    request->redirect("/");
  }
}

bool WebServer::containerSetupDone() {
  for (auto& container : m_container) {
    for (auto& any : container.elements()) {
      Element* element = anyToElement(any);
      if (element != nullptr) {
        m_elementMap[element->configName()] = &any;
        m_logger.log(yal::Level::DEBUG, "Adding % to map", element->configName().c_str());
      }
    }
  }

  const auto checkResult = checkAndWriteHTML(false);
  if (checkResult == WriteAndCheckResult::SUCCESS) {
    return true;
  }

  m_logger.log(yal::Level::INFO, "MD5 mismatch, writing html index file");
  const auto writeResult = checkAndWriteHTML(true);
  return writeResult == WriteAndCheckResult::SUCCESS;
}

WebServer::WriteAndCheckResult WebServer::checkAndWriteHTML(bool writeFS) {
  unsigned int offset = 0U;

  const auto checkOrWrite = [&](
                              const unsigned int offset,
                              const uint8_t* const data,
                              const int size,
                              const bool clearFile = false) {
    if (writeFS) {
      m_logger.log(
        yal::Level::DEBUG, "writing % bytes to config at offset %", size, offset);
      if (!fileSystemWriteChunk(offset, data, size, clearFile)) {
        return WriteAndCheckResult::WRITE_FAILED;
      }
    } else {
      m_logger.log(
        yal::Level::DEBUG, "validating % bytes of config at offset %", size, offset);
      if (!fileSystemAndDataChunksEqual(offset, data, size)) {
        return WriteAndCheckResult::CHECKSUM_MISSMATCH;
      }
    }

    return WriteAndCheckResult::SUCCESS;
  };

  {
    const auto startLen = std::strlen(s_htmlIndexStart);
    const auto result = checkOrWrite(
      offset, reinterpret_cast<const uint8_t*>(s_htmlIndexStart), startLen, true);
    if (result != WriteAndCheckResult::SUCCESS) {
      return result;
    }

    offset += startLen;
  }

  for (auto& container : m_container) {
    std::stringstream ss;
    static const constexpr auto containerStart PROGMEM =
      R"(<div class="flex-card"><div class="hero">)";
    ss << containerStart;

    static const constexpr auto h3 PROGMEM = "<h3>";
    ss << h3;

    ss << container.title().c_str();

    static const constexpr auto containerClass PROGMEM =
      R"(</h3></div><div class="content">)";

    ss << containerClass;

    for (auto& any : container.elements()) {
      auto* element = anyToElement(any);
      if (nullptr == element) {
        m_logger.log(yal::Level::FATAL, "failed to cast any to element!");
        continue;
      }

      String elementValue = "%" + element->configName() + "%";
      String text;

      switch (element->type()) {
        case ElementType::BUTTON:
          makeButton(element, ss);
          break;
        case ElementType::LIST:
          makeDatalist(element, elementValue, "text", ss);
          break;
        case ElementType::DROPDOWN:
          makeSelect(element, elementValue, "text", ss);
          break;
        case ElementType::STRING:
          makeInput(element, elementValue, "text", ss);
          break;
        case ElementType::PASSWORD:
          makeInput(element, elementValue, "password", ss);
          break;
        case ElementType::INT:
        case ElementType::DOUBLE:
          makeInput(element, elementValue, "number", ss);
          break;
        case ElementType::UPLOAD:
          makeUpload(element, ss);
          break;
      }
    }

    static const auto containerEnd PROGMEM = "</div></div>";
    ss << containerEnd;

    ss.seekg(0, std::ios::end);
    const auto ssSize = ss.tellg();
    ss.seekg(0, std::ios::beg);

    const auto result =
      checkOrWrite(offset, reinterpret_cast<const uint8_t*>(ss.str().data()), ssSize);
    if (result != WriteAndCheckResult::SUCCESS) {
      return result;
    }

    offset += ssSize;
  }

  {
    const auto endLen = std::strlen(s_htmlIndexEnd);
    const auto result =
      checkOrWrite(offset, reinterpret_cast<const uint8_t*>(s_htmlIndexEnd), endLen);
    if (result != WriteAndCheckResult::SUCCESS) {
      return result;
    }
    offset += endLen;
  }

  if (!writeFS) {
    Configuration::FileHandle file(m_htmlIndex.c_str());
    file.open("r");
    if (file.file().seek(offset + 1)) {
      m_logger.log(yal::Level::DEBUG, "Trailing data in file");
      return WriteAndCheckResult::CHECKSUM_MISSMATCH;
    }
  }

  return WriteAndCheckResult::SUCCESS;
}

void WebServer::makeInput(
  const Element* element,
  const String& elementValue,
  const String& inputType,
  std::stringstream& ss) {
  const auto& id = element->configName().c_str();
  // clang-format off
  ss <<
    "<label for=\"" << id << "\">" << element->label().c_str() << "</label>"
    "<input id=\"" << id
      << R"(" class="inputLarge")"
      << "name=\"" << id << "\" "
      << "value=\"" << elementValue.c_str() << "\" "
      << "type=\"" << inputType.c_str() << "\" "
      << "form=\"formUpdateConfig\" "
      << "/>"
  "<br/>";
  // clang-format on
}

void WebServer::makeDatalist(
  const Element* element,
  const String& elementValue,
  const String& inputType,
  std::stringstream& ss) {
  const auto& id = element->configName().c_str();
  const auto listId = (id + m_optionSuffix);
  // clang-format off
  ss <<
    "<label for=\"" << id << "\">" << element->label().c_str() << "</label>"
    "<input id=\"" << id
      << R"(" class="inputLarge")"
      << "name=\"" << id << "\" "
      << "value=\"" << elementValue.c_str() << "\" "
      << "type=\"" << inputType.c_str() << "\" "
      << "list=\"" << listId.c_str() << "\" "
      << "form=\"formUpdateConfig\" "
      << "/>"
    << "<datalist id=\"" << listId.c_str() << "\">"
    << "%" << listId.c_str() << "%"
    << "</datalist>"
    << "<br/>";
  // clang-format on
}

void WebServer::makeSelect(
  const Element* element,
  const String& elementValue,
  const String& inputType,
  std::stringstream& ss) {
  const auto& id = element->configName().c_str();
  const auto optionId = (id + m_optionSuffix);
  // clang-format off
    ss <<
       "<label for=\"" << id << "\">" << element->label().c_str() << "</label>"
       "<select id=\"" << id
       << R"(" class="otherLarge")"
       << "name=\"" << id << "\" "
       << "type=\"" << inputType.c_str() << "\" "
       << "form=\"formUpdateConfig\" >"
       << "%" << optionId.c_str() << "%"
       << "</select>"
       << "<br/>";
  // clang-format on
}

void WebServer::makeButton(const Element* element, std::stringstream& ss) {
  const auto& id = element->configName().c_str();
  // clang-format off
  ss <<
    "<label for=\"" << id << "\"></label>"
    "<input id=\"" << id
      << R"(" class="btn btnFlexContainer otherLarge")"
      << "name=\"" << id << "\" "
      << "value=\"" << element->label().c_str() << "\" "
      << "form=\"formOnClick\" "
      << "type=\"submit\" "
      << "/>"
      << "<br/>";
  // clang-format on
}

void WebServer::makeUpload(const Element* element, std::stringstream& ss) {
  const auto& id = element->configName().c_str();
  const auto& idStr = element->configName();
  const auto& formId = element->configName() + "__form";
  const auto& browseId = (element->configName() + "__browse").c_str();
  const auto* upload = findElement<UploadElement>(idStr);
  const auto url = "/" + idStr + "__upload";

  m_asyncWebServer.on(url.c_str(), HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(HTTP_DENIED, CONTENT_TYPE_HTML, "403 Access denied");
  });

  m_asyncWebServer.on(
    url.c_str(),
    HTTP_POST,
    [upload](AsyncWebServerRequest* request) { upload->onPost(request); },
    [upload](
      AsyncWebServerRequest* request,
      const String& filename,
      size_t index,
      uint8_t* data,
      size_t len,
      bool final) { upload->onUpload(request, filename, index, data, len, final); });
  // clang-format off
  ss << "<form method='POST' action='"<< url.c_str()
     << "' enctype='multipart/form-data'>"
      << "<label for=\"" << browseId << "\">"
        << upload->browseLabel().c_str()
      << "</label>"
      << "<input type='file' class=\"input inputLarge\" accept='.bin,.bin.gz' "
        << "id=\"" << browseId << "\" "
        << "name=\"" << browseId << "\">"
      << "<label for=\"" << id << "\"></label>"
      << "<br/>"
      // dummy label to indent button
      << "<label for=\"" << browseId << "\">"
      << "</label>"
      << "<input type='submit' value='Upload' class=\"btn btnFlexContainer\""
        << "id=\"" << id << "\">"
    << "</form>"
    << "<br>";

  // clang-format on
}

bool WebServer::fileSystemAndDataChunksEqual(
  unsigned int offset,
  const uint8_t* data,
  int size) const {
  Configuration::FileHandle file(m_htmlIndex.c_str());
  if (!file.open("r")) {
    m_logger.log(yal::Level::ERROR, "Failed to read config from FS");
    return false;
  }

  if (!file.file().seek(offset, SeekSet)) {
    m_logger.log(yal::Level::ERROR, "Failed to seek to position %", offset);
    return false;
  }

  std::vector<uint8_t> fileContent;
  fileContent.reserve(size);
  const auto readSize = file.file().read(static_cast<uint8_t*>(fileContent.data()), size);
  if (readSize != size) {
    m_logger.log(yal::Level::ERROR, "Tried to read % bytes, received %", size, readSize);
    return false;
  }

  static constexpr const auto md5Len = 16;
  std::array<uint8_t, md5Len> fileMD5{};
  decltype(fileMD5) dataMD5{};

  const auto buildMD5 =
    [&](const uint8_t* inData, unsigned int inSize, decltype(fileMD5)& outData) {
      MD5Builder md5Builder{};
      md5Builder.begin();
      md5Builder.add(inData, inSize);
      md5Builder.calculate();
      md5Builder.getBytes(outData.data());
    };

  buildMD5(fileContent.data(), size, fileMD5);
  buildMD5(data, size, dataMD5);

  const auto equal = std::memcmp(fileMD5.data(), dataMD5.data(), md5Len);
  return equal == 0;
}

bool WebServer::fileSystemWriteChunk(
  unsigned int offset,
  const uint8_t* data,
  unsigned int size,
  bool clearFile) const {
  Configuration::FileHandle file(m_htmlIndex.c_str());

  const auto mode = clearFile ? "w" : "a";
  if (!file.open(mode)) {
    m_logger.log(yal::Level::FATAL, "failed to open html index file");
    return false;
  }

  if (!file.file().seek(offset)) {
    m_logger.log(
      yal::Level::FATAL, "failed to seek to offset % in html index file", offset);
    return false;
  }
  if (file.file().write(data, size) != size) {
    m_logger.log(yal::Level::FATAL, "failed write all bytes to html index file");
    return false;
  }

  m_logger.log(
    yal::Level::DEBUG, "updated html index at offset % with % bytes", offset, size);
  return true;
}

void WebServer::reset(AsyncWebServerRequest* request, const char* reason) {
  AsyncResponseStream* response = request->beginResponseStream(CONTENT_TYPE_HTML);
  response->printf(s_htmlRedirectReset, reason);
  response->addHeader("Connection", "close");
  request->onDisconnect([this]() {
    m_logger.log(yal::Level::WARNING, "Restarting");
    EspClass::reset();
  });

  request->send(response);
}

void WebServer::rootHandleGet(AsyncWebServerRequest* const request) {
  logMemory(m_logger);
  m_logger.log(yal::Level::DEBUG, "Received request for /");

  LittleFS.begin();
  request->send(
    LittleFS,
    m_htmlIndex,
    CONTENT_TYPE_HTML,
    false,
    std::bind(&WebServer::templateCallback, this, std::placeholders::_1));
}

String WebServer::templateCallback(const String& templateString) {
  String templ = templateString;
  bool getDataList = false;
  if (templ.endsWith(m_optionSuffix)) {
    getDataList = true;
    templ.remove(templ.length() - m_optionSuffix.length(), m_optionSuffix.length());
  }

  const auto listValue = findElement<ListElement>(templ);
  const auto dropdownValue = findElement<DropDownElement>(templ);
  if (listValue != nullptr) {
    return optionTemplate(templ, listValue, getDataList);
  } else if (dropdownValue != nullptr) {
    return optionTemplate(templ, dropdownValue, getDataList);
  } else {
    const auto value = m_config.value<String>(templ);
    m_logger.log(
      yal::Level::DEBUG,
      "Replacing template string '%' with %",
      templ.c_str(),
      value.c_str());
    return m_config.value<String>(templ);
  }
}

String WebServer::optionTemplate(
  const String& templ,
  const ChoiceElementBase* listValue,
  bool getDataList) {
  if (!getDataList) {
    return m_config.value<String>(templ);
  }

  String value;
  const auto selected = m_config.value<String>(listValue->configName());
  const auto& options = listValue->options();
  m_logger.log(yal::Level::DEBUG, "List has % options", options.size());

  for (const auto& option : options) {
    m_logger.log(yal::Level::DEBUG, "Adding select option %", option.c_str());
    String selectedStr;
    if (option == selected) {
      selectedStr = "selected";
    }
    value +=
      "<option value=\"" + option + "\" " + selectedStr + ">" + option + "</option>";
  }
  return value;
}

void WebServer::rootHandlePost(AsyncWebServerRequest* const request) {
  m_logger.log(yal::Level::INFO, "Received POST on /");
  for (size_t i = 0; i < request->params(); ++i) {
    const auto param = request->getParam(i);
    m_logger.log(
      yal::Level::DEBUG,
      "Updating param '%' to value '%'",
      param->name().c_str(),
      param->value().c_str());

    m_config.setValue(param->name(), param->value(), false);
  }

  m_config.store();

  redirectBackToHome(request, 0s);
}

void WebServer::eraseConfig(AsyncWebServerRequest* const request) {
  m_config.reset(true);
  redirectBackToHome(request, 0s);
}

void WebServer::onClick(AsyncWebServerRequest* const request) {
  for (size_t i = 0; i < request->params(); ++i) {
    const auto param = request->getParam(i);
    m_logger.log(yal::Level::DEBUG, "Calling button %", param->name().c_str());
    auto button = findElement<ButtonElement>(param->name());
    if (button == nullptr) {
      continue;
    }
    button->click();

    redirectBackToHome(request, button->delay());
    return;
  }
}

bool WebServer::isCaptivePortal(AsyncWebServerRequest* request) {
  if (m_hostname.isEmpty()) {
    return false;
  }

  const auto hostHeader = request->getHeader("host")->value();
  const auto hostIsIp = isIp(hostHeader);

  const auto captive = !hostIsIp && (!hostHeader.startsWith(m_hostname));
  m_logger.log(
    yal::Level::TRACE,
    "Captivity Portal Check: "
    "hostname %, host header %, isCaptive %",
    m_hostname.c_str(),
    hostHeader.c_str(),
    captive);
  if (!captive) {
    return false;
  }

  AsyncWebServerResponse* response = request->beginResponse(HTTP_FOUND, "text/plain", "");
  response->addHeader("Location", "http://" + WiFi.softAPIP().toString());
  request->send(response);
  m_logger.log(yal::Level::TRACE, "Redirect for config portal");
  return true;
}

void WebServer::onNotFound(AsyncWebServerRequest* request) {
  if (isCaptivePortal(request)) {
    return;
  }

  request->send(
    HTTP_NOT_FOUND,
    CONTENT_TYPE_HTML,
    "<!DOCTYPE html><html><head><title>404</title></head><body><h1>404</h1></body>");
}

bool WebServer::isIp(const String& str) {
  return std::all_of(
    str.begin(), str.end(), [](char c) { return !(c != '.' && (c < '0' || c > '9')); });

  // regex needs about 6k of stack (we only have 4KB in total)
  //  static std::regex expr(
  //    R"((\b25[0-5]|\b2[0-4][0-9]|\b[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3})");
  //  return std::regex_match(str.c_str(), expr);
}

Element* WebServer::anyToElement(std::any& any) {
  if (std::type_index(typeid(Element)) == any.type()) {
    return std::any_cast<Element>(&any);
  } else if (std::type_index(typeid(ListElement)) == any.type()) {
    return std::any_cast<ListElement>(&any);
  } else if (std::type_index(typeid(ButtonElement)) == any.type()) {
    return std::any_cast<ButtonElement>(&any);
  } else if (std::type_index(typeid(InputElement)) == any.type()) {
    return std::any_cast<InputElement>(&any);
  } else if (std::type_index(typeid(UploadElement)) == any.type()) {
    return std::any_cast<UploadElement>(&any);
  } else if (std::type_index(typeid(DropDownElement)) == any.type()) {
    return std::any_cast<DropDownElement>(&any);
  }

  m_logger.log(yal::Level::ERROR, "failed to cast %", any.type().name());

  return nullptr;
}

}  // namespace esp_gui
