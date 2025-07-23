#!/bin/bash

set -xe

make clean
make distclean
./bootstrap.sh
make -j$(nproc)
