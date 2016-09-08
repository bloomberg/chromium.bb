#!/bin/bash
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(ayatane): Unify bin/run_sysmon.sh with this once this is stable.

cd "$(dirname "$(readlink -f -- "$0")")"
if [ requirements.txt -nt venv/timestamp ]; then
  echo Creating or updating virtualenv in venv/
  virtualenv venv --extra-search-dir=pip_packages
  source venv/bin/activate
  pip install --no-index -f pip_packages -r requirements.txt
  touch venv/timestamp
else
  echo Existing virtualenv is already up to date.
fi
