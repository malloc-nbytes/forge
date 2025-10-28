#!/usr/bin/env earl

module Debug

$"cc *.c forge-headers-src/*.c -o forge-debug-build -Iinclude/ -I. -lsqlite3 -ggdb -O0";
