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

# Escape '.' -> '\.' for regular-expression matching in pgrep, as pgrep doesn't
# have any option for fixed-string matching.
script_command_regex="${SCRIPT_FILE//./\\.} --run-from-launchd"

# '-f' for full argument matching has to be used, as the executable command is
# just '/bin/sh'.
for pid in $(pgrep -f "$script_command_regex" || true); do
  # 'tail' is needed to strip the header from 'ps'.
  uid="$(ps -p "$pid" -o uid | tail -n 1)"
  if [[ -n "$uid" ]]; then
    echo "$uid" >> "$USERS_TMP_FILE"
    if [[ "$uid" = "0" ]]; then
      context="LoginWindow"
    else
      context="Aqua"
    fi
    launchctl bsexec "$pid" sudo -u "#$uid" launchctl stop "$SERVICE_NAME"
    launchctl bsexec "$pid" sudo -u "#$uid" launchctl unload -w -S "$context" \
      "$PLIST"
  fi
done

exit 0
