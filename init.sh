#!/usr/bin/env bash
function init () {
  platformio -c clion init --ide clion
  pio pkg update
}
init
cd examples 
init
