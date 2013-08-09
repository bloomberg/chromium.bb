# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile is used by debhelper, which supplies the appropriate value of
# variables not defined here, such as DESTDIR.

SRC = ../../../..
TOOLS = $(SRC)/remoting/tools
RELEASE = $(SRC)/out/Release

INSTALL_DIR = $(DESTDIR)/opt/google/chrome-remote-desktop
CHROME_MANIFEST_DIR = $(DESTDIR)/etc/opt/chrome/native-messaging-hosts
MANIFEST = $(CHROME_MANIFEST_DIR)/com.google.chrome.remote_desktop.json

ME2ME_PROGNAME = $(RELEASE)/remoting_me2me_host
ME2ME_DEBUGFILE = $(ME2ME_PROGNAME).debug
START_PROGNAME = $(RELEASE)/remoting_start_host
START_DEBUGFILE = $(START_PROGNAME).debug
NM_PROGNAME = $(RELEASE)/remoting_native_messaging_host
NM_DEBUGFILE = $(NM_PROGNAME).debug

all:

install:
	install -d "$(INSTALL_DIR)"
	install -d "$(CHROME_MANIFEST_DIR)"

	install "$(TOOLS)/me2me_virtual_host.py" \
	  "$(INSTALL_DIR)/chrome-remote-desktop"
	install "$(TOOLS)/is-remoting-session" "$(INSTALL_DIR)"

	install -T "$(RELEASE)/remoting/native_messaging_manifest.json" \
	  -m 0644 "$(MANIFEST)"

	eu-strip -f "$(ME2ME_DEBUGFILE)" "$(ME2ME_PROGNAME)"
	install "$(ME2ME_PROGNAME)" \
	  "$(INSTALL_DIR)/chrome-remote-desktop-host"

	eu-strip -f "$(START_DEBUGFILE)" "$(START_PROGNAME)"
	install "$(START_PROGNAME)" "$(INSTALL_DIR)/start-host"

	eu-strip -f "$(NM_DEBUGFILE)" "$(NM_PROGNAME)"
	install "$(NM_PROGNAME)" "$(INSTALL_DIR)/native-messaging-host"
