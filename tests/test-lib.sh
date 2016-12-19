#!/usr/bin/env bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Abort on error.
set -e

export DEPOT_TOOLS_UPDATE=0
export PYTHONUNBUFFERED=

PWD=$(pwd)
GITREPO_PATH=$PWD/git_remote
GITREPO_URL=file://$GITREPO_PATH
PATH="$(dirname $PWD):$PATH"
GIT_CL=$(dirname $PWD)/git-cl
GIT_CL_STATUS="$GIT_CL status -f"

# Set up a git repo that has a few commits to master.
setup_git_remote() {
  echo "Setting up test upstream git repo..."
  rm -rf git_remote
  mkdir git_remote

  (
    cd git_remote
    git init -q
    git config user.name 'TestDood'
    git config user.email 'TestDood@example.com'
    echo "test" > test
    git add test
    git commit -qam "initial commit"
    echo "test2" >> test
    git commit -qam "second commit"
    # Hack: make sure master is not the current branch
    #       otherwise push will give a warning
    git checkout -q --detach master
  )
}

# Set up a git checkout of the repo.
setup_git_checkout() {
  echo "Setting up test git repo..."
  rm -rf git_checkout
  git clone -q $GITREPO_URL git_checkout
  (
    cd git_checkout
    git config user.name 'TestDood'
    git config user.email 'TestDood@example.com'
  )
}

cleanup() {
  rm -rf git_remote git_checkout
}

# Usage: test_expect_success "description of test" "test code".
test_expect_success() {
  echo "TESTING: $1"
  exit_code=0
  sh -c "$2" || exit_code=$?
  if [ $exit_code != 0 ]; then
    echo "FAILURE: $1"
    return $exit_code
  fi
}

# Usage: test_expect_failure "description of test" "test code".
test_expect_failure() {
  echo "TESTING: $1"
  exit_code=0
  sh -c "$2" || exit_code=$?
  if [ $exit_code = 0 ]; then
    echo "SUCCESS, BUT EXPECTED FAILURE: $1"
    return $exit_code
  fi
}

# Grab the XSRF token from the review server and print it to stdout.
print_xsrf_token() {
  curl --cookie dev_appserver_login="test@example.com:False" \
    --header 'X-Requesting-XSRF-Token: 1' \
    http://localhost:10000/xsrf_token 2>/dev/null
}
