#!/usr/bin/env bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. ./test-lib.sh

setup_git_remote
setup_git_checkout

(
  set -e
  cd git_checkout
  git config rietveld.server localhost:10000
  export GIT_EDITOR=$(which true)

  git checkout -q -b work
  echo "ben@chromium.org" > OWNERS
  cat <<END > PRESUBMIT.py
def CheckChangeOnCommit(input_api, output_api):
  return input_api.canned_checks.CheckOwners(input_api, output_api)

CheckChangeOnUpload = CheckChangeOnCommit
END

  git add OWNERS PRESUBMIT.py ; git commit -q -m "add OWNERS"

  test_expect_success "upload succeeds (needs a server running on localhost)" \
    "$GIT_CL upload --no-oauth2 -m test master | grep -q 'Issue created'"

  test_expect_failure "git-cl land fails w/ missing LGTM" \
    "$GIT_CL land -f --no-oauth2"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
