#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

HELPERTOOLS=/Library/PrivilegedHelperTools
NAME=org.chromium.chromoting
CONFIG_FILE="$HELPERTOOLS/$NAME.json"
PLIST=/Library/LaunchAgents/org.chromium.chromoting.plist
ENABLED_FILE="$HELPERTOOLS/$NAME.me2me_enabled"
ENABLED_FILE_BACKUP="$ENABLED_FILE.backup"

KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
KSUPDATE=https://tools.google.com/service/update2
KSPID=com.google.chrome_remote_desktop
KSPVERSION=@@VERSION_SHORT@@

trap onexit ERR

function onexit {
  # Log an error but don't report an install failure if this script has errors.
  logger An error occurred while launching the service
  exit 0
}

# Create auth file (with correct owner and permissions) if it doesn't already
# exist.
if [[ ! -f "$CONFIG_FILE" ]]; then
  touch "$CONFIG_FILE"
  chmod 600 "$CONFIG_FILE"
  chmod +a "$USER:allow:read" "$CONFIG_FILE"
fi

# If there is a backup _enabled file, re-enable the service.
if [[ -f "$ENABLED_FILE_BACKUP" ]]; then
  mv "$ENABLED_FILE_BACKUP" "$ENABLED_FILE"
fi

# Load the service.
# The launchctl command we'd like to run:
#   launchctl load -w -S Aqua $PLIST
# However, since we're installing as an admin, the launchctl command is run
# as if it had a sudo prefix, which means it tries to load the service in
# system rather than user space.
# To launch the service in user space, we need to get the current user (using
# ps and grepping for the loginwindow.app) and run the launchctl cmd as that
# user (using launchctl bsexec).
set `ps aux | grep loginwindow.app | grep -v grep`
USERNAME=$1
USERID=$2
if [[ -n "$USERNAME" && -n "$USERID" ]]; then
  launchctl bsexec "$USERID" sudo -u "$USERNAME" launchctl load -w -S Aqua "$PLIST"
fi

# Register a ticket with Keystone so we're updated.
$KSADMIN --register --productid "$KSPID" --version "$KSPVERSION" --xcpath "$PLIST" --url "$KSUPDATE"
