# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile is used by debhelper, which supplies the appropriate value of
# variables not defined here, such as DESTDIR.

SRC = ../../../..
TOOLS = $(SRC)/remoting/tools
RELEASE = $(SRC)/out/Release

INSTALL_DIR = $(DESTDIR)/opt/google/chrome-remote-desktop

all:

install:
	install -d $(INSTALL_DIR)
	install $(TOOLS)/me2me_virtual_host.py -m 0755 \
	  $(INSTALL_DIR)/chrome-remote-desktop
	install $(TOOLS)/is-remoting-session $(INSTALL_DIR)
	install $(RELEASE)/remoting_me2me_host -m 0755 \
	  $(INSTALL_DIR)/chrome-remote-desktop-host
