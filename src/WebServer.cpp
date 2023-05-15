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

namespace esp_gui {

static const constexpr char* const s_htmlIndexStart PROGMEM =
  R"(<!DOCTYPE html><html lang=en><title>%page_title%</title><meta charset=utf-8><meta content="width=device-width,user-scalable=no"name=viewport><style>html{background-color:#212121}p{font-weight:500}a:visited{text-decoration:none;color:#E0E0E0}a{text-decoration:none}*{margin:0;padding:0;color:#E0E0E0;overflow-x:hidden}body{font-size:16px;font-family:Roboto,sans-serif;font-weight:300;color:#4a4a4a}input,select{width:120px;background:#121212;border:none;border-radius:4px;padding:1rem;height:50px;margin:.25em;font-size:1rem;box-shadow:0 10px 20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.input-group-text{width:120px;background:#121212;border:none;border-radius:4px;padding:1rem;height:50px;margin-left:-.5em;z-index:-1;font-size:1rem;box-shadow:0 10px 20px -20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.inputMedium{width:155px}.inputSmall{width:85px}.inputLarge{width:260px}label{margin-right:1em;font-size:1.15rem;display:inline-block;width:85px}.break{flex-basis:100%%;height:0}.btn{background:#303F9F;color:#EEE;border-radius:4px}.btnLarge{width:262px}.flex-container{display:flex;flex-wrap:wrap}.flex-nav{flex-grow:1;flex-shrink:0;background:#303F9F;height:3rem}.featured{background:#3F51B5;color:#fff;padding:1em}.featured h1{font-size:2rem;margin-bottom:1rem;font-weight:300}.flex-card{overflow-y:hidden;flex:1;flex-shrink:0;flex-basis:400px;display:flex;flex-wrap:wrap;background:#212121;margin:.5rem;box-shadow:0 10px 20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.flex-card div{flex:100%%}.fit-content{height:fit-content}.flex-card .hero{position:relative;color:#fff;height:70px;background:linear-gradient(rgba(0,0,0,.5),rgba(0,0,0,.5)) no-repeat;background-size:cover}.flex-card .hero h3{position:absolute;bottom:15px;left:0;padding:0 1rem}.content{min-height:100%%;min-width:400px}.flex-card .content{color:#BDBDBD;padding:1.5rem 1rem 2rem 1rem}</style><div class=flex-container><div class=flex-nav></div></div><div class=featured><h1><a href=/ >%page_title%</a></h1></div><div><form action=/eraseConfig enctype=multipart/form-data method=POST style=margin-left:20px;margin-top:20px;margin-bottom:-20px><input class="btn btnLarge"value="Erase config"type=submit></form><form action=/ enctype=multipart/form-data method=POST style=margin:20px><input class="btn btnLarge"value="Update settings & Reboot"type=submit><div class="flex-container animated zoomIn"><div class=flex-card><div class=hero><h3>WiFi Configuration</h3></div><div class=content><label for=wifi_ssid>SSID</label><input class=inputLarge value=%wifi_ssid% id=wifi_ssid name=wifi_ssid list=wifi_networklist><datalist id=wifi_networklist>%wifi_networklist%</datalist><br><label for=wifi_password>Password</label><input class=inputLarge value=%wifi_password% id=wifi_password name=wifi_password type=password><br><label for=wifi_hostname>Hostname</label><input class=inputLarge value=%wifi_hostname% id=wifi_hostname name=wifi_hostname></div></div>)";
static const constexpr char* const s_htmlIndexEnd = R"(</div></form><div class="animated flex-container zoomIn"><div class="animated flex-container zoomIn"><div class=flex-card><div class=hero><h3>System</h3></div><div class=content><h3>Firmware update</h3><form action=/update enctype=multipart/form-data method=POST><input class="input inputLarge"type=file accept=.bin,.bin.gz name=firmware> <input class=btn type=submit value=Update></form></body><html>)";
static const constexpr char* const s_htmlRedirect15s PROGMEM = R"(<html lang=en><style>html{background-color:#424242;font-size:16px;font-family:Roboto,sans-serif;font-weight:300;color:#fefefe;text-align:center}</style><meta content=15;/ http-equiv=refresh><h1>Operation %s, reloading in 15 seconds...</h1>)";


void WebServer::setup(const String& hostname) {
  m_logger.log(
    yal::Level::DEBUG, "Setting up web server with hostname: %", hostname.c_str());
  m_container.reserve(10);
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

  m_asyncWebServer.onNotFound(
    std::bind(&WebServer::onNotFound, this, std::placeholders::_1));

  m_asyncWebServer.on(rootPath, HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isCaptivePortal(request)) {
      rootHandleGet(request);
    }
  });

  static constexpr const char* updatePath = "/update";
  m_asyncWebServer.on(updatePath, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(HTTP_DENIED, CONTENT_TYPE_HTML, "403 Access denied");
  });

  m_asyncWebServer.on(
    updatePath,
    HTTP_POST,
    std::bind(&WebServer::updateHandlePost, this, std::placeholders::_1),
    std::bind(&WebServer::updateHandleUpload, this,
      std::placeholders::_1,
      std::placeholders::_2,
      std::placeholders::_3,
      std::placeholders::_4,
      std::placeholders::_5,
      std::placeholders::_6));
  
  m_asyncWebServer.begin();
  m_logger.log(yal::Level::DEBUG, "Web server ready");
}

void WebServer::addContainer(Container&& container) {
  m_container.emplace_back(container);
}

bool WebServer::containerSetupDone() {
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

  // todo support lists
  for (const auto& container : m_container) {
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

    for (const auto& element : container.elements()) {
      const auto& strId = element.configName();

      // todo catch std::any cast errors
      // todo support lists
      String elementValue = "%" + element.configName() + "%";
      String elementHTMLType;
      switch (element.type()) {
        case ElementType::STRING:
          elementHTMLType = "text";
          break;
        case ElementType::STRING_PASSWORD:
          elementHTMLType = "password";
          break;
        case ElementType::INT:
        case ElementType::DOUBLE:
          elementHTMLType = "number";
          break;
      }

      const auto labelAndInput = "<label for=\"" + strId + "\">" + element.label() +
                                 "</label>"
                                 "<input id=\"" +
                                 strId + R"(" class="inputLarge")" + "name=\"" + strId +
                                 +"\" value=\"" + elementValue + "\" " + "type=\"" +
                                 elementHTMLType + "\"</><br/>";
      ss << labelAndInput.c_str();
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

void WebServer::reset(
  AsyncWebServerRequest* const request,
  AsyncResponseStream* const response) {
    response->addHeader("Connection", "close");
    request->onDisconnect([this]() {
      m_logger.log(yal::Level::WARNING, "Restarting");
      EspClass::reset();
    });
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

size_t WebServer::chunkedResponseCopy(
  size_t index,
  size_t maxLen,
  uint8_t* dst,
  const char* const source,
  size_t sourceLength) {
  const auto idxBytesLeft = sourceLength - index;
  auto bytesCopied = std::min(maxLen, idxBytesLeft);
  if (idxBytesLeft == 0) {
    return 0;
  }
  memcpy(dst, source + index, bytesCopied);

  return bytesCopied;
}

String WebServer::templateCallback(const String& templateString) {
  auto value = m_config.value<String>(templateString);
  m_logger.log(
    yal::Level::DEBUG,
    "Replacing template string '%' with %",
    templateString.c_str(),
    value.c_str());
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

  request->send(
    HTTP_OK,
    CONTENT_TYPE_HTML,
    "<html><head><title>200</title></head><body><h1>OK</h1></body>");
}

void WebServer::updateHandlePost(AsyncWebServerRequest* request) {
  m_logger.log(yal::Level::INFO, "Getting ready for some new firmware");

  const bool updateSuccess = !Update.hasError();
  AsyncResponseStream* const response = request->beginResponseStream(CONTENT_TYPE_HTML);
  response->printf(s_htmlRedirect15s, updateSuccess ? "succeeded" : "failed");

  if (updateSuccess) {
    reset(request, response);
  }

  request->send(response);
}

void WebServer::updateHandleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    m_logger.log(yal::Level::INFO, "Starting update with file: %", filename.c_str());

    Update.runAsync(true);
    if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
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

void WebServer::eraseConfig(AsyncWebServerRequest* const request) {
  m_config.reset(true);
  request->send(
    HTTP_NOT_FOUND,
    CONTENT_TYPE_HTML,
    "<html><head><title>200</title></head><body><h1>OK</h1></body>");
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
    "<html><head><title>404</title></head><body><h1>404</h1></body>");
}

bool WebServer::isIp(const String& str) {
  return std::all_of(
    str.begin(), str.end(), [](char c) { return !(c != '.' && (c < '0' || c > '9')); });

  // regex needs about 6k of stack (we only have 4KB in total)
  //  static std::regex expr(
  //    R"((\b25[0-5]|\b2[0-4][0-9]|\b[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3})");
  //  return std::regex_match(str.c_str(), expr);
}

}  // namespace esp_gui
