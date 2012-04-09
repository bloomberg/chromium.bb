#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

HELPERTOOLS=/Library/PrivilegedHelperTools
NAME=org.chromium.chromoting
AUTH_FILE="$HELPERTOOLS/$NAME.json"
PLIST=/Library/LaunchAgents/org.chromium.chromoting.plist

KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
KSUPDATE=https://tools.google.com/service/update2
KSPID=com.google.chrome_remote_desktop
KSPVERSION=0.5

trap onexit ERR

function onexit {
  # Log an error but don't report an install failure if this script has errors.
  logger An error occurred while launching the service
  exit 0
}

# Update owner and permissions for auth file.
chown $USER "$AUTH_FILE"
chmod 600 "$AUTH_FILE"

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
# At this point, $1=username and $2=userid
if [[ -n $1 && -n $2 ]]; then
  launchctl bsexec "$2" sudo -u "$1" launchctl load -w -S Aqua $PLIST
fi

# Register a ticket with Keystone so we're updated.
$KSADMIN --register --productid $KSPID --version $KSPVERSION --xcpath $PLIST --url $KSUPDATE
