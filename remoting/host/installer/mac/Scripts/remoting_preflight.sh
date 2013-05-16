#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Version = @@VERSION@@

HELPERTOOLS=/Library/PrivilegedHelperTools
SERVICE_NAME=org.chromium.chromoting
CONFIG_FILE="$HELPERTOOLS/$SERVICE_NAME.json"
SCRIPT_FILE="$HELPERTOOLS/$SERVICE_NAME.me2me.sh"
USERS_TMP_FILE="$SCRIPT_FILE.users"
PLIST=/Library/LaunchAgents/org.chromium.chromoting.plist
ENABLED_FILE="$HELPERTOOLS/$SERVICE_NAME.me2me_enabled"
ENABLED_FILE_BACKUP="$ENABLED_FILE.backup"

# In case of errors, log the fact, but continue to unload launchd jobs as much
# as possible. When finished, this preflight script should exit successfully in
# case this is a Keystone-triggered update which must be allowed to proceed.
function on_error {
  logger An error occurred during Chrome Remote Desktop setup.
}

function find_users_with_active_hosts {
  ps -eo uid,command | awk -v script="$SCRIPT_FILE" '
    $2 == "/bin/sh" && $3 == script && $4 == "--run-from-launchd" {
      print $1
    }'
}

function find_login_window_for_user {
  # This function mimics the behaviour of pgrep, which may not be installed
  # on Mac OS X.
  local user=$1
  ps -ec -u "$user" -o comm,pid | awk '$1 == "loginwindow" { print $2; exit }'
}

trap on_error ERR

logger Running Chrome Remote Desktop preflight script @@VERSION@@

# If there is an _enabled file, rename it while upgrading.
if [[ -f "$ENABLED_FILE" ]]; then
  mv "$ENABLED_FILE" "$ENABLED_FILE_BACKUP"
fi

# Stop and unload the service for each user currently running the service, and
# record the user IDs so the service can be restarted for the same users in the
# postflight script.
rm -f "$USERS_TMP_FILE"

for uid in $(find_users_with_active_hosts); do
  if [[ -n "$uid" ]]; then
    echo "$uid" >> "$USERS_TMP_FILE"
    if [[ "$uid" = "0" ]]; then
      context="LoginWindow"
    else
      context="Aqua"
    fi
    pid="$(find_login_window_for_user "$uid")"
    if [[ -n "$pid" ]]; then
      launchctl bsexec "$pid" sudo -u "#$uid" launchctl stop "$SERVICE_NAME"
      launchctl bsexec "$pid" sudo -u "#$uid" launchctl unload -w -S \
        "$context" "$PLIST"
    fi
  fi
done

exit 0
