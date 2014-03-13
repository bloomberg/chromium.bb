#!/bin/bash

# URL from which the latest version of this script can be downloaded.
script_url="http://src.chromium.org/svn/trunk/src/tools/android/adb_remote_setup.sh"

# Replaces this file with the latest version of the script and runs it.
update-self() {
  local script="${BASH_SOURCE[0]}"
  local new_script="${script}.new"
  local updater_script="${script}.updater"
  curl -f -o "$new_script" "$script_url" || return
  chmod +x "$new_script" || return

  # Replace this file with the newly downloaded script.
  cat > "$updater_script" << EOF
#!/bin/bash
if mv "$new_script" "$script"; then
  rm -- "$updater_script"
else
  echo "Note: script update failed."
fi
ADB_REMOTE_SETUP_NO_UPDATE=1 exec /bin/bash "$script" $@
EOF
  exec /bin/bash "$updater_script" "$@"
}

if [[ "$ADB_REMOTE_SETUP_NO_UPDATE" -ne 1 ]]; then
  update-self "$@" || echo 'Note: script update failed'
fi

if [[ $# -ne 2 ]]; then
  cat <<'EOF'
Usage: adb_remote_setup.sh REMOTE_HOST REMOTE_ADB

Configures adb on a remote machine to communicate with a device attached to the
local machine. This is useful for installing APKs, running tests, etc while
working remotely.

Arguments:
  REMOTE_HOST  hostname of remote machine
  REMOTE_ADB   path to adb on the remote machine
EOF
  exit 1
fi

remote_host="$1"
remote_adb="$2"

if which kinit >/dev/null; then
  # Allow ssh to succeed without typing your password multiple times.
  kinit -R || kinit
fi

# Kill the adb server on the remote host.
ssh "$remote_host" "$remote_adb kill-server"

# Start the adb server locally.
adb start-server

# Forward various ports from the remote host to the local host:
#   5037: adb
#   8001: http server
#   9031: sync server
#   10000: net unittests
#   10201: net unittests
ssh -C \
    -R 5037:localhost:5037 \
    -L 8001:localhost:8001 \
    -L 9031:localhost:9031 \
    -R 10000:localhost:10000 \
    -R 10201:localhost:10201 \
    "$remote_host"
