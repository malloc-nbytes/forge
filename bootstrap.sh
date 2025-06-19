#!/bin/bash

set -xe

wget https://sqlite.org/2025/sqlite-autoconf-3500100.tar.gz
mv sqlite-autoconf-3500100.tar.gz ./src
cd src
tar -vxzf ./sqlite-autoconf-3500100.tar.gz
cd ..
autoreconf --install
