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

  cat > .git/hooks/post-cl-land << _EOF
#!/usr/bin/env bash
git branch -m COMMITTED
_EOF
  chmod +x .git/hooks/post-cl-land

  git config rietveld.server localhost:1
  git checkout -q -t origin/master -b work
  echo "some work done" >> test
  git add test; git commit -q -m "work \
TBR=foo"

  test_expect_success "landed code" \
      "$GIT_CL land --no-oauth2 -f --bypass-hooks -m 'land'"

  test_expect_success "post-cl-land hook executed" \
      "git symbolic-ref HEAD | grep -q COMMITTED"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
