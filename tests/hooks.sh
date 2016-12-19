#!/usr/bin/env bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tests the "preupload and preland hooks" functionality, which lets you run
# hooks by installing a script into .git/hooks/pre-cl-* first.

set -e

. ./test-lib.sh

setup_git_remote
setup_git_checkout

setup_hooks() {
  upload_retval=$1
  land_retval=$2

  echo > PRESUBMIT.py <<END
def CheckChangeOnUpload(input_api, output_api):
  return $upload_retval

def CheckChangeOnCommit(input_api, output_api):
  return $land_retval
END
}

(
  set -e
  cd git_checkout

  # We need a server set up, but we don't use it.  git config rietveld.server localhost:1

  # Install hooks that will fail on upload and commit
  setup_hooks 1 1

  echo "some work done" >> test
  git add test; git commit -q -m "work"

  # Verify git cl upload fails.
  test_expect_failure "git-cl upload hook fails" "$GIT_CL upload master"

  # Verify git cl land fails.
  test_expect_failure "git-cl land hook fails" "$GIT_CL land master"
)
SUCCESS=$?

#cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
