# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile is used by debhelper, which supplies the appropriate value of
# variables not defined here, such as DESTDIR.

SRC = ../../../..
TOOLS = $(SRC)/remoting/tools
RELEASE = $(SRC)/out/Release

INSTALL_DIR = $(DESTDIR)/opt/google/chrome-remote-desktop
CRON_DIR = $(DESTDIR)/etc/cron.daily
NATIVE_MESSAGING_DIR = $(DESTDIR)/etc/opt/chrome/native-messaging-hosts

ME2ME_PROGNAME = $(RELEASE)/remoting_me2me_host
ME2ME_DEBUGFILE = $(ME2ME_PROGNAME).debug
START_PROGNAME = $(RELEASE)/remoting_start_host
START_DEBUGFILE = $(START_PROGNAME).debug
ME2ME_NM_PROGNAME = $(RELEASE)/remoting_native_messaging_host
ME2ME_NM_DEBUGFILE = $(ME2ME_NM_PROGNAME).debug
REMOTE_ASSISTANCE_PROGNAME = $(RELEASE)/remoting_it2me_native_messaging_host
REMOTE_ASSISTANCE_DEBUGFILE = $(REMOTE_ASSISTANCE_PROGNAME).debug

all:

install:
	install -d "$(INSTALL_DIR)"
	install -d "$(INSTALL_DIR)/remoting_locales"
	install -d "$(NATIVE_MESSAGING_DIR)"
	install -d "$(CRON_DIR)"

	install "$(TOOLS)/me2me_virtual_host.py" \
	  "$(INSTALL_DIR)/chrome-remote-desktop"
	install "$(TOOLS)/is-remoting-session" "$(INSTALL_DIR)"

	install -m 0644 \
	    "$(RELEASE)/remoting/com.google.chrome.remote_desktop.json" \
	   "$(NATIVE_MESSAGING_DIR)/com.google.chrome.remote_desktop.json"

	install -m 0644 \
	  "$(RELEASE)/remoting/com.google.chrome.remote_assistance.json" \
	  "$(NATIVE_MESSAGING_DIR)/com.google.chrome.remote_assistance.json"

	eu-strip -f "$(ME2ME_DEBUGFILE)" "$(ME2ME_PROGNAME)"
	install "$(ME2ME_PROGNAME)" \
	  "$(INSTALL_DIR)/chrome-remote-desktop-host"

	eu-strip -f "$(START_DEBUGFILE)" "$(START_PROGNAME)"
	install "$(START_PROGNAME)" "$(INSTALL_DIR)/start-host"

	eu-strip -f "$(ME2ME_NM_DEBUGFILE)" "$(ME2ME_NM_PROGNAME)"
	install "$(ME2ME_NM_PROGNAME)" "$(INSTALL_DIR)/native-messaging-host"

	eu-strip -f "$(REMOTE_ASSISTANCE_DEBUGFILE)" \
	  "$(REMOTE_ASSISTANCE_PROGNAME)"
	install "$(REMOTE_ASSISTANCE_PROGNAME)" \
	  "$(INSTALL_DIR)/remote-assistance-host"

	install -m 0644 \
          "$(RELEASE)/icudtl.dat" "$(INSTALL_DIR)/icudtl.dat"

	for locale in $$(ls $(RELEASE)/remoting_locales); do \
	  install "$(RELEASE)/remoting_locales/$$locale" \
	    "$(INSTALL_DIR)/remoting_locales/$$locale"; \
	done

	install $(RELEASE)/remoting/installer/cron/chrome-remote-desktop \
	  "$(CRON_DIR)/chrome-remote-desktop"
