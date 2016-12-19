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
  git checkout -q -t origin/master -b work
  echo "some work done on a branch" >> test
  git add test; git commit -q -m "branch work"
  echo "some other work done on a branch" >> test
  git add test; git commit -q -m "branch work"

  test_expect_success "git-cl upload wants a server" \
    "$GIT_CL upload --no-oauth2 2>&1 | grep -q 'You must configure'"

  git config rietveld.server localhost:10000

  test_expect_success "git-cl status has no issue" \
    "$GIT_CL_STATUS | grep -q 'No issue assigned'"

  # Prevent the editor from coming up when you upload.
  export EDITOR=$(which true)
  test_expect_success "upload succeeds (needs a server running on localhost)" \
      "$GIT_CL upload --no-oauth2 -t test master | \
      grep -q 'Issue created'"

  test_expect_success "git-cl status now knows the issue" \
      "$GIT_CL_STATUS | grep -q 'Issue number'"

  # Check to see if the description contains the local commit messages.
  # Should contain 'branch work' x 2.
  test_expect_success "git-cl status has the right description for the log" \
      "[ $($GIT_CL_STATUS --field desc | egrep '^branch work$' -c) -eq 2 ]"

  test_expect_success "git-cl status has the right subject from message" \
      "$GIT_CL_STATUS --field desc | head -n 1 | grep -q '^test$'"

  test_expect_success "git-cl land ok" \
    "$GIT_CL land --bypass-hooks"

  git fetch origin
  git checkout origin/master

  test_expect_success "committed code has proper summary" \
      "[ $(git log -n 1 --pretty=format:%s | egrep '^test$' -c) -eq 1 ]"

  test_expect_success "committed code has proper description" \
      "[ $(git log -n 1 --pretty=format:%b | egrep '^branch work$' -c) -eq 2 ]"

  # # Have to sleep to let the server return the new status.
  # sleep 5
  # test_expect_success "branch issue is closed" \
  #     "$GIT_CL_STATUS | grep -q 'work :.*closed'"

)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
