#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
######################################################################
#
# This script facilitates merging changes from the LLVM upstream
# repository into the PNaCl repository.
#
######################################################################
# TODO(dschuff): Automate the process of getting the right clang commit
# TODO(dschuff): make it work for the merge bots
# TODO(dschuff): Add push step? or just instructions in banner for push
if [[ ${PNACL_BUILDBOT} == "true" ]]; then
  set -o xtrace
fi
set -o nounset
set -o errexit


# The script is located in "pnacl/scripts".
# Set pwd to pnacl/
cd "$(dirname "$0")"/..
if [[ $(basename "$(pwd)") != "pnacl" ]] ; then
  echo "ERROR: cannot find pnacl/ directory"
  exit -1
fi

source scripts/common-tools.sh
readonly PNACL_ROOT="$(pwd)"

SetScriptPath "${PNACL_ROOT}/scripts/merge-tool-git.sh"
SetLogDirectory "${PNACL_ROOT}/build/log"
readonly MERGE_LOG_FILE="${PNACL_ROOT}/merge.log"
######################################################################

# Location of the sources
# These should match the values in build.sh
readonly TC_SRC="${PNACL_ROOT}/git"
readonly TC_SRC_LLVM="${TC_SRC}"/llvm
readonly TC_SRC_CLANG="${TC_SRC}"/clang

readonly PREDIFF="${TC_SRC}/prediff"
readonly POSTDIFF="${TC_SRC}/postdiff"

readonly UPSTREAM_URL=http://llvm.org/git/llvm.git

readonly UPSTREAM_REMOTE=llvm-upstream
readonly PNACL_REMOTE=origin

readonly UPSTREAM_REMOTE_BRANCH=remotes/${UPSTREAM_REMOTE}/master
readonly PNACL_REMOTE_BRANCH=remotes/${PNACL_REMOTE}/master

readonly UPSTREAM_BRANCH=upstream/master
readonly PNACL_BRANCH=master

#@ auto            - Non-interactive merge
auto() {
  INTERACTIVE_MERGE=false
  export DISPLAY=""
  local revision=$1
  merge-all ${revision}
  checkout-clang $(get-clang-rev ${revision})
}

#@ manual [git rev or "HEAD"]          - Interactive merge
manual() {
  INTERACTIVE_MERGE=true
  local revision=$1
  if [[ $revision == "HEAD" ]]; then
    pull-upstream
    revision=$(get-head-revision)
  fi
  merge-all ${revision}
  echo "Clang rev corresponging to LLVM rev ${revision} is"
  get-clang-rev ${revision}
  echo "You should manually check it out"
}

#@ upstream-remote-setup - Add the LLVM remote repo to a pnacl git checkout
upstream-remote-setup() {
  pushd "${TC_SRC_LLVM}"
  if ! git remote | grep ${UPSTREAM_REMOTE}; then
    git remote add ${UPSTREAM_REMOTE} ${UPSTREAM_URL}
    git fetch ${UPSTREAM_REMOTE}
    git branch ${UPSTREAM_BRANCH} ${UPSTREAM_REMOTE}/master
  fi
  popd
}

#+ get-head-revision   - get the head revision for LLVM
# Must have already fetched the upstream remote
get-head-revision() {
  spushd ${TC_SRC_LLVM}
  git rev-parse ${UPSTREAM_REMOTE_BRANCH}
  spopd
}

#+ get-git-svn-revision  - Return the git revision matching the given SVN rev
get-git-svn-revision() {
  spushd ${TC_SRC_LLVM}
  git rev-list --grep=@${1} ${UPSTREAM_REMOTE}/master
  spopd
}

pull-upstream() {
  spushd ${TC_SRC_LLVM}
  # Get the upstream changes onto the local tracking branch
  git checkout ${UPSTREAM_BRANCH}
  git fetch ${UPSTREAM_REMOTE}
  git merge --ff ${UPSTREAM_REMOTE_BRANCH}
  spopd
}

pull-pnacl() {
  git checkout ${PNACL_BRANCH}
  git fetch ${PNACL_REMOTE}
  git merge --ff ${PNACL_REMOTE_BRANCH}
}

#+ merge-all [git rev]  - Merge everything
merge-all() {
  if [[ $# -ne 1 ]]; then
    Fatal "Please specify revision"
  fi
  local revision=$1

  cd "${TC_SRC_LLVM}"

  git-assert-no-changes .
  generate-diff "${PREDIFF}"

  pull-upstream
  pull-pnacl

  echo "@@@BUILD_STEP Merge ${revision}@@@"
  git-merge ${revision}

  generate-diff "${POSTDIFF}"

  if ${INTERACTIVE_MERGE}; then
    vim-diff-diff
  else
    dump-diff-diff
  fi

  final-banner
}

final-banner() {
  echo "********************************************************************"
  echo "The pnacl/git/llvm working directory is now in a merged state."
  echo "Before you commit and push, you should build PNaCl and run all tests."
  echo ""
  echo "1) Build PNaCl:"
  echo "     pnacl/build.sh build-all"
  echo "     pnacl/build.sh translator-all"
  echo ""
  #TODO(dschuff): Is this still everything we want to test?
  echo "2) Run all tests:"
  echo "     pnacl/test.sh test-all"
  echo ""
  echo "Depending on the size of the merge, there may be lots of bugs. You may"
  echo "need to fix and rebuild several times. When you are confident all tests"
  echo "are passing, you can commit and push."
  echo "********************************************************************"
}

#+ generate-diff     - Generate upstream:pnacl diff from pnacl branch point
generate-diff() {
  local diff_file=$1
  local base=$(git merge-base ${PNACL_BRANCH} ${UPSTREAM_BRANCH})
  git diff ${base} ${PNACL_BRANCH} &> "${diff_file}"
}

git-merge() {
  local revision=$1
  git checkout ${PNACL_BRANCH}
  git merge --no-commit $1 2>&1 | tee "${MERGE_LOG_FILE}"
  local gitret=${PIPESTATUS[0]}
  if [[ ${gitret} != 0 ]]; then
    echo "==== REMAINING MERGE CONFLICTS (file deletes, etc.) ===="
    grep CONFLICT "${MERGE_LOG_FILE}"
    echo "==== END grep of ${MERGE_LOG_FILE} ===="
    echo "Please handle these file changes manually."
    return 1
  fi
  return 0
}

#+ vim-diff-diff         - Review diff-diff using vim
vim-diff-diff() {
  vimdiff "${PREDIFF}" "${POSTDIFF}"
}

#+dump-diff-diff        - Review diff-diff
dump-diff-diff() {
  diff "${PREDIFF}" "${POSTDIFF}" || true
}

#@ find-last-good-rev    - Find the most recent revision with no merge conflict
find-last-good-rev() {
  cd "${TC_SRC_LLVM}"
  git-assert-no-changes .
  pull-upstream
  pull-pnacl

  local revs=$(git rev-list ${UPSTREAM_BRANCH} ^${PNACL_BRANCH})
  for rev in ${revs}; do
    if ! git-merge ${rev}; then
      git merge --abort
    else
      echo "Successful at rev ${rev}"
      return 0
    fi
  done
}

#+ checkout-clang [rev]   - Checkout the specified revision of clang
checkout-clang() {
  local rev=$1
  spushd "${TC_SRC_CLANG}"
  git-assert-no-changes .
  git checkout $1
  spopd
}

#@ find-first-bad-rev    - Find the first revision with a merge conflict
find-first-bad-rev() {
  cd "${TC_SRC_LLVM}"
  git-assert-no-changes .
  pull-upstream
  pull-pnacl

  local revs=$(git rev-list --reverse ${UPSTREAM_BRANCH} ^${PNACL_BRANCH})
  for rev in ${revs}; do
    if git-merge ${rev}; then
      git merge --abort
    else
      echo "First fail at rev ${rev}"
      return 0
    fi
  done
}

#@ get-clang-rev   - Find last clang rev before the given LLVM rev
get-clang-rev() {
  local rev=$1
  spushd "${TC_SRC_LLVM}"
  local timestamp=$(git rev-list --timestamp --max-count=1 ${rev} \
    | cut --fields=1 --delimiter=' ')
  cd "${TC_SRC_CLANG}"
  git fetch -q origin
  git rev-list --before=${timestamp} --max-count=1 origin/master
  spopd
}

#@ clean  - reset the source tree to a known state
clean() {
  spushd "${TC_SRC_LLVM}"
  git reset --hard HEAD
  git checkout ${PNACL_BRANCH}
  spopd
}

#@ help                  - Usage information.
help() {
  Usage
}

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

"$@"
