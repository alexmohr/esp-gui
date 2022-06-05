//
// Copyright (c) 2022 Alexander Mohr
// Licensed under the terms of the MIT license
//

#include <esp-gui/Util.hpp>
void logMemory(const yal::Logger& logger) {
  logger.log(
    yal::Level::DEBUG,
    "Free heap: %, free stack %",
    EspClass::getFreeHeap(),
    EspClass::getFreeContStack());
}
