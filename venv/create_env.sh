#!/bin/bash
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

# Activate virtualenv
activate() {
  local venv_dir=$1
  # virtualenv relies on unset variables, so we disable unset variable checking
  # just for this.
  set +u
  source "$venv_dir/bin/activate"
  set -u
}

venv_dir="venv"

cd "$(dirname "$(readlink -f -- "$0")")"

(
  # Try for 90 seconds to acquire an exclusive lock on virtualenv.
  flock -w 90 9 || ( echo "Failed to acquire lock on virtualenv."; exit 1 )
  installed="$venv_dir/.installed.txt"
  if ! cmp -s requirements.txt "$installed" ; then
    echo "Creating or updating virtualenv in $(pwd)/$venv_dir/"
    virtualenv "$venv_dir" --extra-search-dir=pip_packages
    activate "$venv_dir"
    pip install --no-index -f pip_packages -r requirements.txt
    cp requirements.txt "$installed"
  else
    echo "Existing virtualenv $(pwd)/$venv_dir/ is already up to date."
  fi
) 9>.venv_lock
