#!/bin/bash

set -e

. ./test-lib.sh

setup_initgit
setup_gitgit

(
  set -e
  cd git-git
  git checkout -q -b work HEAD^
  git checkout -q -t -b work2 work
  echo "some work done on a branch that tracks a local branch" >> test
  git add test; git commit -q -m "local tracking branch work"

  git config rietveld.server localhost:8080

  # Prevent the editor from coming up when you upload.
  export EDITOR=$(which true)
  test_expect_success "upload succeeds (needs a server running on localhost)" \
    "$GIT_CL upload -m test | grep -q 'Issue created'"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
