#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Version = @@VERSION@@

HELPERTOOLS=/Library/PrivilegedHelperTools
NAME=org.chromium.chromoting
CONFIG_FILE="$HELPERTOOLS/$NAME.json"
PLIST=/Library/LaunchAgents/org.chromium.chromoting.plist
ENABLED_FILE="$HELPERTOOLS/$NAME.me2me_enabled"
ENABLED_FILE_BACKUP="$ENABLED_FILE.backup"

trap onexit ERR

function onexit {
  # Log an error but don't report an install failure if this script has errors.
  logger An error occurred while launching the service
  exit 0
}

logger Running Chrome Remote Desktop preflight script @@VERSION@@

# If there is an _enabled file, rename it while upgrading.
if [[ -f "$ENABLED_FILE" ]]; then
  mv "$ENABLED_FILE" "$ENABLED_FILE_BACKUP"
fi

# Stop and unload the service.
set `ps aux | grep loginwindow.app | grep -v grep`
USERNAME=$1
USERID=$2
if [[ -n "$USERNAME" && -n "$USERID" ]]; then
  launchctl bsexec "$USERID" sudo -u "$USERNAME" launchctl stop "$NAME"
  launchctl bsexec "$USERID" sudo -u "$USERNAME" launchctl unload -w -S Aqua "$PLIST"
fi
