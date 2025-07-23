#!/bin/bash

set -x

make clean
make distclean
./bootstrap.sh
make -j$(nproc)
