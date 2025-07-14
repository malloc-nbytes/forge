#!/bin/bash

set -xe

if command -v curl >/dev/null 2>&1; then
    if [ ! -e "src/sqlite-autoconf-3500100" ]; then
        # wget https://sqlite.org/2025/sqlite-autoconf-3500100.tar.gz
        curl -O https://sqlite.org/2025/sqlite-autoconf-3500100.tar.gz
        mv sqlite-autoconf-3500100.tar.gz ./src
        cd src
        tar -vxzf ./sqlite-autoconf-3500100.tar.gz
        cd ..
    fi
    autoreconf --install
    ./configure --prefix=/usr --libdir=/usr/lib64
    exit 0
else
    unset -x
    echo "wget is not installed, it is required to get sqlite."
    echo "You could also download it by visiting this link: https://sqlite.org/2025/sqlite-autoconf-3500100.tar.gz"
    exit 256
fi

