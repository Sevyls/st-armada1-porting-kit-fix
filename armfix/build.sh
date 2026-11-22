#!/bin/sh
# Builds bin/Strlkup.dll (32-bit) with MinGW-w64.
#   macOS:  brew install mingw-w64
#   Debian/Ubuntu:  apt install gcc-mingw-w64-i686
set -e
cd "$(dirname "$0")"
i686-w64-mingw32-gcc -O2 -shared -s -o ../bin/Strlkup.dll armfix.c strlkup.def \
    -luser32 -lgdi32 -static-libgcc
echo "built ../bin/Strlkup.dll"
