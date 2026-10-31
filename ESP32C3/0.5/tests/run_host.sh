#!/bin/sh
set -eu
cd "$(dirname "$0")"
bin=$(mktemp)
trap 'rm -f "$bin"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -Werror -DCONFIG_IDF_TARGET_ESP32C3 -I stubs -I .. test_host.cpp -o "$bin"
"$bin"
