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
PAM_CONFIG=/etc/pam.d/chrome-remote-desktop
ENABLED_FILE="$HELPERTOOLS/$SERVICE_NAME.me2me_enabled"
ENABLED_FILE_BACKUP="$ENABLED_FILE.backup"
LOG_FILE=/var/log/org.chromium.chromoting.log

KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
KSUPDATE=https://tools.google.com/service/update2
KSPID=com.google.chrome_remote_desktop
KSPVERSION=@@VERSION@@

function on_error {
  logger An error occurred during Chrome Remote Desktop setup.
  exit 1
}

function find_login_window_for_user {
  # This function mimics the behaviour of pgrep, which may not be installed
  # on Mac OS X.
  local user=$1
  ps -ec -u "$user" -o comm,pid | awk '$1 == "loginwindow" { print $2; exit }'
}

trap on_error ERR
trap 'rm -f "$USERS_TMP_FILE"' EXIT

logger Running Chrome Remote Desktop postflight script @@VERSION@@

# Register a ticket with Keystone to keep this package up to date.
$KSADMIN --register --productid "$KSPID" --version "$KSPVERSION" \
    --xcpath "$PLIST" --url "$KSUPDATE"

# If there is a backup _enabled file, re-enable the service.
if [[ -f "$ENABLED_FILE_BACKUP" ]]; then
  mv "$ENABLED_FILE_BACKUP" "$ENABLED_FILE"
fi

# Create the PAM configuration unless it already exists and has been edited.
update_pam=1
CONTROL_LINE="# If you edit this file, please delete this line."
if [[ -f "$PAM_CONFIG" ]] && ! grep -qF "$CONTROL_LINE" "$PAM_CONFIG"; then
  update_pam=0
fi

if [[ "$update_pam" == "1" ]]; then
  logger Creating PAM config.
  cat > "$PAM_CONFIG" <<EOF
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

auth       required   pam_deny.so
account    required   pam_permit.so
password   required   pam_deny.so
session    required   pam_deny.so

# This file is auto-updated by the Chrome Remote Desktop installer.
$CONTROL_LINE
EOF
else
  logger PAM config has local edits. Not updating.
fi

# Create the log file (if this isn't created ahead of time
# then directing output from the service there won't work).
# Make sure admins have write privileges (CRD users are
# typically admins)
touch "$LOG_FILE"
chown :admin "$LOG_FILE"
chmod 660 "$LOG_FILE"

# Load the service for each user for whom the service was unloaded in the
# preflight script (this includes the root user, in case only the login screen
# is being remoted and this is a Keystone-triggered update).
# Also, in case this is a fresh install, load the service for the user running
# the installer, so they don't have to log out and back in again.
if [[ -n "$USER" && "$USER" != "root" ]]; then
  id -u "$USER" >> "$USERS_TMP_FILE"
fi

if [[ -r "$USERS_TMP_FILE" ]]; then
  for uid in $(sort "$USERS_TMP_FILE" | uniq); do
    logger Starting service for user "$uid".

    if [[ "$uid" = "0" ]]; then
      context="LoginWindow"
    else
      context="Aqua"
    fi

    # Load the launchd agent in the bootstrap context of user $uid's graphical
    # session, so that screen-capture and input-injection can work. To do this,
    # find the PID of a process which is running in that context. The
    # loginwindow process is a good candidate since the user (if logged in to
    # a session) will definitely be running it.
    pid="$(find_login_window_for_user "$uid")"
    if [[ -n "$pid" ]]; then
      launchctl bsexec "$pid" sudo -u "#$uid" launchctl load -w -S Aqua "$PLIST"
      launchctl bsexec "$pid" sudo -u "#$uid" launchctl start "$SERVICE_NAME"
    fi
  done
fi
