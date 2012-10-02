#!/bin/bash

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run the gesture regression test and check if there is any
# regression for each submit.

# Set current directory to the project one and load the common script.
cd "$(dirname "$(readlink -f "$0")")/.."
. "../../scripts/common.sh" || exit 1

update_chroot_libgestures() {
  info "Check chroot libgestures version ${commit_head_hash}..."

  if ! grep -q ${commit_head_hash} /usr/lib/libgestures.so; then
    info "Update the gestures library under chroot.."
    sudo emerge -q gestures
    if ! grep -q ${commit_head_hash} /usr/lib/libgestures.so; then
      die_notrace "Can not install libgestures successfully"
    fi
  fi
}

install_regression_test_suite() {
  info "Install regression test suite first..."
  sudo emerge -q gestures libevdev utouch-evemu -j3
  pushd ~/trunk/src/platform/touchpad-tests >/dev/null
  make -j${NUM_JOBS} -s all
  sudo make -s local-install
  popd >/dev/null
}

run_regression_tests() {
  info "Run regression tests..."

  touchtests run all | egrep "failure|error"
  if [[ $? -eq 0 ]]; then
    die_notrace "Regression Tests failed, please check your patch again."
  fi
}

check_test_setup() {
  if [[ ! -e  /usr/lib/libgestures.so ]]; then
    install_regression_test_suite
  else
    update_chroot_libgestures
  fi
}

commit_head_hash=`git rev-parse HEAD`
if [[ ${INSIDE_CHROOT} -ne 1 ]]; then
  if [[ "${PRESUBMIT_COMMIT}" == "${commit_head_hash}" ]]; then
    restart_in_chroot_if_needed "$@"
  fi
else
  cros_workon --host start gestures
  check_test_setup
  run_regression_tests
fi
exit 0
