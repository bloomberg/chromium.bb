# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile is used by debhelper, which supplies the appropriate value of
# variables not defined here, such as DESTDIR.

SRC = ../../../..
TOOLS = $(SRC)/remoting/tools
RELEASE = $(SRC)/out/Release

BIN = $(DESTDIR)/usr/bin
LIB = $(DESTDIR)/usr/lib/chrome-remote-desktop

all:

install:
	install -d $(BIN) $(LIB)
	install $(TOOLS)/me2me_virtual_host.py -m 0755 \
	  $(BIN)/me2me_virtual_host
	install $(TOOLS)/is_me2me_desktop $(LIB)
	install $(RELEASE)/remoting_me2me_host -m 0755 $(LIB)
