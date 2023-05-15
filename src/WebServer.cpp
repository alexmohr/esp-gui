//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <ESP8266mDNS.h>
#include <esp-gui/Util.hpp>
#include <esp-gui/WebServer.hpp>
#include <functional>
#include <string>

namespace esp_gui {

static const constexpr char* const s_indexDataStart =
  R"(<!DOCTYPE html><html lang=en><title>%page_title%</title><meta charset=utf-8><meta content="width=device-width,user-scalable=no"name=viewport><style>html{background-color:#212121}p{font-weight:500}a:visited{text-decoration:none;color:#E0E0E0}a{text-decoration:none}*{margin:0;padding:0;color:#E0E0E0;overflow-x:hidden}body{font-size:16px;font-family:Roboto,sans-serif;font-weight:300;color:#4a4a4a}input,select{width:120px;background:#121212;border:none;border-radius:4px;padding:1rem;height:50px;margin:.25em;font-size:1rem;box-shadow:0 10px 20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.input-group-text{width:120px;background:#121212;border:none;border-radius:4px;padding:1rem;height:50px;margin-left:-.5em;z-index:-1;font-size:1rem;box-shadow:0 10px 20px -20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.inputMedium{width:155px}.inputSmall{width:85px}.inputLarge{width:260px}label{margin-right:1em;font-size:1.15rem;display:inline-block;width:85px}.break{flex-basis:100%%;height:0}.btn{background:#303F9F;color:#EEE;border-radius:4px}.btnLarge{width:262px}.flex-container{display:flex;flex-wrap:wrap}.flex-nav{flex-grow:1;flex-shrink:0;background:#303F9F;height:3rem}.featured{background:#3F51B5;color:#fff;padding:1em}.featured h1{font-size:2rem;margin-bottom:1rem;font-weight:300}.flex-card{overflow-y:hidden;flex:1;flex-shrink:0;flex-basis:400px;display:flex;flex-wrap:wrap;background:#212121;margin:.5rem;box-shadow:0 10px 20px rgba(0,0,0,.19),0 6px 6px rgba(0,0,0,.23)}.flex-card div{flex:100%%}.fit-content{height:fit-content}.flex-card .hero{position:relative;color:#fff;height:70px;background:linear-gradient(rgba(0,0,0,.5),rgba(0,0,0,.5)) no-repeat;background-size:cover}.flex-card .hero h3{position:absolute;bottom:15px;left:0;padding:0 1rem}.content{min-height:100%%;min-width:400px}.flex-card .content{color:#BDBDBD;padding:1.5rem 1rem 2rem 1rem}</style><div class=flex-container><div class=flex-nav></div></div><div class=featured><h1><a href=/ >%page_title%</a></h1></div><form action=/eraseConfig enctype=multipart/form-data method=POST style=margin-left:20px;margin-top:20px;margin-bottom:-20px><input class="btn btnLarge"value="Erase config"type=submit></form><form action=/ enctype=multipart/form-data method=POST style=margin:20px><input class="btn btnLarge"value="Update settings & Reboot"type=submit><div class="flex-container animated zoomIn"><div class=flex-card><div class=hero><h3>WiFi Configuration</h3></div><div class=content><label for=wifi_ssid>SSID</label><input class=inputLarge value=%wifi_ssid% id=wifi_ssid name=wifi_ssid list=wifi_networklist><datalist id=wifi_networklist>%wifi_networklist%</datalist><br><label for=wifi_password>Password</label><input class=inputLarge value=%wifi_password% id=wifi_password name=wifi_password type=password><br><label for=wifi_hostname>Hostname</label><input class=inputLarge value=%wifi_hostname% id=wifi_hostname name=wifi_hostname></div></div>)";

static const constexpr char* const s_indexDataEnd = R"(</div></form></body></html>)";

void WebServer::setup(const String& hostname) {
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

  m_asyncWebServer.begin();
  m_logger.log(yal::Level::DEBUG, "Web server ready");
}

void WebServer::addContainer(Container&& container) {
  m_container.emplace_back(container);
}

void WebServer::addToContainerData(const char* const data) {
  const auto len = std::strlen(data);
  const auto remainingSize = std::min(m_containerData.size() - m_containerDataUsed, len);

  memcpy(m_containerData.data() + m_containerDataUsed, data, remainingSize);
  m_containerDataUsed += remainingSize;

  m_logger.log(
    yal::Level::INFO,
    "Container RAM usage: % of % bytes (%)",
    m_containerDataUsed,
    m_containerData.size(),
    (static_cast<float>(m_containerDataUsed) / static_cast<float>(m_containerData.size())) * 100.0F);
}

void WebServer::containerSetupDone() {
  // todo support lists

  for (int containerIdx = 0; containerIdx < m_container.size(); ++containerIdx) {
    auto& container = m_container.at(containerIdx);
    static const constexpr auto containerStart PROGMEM =
      R"(<div class="flex-card"><div class="hero">)";
    addToContainerData(containerStart);

    static const constexpr auto h3 PROGMEM = "<h3>";
    addToContainerData(h3);

    addToContainerData(container.title().c_str());

    static const constexpr auto containerClass PROGMEM =
      R"(</h3></div><div class="content">)";
    addToContainerData(containerClass);

    for (int elementIdx = 0; elementIdx < container.elements().size(); ++elementIdx) {
      const auto element = container.elements().at(elementIdx);
      const auto& strId = element.configName();

      // todo catch std::any cast errors
      // todo support lists
      String elementValue = "%" + element.configName() + "%";
      String elementHTMLType;
      switch (element.type()) {
        case ElementType::STRING:

           // std::any_cast<String>(element.value());
          elementHTMLType = "text";
          break;
        case ElementType::STRING_PASSWORD:
         // elementValue = std::any_cast<String>(element.value());
          elementHTMLType = "password";
          break;
        case ElementType::INT:
         // elementValue = String(std::any_cast<int>(element.value()));
          elementHTMLType = "number";
          break;
        case ElementType::DOUBLE:
         // elementValue = String(std::any_cast<double>(element.value()));
          elementHTMLType = "number";
          break;
      }

      const auto labelAndInput = "<label for=\"" + strId + "\">" + element.label() +
                                 "</label>"
                                 "<input id=\"" +
                                 strId + R"(" class="inputLarge")" + "name=\"" + strId +
                                 +"\" value=\"" + elementValue + "\" " + "type=\"" +
                                 elementHTMLType + "\"</><br/>";
      addToContainerData(labelAndInput.c_str());
    }
    static const auto containerEnd PROGMEM = "</div></div>";
    addToContainerData(containerEnd);
  }
}

void WebServer::reset(
  AsyncWebServerRequest* const request,
  AsyncResponseStream* const response) {
  //  response->addHeader("Connection", "close");
  //  request->onDisconnect([this]() {
  //    m_logger.log(yal::Level::WARNING, "Restarting");
  //    EspClass::reset();
  //  });
  // todo fix me
}

void WebServer::rootHandleGet(AsyncWebServerRequest* const request) {
  logMemory(m_logger);
  m_logger.log(yal::Level::DEBUG, "Received request for /");
  static const auto indexStartLen = std::strlen(s_indexDataStart);
  static const auto indexEndLen = std::strlen(s_indexDataEnd);

  AsyncWebServerResponse* response = request->beginChunkedResponse(
    CONTENT_TYPE_HTML,
    [&](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
      if (index < indexStartLen) {
        return chunkedResponseCopy(
          index, maxLen, buffer, s_indexDataStart, indexStartLen);
      }

      if (index < (indexStartLen + m_containerDataUsed)) {
        return chunkedResponseCopy(
          index - indexStartLen,
          maxLen,
          buffer,
          m_containerData.data(),
          m_containerDataUsed);
      }

      return chunkedResponseCopy(
        index - indexStartLen - m_containerDataUsed,
        maxLen,
        buffer,
        s_indexDataEnd,
        indexEndLen);
    },
    std::bind(&WebServer::templateCallback, this, std::placeholders::_1));
  request->send(response);
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

  const auto captive =
    !hostIsIp && ((hostHeader != m_hostname) || (hostHeader != m_hostname + ".local"));
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
