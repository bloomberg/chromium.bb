#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(garykac): Replace this with a proper double-clickable uninstaller app.
NAME=org.chromium.chromoting
LAUNCHAGENTS=/Library/LaunchAgents
HELPERTOOLS=/Library/PrivilegedHelperTools
PLIST="$LAUNCHAGENTS/$NAME.plist"

# Stop service if currently running.
# TODO(garykac): Trap errors and replace sleep with a check for when the
# service is stopped.
launchctl stop $NAME
launchctl unload -w -S Aqua $PLIST
sleep 1

# Cleanup files from old versions of installer.
sudo rm -f "$LAUNCHAGENTS/com.google.chrome_remote_desktop.plist"
sudo rm -f "$HELPERTOOLS/remoting_me2me_host"
sudo rm -f "$HELPERTOOLS/me2me.sh"
sudo rm -f "$HELPERTOOLS/com.google.chrome_remote_desktop.me2me_host"
sudo rm -f "$HELPERTOOLS/com.google.chrome_remote_desktop.me2me.sh"
sudo rm -f ~/.ChromotingConfig.json
sudo rm -f "$HELPERTOOLS/auth.json"
sudo rm -f "$HELPERTOOLS/host.json"
sudo rm -f "$HELPERTOOLS/me2me_enabled"
sudo rm -f "$HELPERTOOLS/com.google.chrome_remote_desktop.me2me_enabled"
sudo rm -rf /ChromotingSetup

# Cleanup installed files.
sudo rm -f $PLIST
sudo rm -f "$HELPERTOOLS/$NAME.me2me_host"
sudo rm -f "$HELPERTOOLS/$NAME.me2me.sh"
sudo rm -f "$HELPERTOOLS/$NAME.json"
sudo rm -f "$HELPERTOOLS/$NAME.me2me_enabled"

# Unregister our ticket from Keystone.
KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
KSPID=com.google.chrome_remote_desktop
$KSADMIN --delete --productid $KSPID
