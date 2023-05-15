#!/bin/bash 
find . -not -path "*.pio*" -regex '.*\.\(cpp\|hpp\)' -exec clang-format -style=file -i {} \;

