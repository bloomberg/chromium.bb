#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

NAME=org.chromium.chromoting
CONFIG_DIR=/Library/PrivilegedHelperTools
HOST_EXE=$CONFIG_DIR/$NAME.me2me_host.app/Contents/MacOS/remoting_me2me_host
PLIST_FILE=$CONFIG_DIR/$NAME.me2me_host.app/Contents/Info.plist
ENABLED_FILE=$CONFIG_DIR/$NAME.me2me_enabled
CONFIG_FILE=$CONFIG_DIR/$NAME.json

# The exit code returned by 'wait' when a process is terminated by SIGTERM.
SIGTERM_EXIT_CODE=143

# Range of exit codes returned by the host to indicate that a permanent error
# has occurred and that the host should not be restarted. Please, keep these
# constants in sync with remoting/host/constants.h.
MIN_PERMANENT_ERROR_EXIT_CODE=2
MAX_PERMANENT_ERROR_EXIT_CODE=4

HOST_PID=0
SIGNAL_WAS_TRAPPED=0

handle_signal() {
  SIGNAL_WAS_TRAPPED=1
}

run_host() {
  # This script works as a proxy between launchd and the host. Signals of
  # interest to the host must be forwarded.
  trap "handle_signal" SIGHUP SIGINT SIGQUIT SIGILL SIGTRAP SIGABRT SIGEMT \
      SIGFPE SIGKILL SIGBUS SIGSEGV SIGSYS SIGPIPE SIGALRM SIGTERM SIGURG \
      SIGSTOP SIGTSTP SIGCONT SIGCHLD SIGTTIN SIGTTOU SIGIO SIGXCPU SIGXFSZ \
      SIGVTALRM SIGPROF SIGWINCH SIGINFO SIGUSR1 SIGUSR2

  while true; do
    if [[ ! -f "$ENABLED_FILE" ]]; then
      echo "Daemon is disabled."
      exit 0
    fi

    # Execute the host asynchronously
    "$HOST_EXE" --auth-config="$CONFIG_FILE" --host-config="$CONFIG_FILE" &
    HOST_PID="$!"

    # Wait for the host to return and process its exit code.
    while true; do
      wait "$HOST_PID"
      EXIT_CODE="$?"
      if [[ $SIGNAL_WAS_TRAPPED -eq 1 ]]; then
        # 'wait' returned as the result of a trapped signal and the exit code is
        # the signal that was trapped + 128. Forward the signal to the host.
        SIGNAL_WAS_TRAPPED=0
        local SIGNAL=$(($EXIT_CODE - 128))
        echo "Forwarding signal $SIGNAL to host"
        kill -$SIGNAL "$HOST_PID"
      elif [[ "$EXIT_CODE" -eq "$SIGTERM_EXIT_CODE" ||
              ("$EXIT_CODE" -ge "$MIN_PERMANENT_ERROR_EXIT_CODE" && \
              "$EXIT_CODE" -le "$MAX_PERMANENT_ERROR_EXIT_CODE") ]]; then
        echo "Host returned permanent exit code $EXIT_CODE"
        exit "$EXIT_CODE"
      else
        # Ignore non-permanent error-code and launch host again.
        echo "Host returned non-permanent exit code $EXIT_CODE"
        break
      fi
    done
  done
}

if [[ "$1" = "--disable" ]]; then
  # This script is executed from base::mac::ExecuteWithPrivilegesAndWait(),
  # which requires the child process to write its PID to stdout before
  # anythine else. See base/mac/authorization_util.h for details.
  echo $$
  rm -f "$ENABLED_FILE"
elif [[ "$1" = "--enable" ]]; then
  echo $$
  cat > "$CONFIG_FILE"
  touch "$ENABLED_FILE"
elif [[ "$1" = "--save-config" ]]; then
  echo $$
  cat > "$CONFIG_FILE"
elif [[ "$1" = "--host-version" ]]; then
  PlistBuddy -c "Print CFBundleVersion" "$PLIST_FILE"
elif [[ "$1" = "--run-from-launchd" ]]; then
  run_host
else
  echo $$
  exit 1
fi
