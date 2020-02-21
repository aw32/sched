# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause

BASE_DIR=$(HOME)/sched/install

# normal g++
GPP=g++

# clang
#GPP=clang++

# cc8
#GPP=/opt/rh/devtoolset-2/root/usr/bin/g++

GPPFLAGS=-std=c++11 -Wall -Werror -Wno-unused

.PHONY: docs install clean build_dir

all: build_dir
	cd build && cmake3 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(GPP) -DCMAKE_CXX_FLAGS="$(GPPFLAGS)" -DCMAKE_INSTALL_PREFIX=$(BASE_DIR) .. && make

debug: build_dir
	cd build && cmake3 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=$(GPP) -DCMAKE_CXX_FLAGS="$(GPPFLAGS)" -DCMAKE_INSTALL_PREFIX=$(BASE_DIR) .. && make -j 16

install:
	mkdir -p $(BASE_DIR)/bin $(BASE_DIR)/scripts
	cd build && make install

build_dir:
	mkdir -p build

clean:
	rm -r build

docs:
	doxygen doxygen.conf
