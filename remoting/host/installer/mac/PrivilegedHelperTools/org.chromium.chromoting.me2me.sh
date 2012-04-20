#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

NAME=org.chromium.chromoting
CONFIG_DIR=/Library/PrivilegedHelperTools
HOST_EXE=$CONFIG_DIR/$NAME.me2me_host
ENABLED_FILE=$CONFIG_DIR/$NAME.me2me_enabled
CONFIG_FILE=$CONFIG_DIR/$NAME.json

if [ "$1" = "--disable" ]; then
  # This script is executed from base::mac::ExecuteWithPrivilegesAndWait(),
  # which requires the child process to write its PID to stdout before
  # anythine else. See base/mac/authorization_util.h for details.
  echo $$
  rm -f "$ENABLED_FILE"
elif [ "$1" = "--enable" ]; then
  echo $$
  cat > "$CONFIG_FILE"
  touch "$ENABLED_FILE"
elif [ "$1" = "--save-config" ]; then
  echo $$
  cat > "$CONFIG_FILE"
else
  exec "$HOST_EXE" \
    --auth-config="$CONFIG_FILE" \
    --host-config="$CONFIG_FILE"
fi
