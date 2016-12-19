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
  git checkout -q --track -b work origin
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
  export GIT_EDITOR=$(which true)

  test_expect_success "upload succeeds (needs a server running on localhost)" \
    "$GIT_CL upload  --no-oauth2  -m test master | grep -q 'Issue created'"

  test_expect_success "git-cl status now knows the issue" \
    "$GIT_CL_STATUS | grep -q 'Issue number'"

  # Push a description to this URL.
  URL=$($GIT_CL_STATUS | sed -ne '/Issue number/s/[^(]*(\(.*\))/\1/p')
  curl --cookie dev_appserver_login="test@example.com:False" \
       --data-urlencode subject="test" \
       --data-urlencode description="foo-quux" \
       --data-urlencode xsrf_token="$(print_xsrf_token)" \
       $URL/edit

  test_expect_success "git-cl land ok" \
    "$GIT_CL land -f --no-oauth2"

  test_expect_success "branch still has an issue" \
      "$GIT_CL_STATUS | grep -q 'Issue number'"

  git checkout -q master > /dev/null 2>&1
  git pull -q > /dev/null 2>&1

  test_expect_success "committed code has proper description" \
      "git show | grep -q 'foo-quux'"

  cd $GITREPO_PATH
  test_expect_success "upstream repo has our commit" \
      "git log master 2>/dev/null | grep -q 'foo-quux'"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
