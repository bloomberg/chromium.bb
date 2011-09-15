#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
######################################################################
#
# This script facilitates merging changes from the LLVM subversion
# repository into the PNaCl Mercurial repository.
#
######################################################################
set -o nounset
set -o errexit

# Make sure this script is run from the right place
if [[ $(basename $(pwd)) != "native_client" ]] ; then
  echo "ERROR: run this script from the native_client/ dir"
  exit -1
fi

source tools/llvm/common-tools.sh
readonly NACL_ROOT="$(pwd)"
SetScriptPath "${NACL_ROOT}/tools/llvm/merge-tool.sh"
SetLogDirectory "${NACL_ROOT}/toolchain/hg-log"
readonly SCRIPT_PATH="$0"
readonly MERGE_LOG_FILE="${NACL_ROOT}/merge.log"
######################################################################

# Location of the sources
# These should match the values in utman.sh
readonly TC_SRC="$(pwd)/hg"
readonly TC_SRC_UPSTREAM="${TC_SRC}/upstream"
readonly TC_SRC_LLVM_MASTER="${TC_SRC}/llvm-master"
readonly TC_SRC_LLVM_GCC_MASTER="${TC_SRC}/llvm-gcc-master"

readonly PREDIFF="${TC_SRC}/prediff"
readonly POSTDIFF="${TC_SRC}/postdiff"

readonly UPSTREAM_BRANCH=pnacl-sfi

readonly HG_CONFIG_AUTO=(--config ui.merge=internal:merge
                         --config ui.username=chromebot1@gmail.com)

readonly HG_CONFIG_MANUAL=(
  --config kdiff3.executable=/usr/bin/diff3
  --config kdiff3.args="--auto --base \$base \$local \$other -o \$output")

# TODO(pdox): Refactor repository checkout into a separate script
#             so that we don't need to invoke utman.
utman() {
  "${NACL_ROOT}"/tools/llvm/utman.sh "$@"
}

#@ auto [rev]            - Non-interactive merge
auto() {
  INTERACTIVE_MERGE=false
  export DISPLAY=""
  HG_CONFIG=("${HG_CONFIG_AUTO[@]}")
  merge-all "$@"
}

#@ manual [rev]          - Interactive merge
manual() {
  INTERACTIVE_MERGE=true
  HG_CONFIG=("${HG_CONFIG_MANUAL[@]}")
  merge-all "$@"
}

get-tip-revision() {
  svn info "http://llvm.org/svn/llvm-project/" \
    | grep Revision \
    | awk '{print $2}'
}

set-master-revision() {
  echo "@@@BUILD_STEP Set LLVM revision: ${MERGE_REVISION}@@@"
  echo "MERGE REVISION: ${MERGE_REVISION}"

  # Set environmental variable for utman
  export LLVM_PROJECT_REV=${MERGE_REVISION}
  utman svn-checkout-llvm-master
  utman svn-update-llvm-master
  utman svn-checkout-llvm-gcc-master
  utman svn-update-llvm-gcc-master

  check-revisions
}

get-upstream() {
  echo "@@@BUILD_STEP Get mercurial source@@@"
  export UPSTREAM_REV=${UPSTREAM_BRANCH}
  utman hg-checkout-upstream
  utman hg-pull-upstream
  utman hg-update-upstream
}

#+ merge-all             - Merge everything
merge-all() {
  if [ $# -ne 1 ]; then
    Fatal "Please specify revision"
  fi
  MERGE_REVISION=$1

  set-master-revision
  get-upstream

  assert-clean

  generate-pre-diff

  commit-vendor
  hg-merge

  generate-post-diff

  if ${INTERACTIVE_MERGE}; then
    vim-diff-diff
  else
    dump-diff-diff
  fi

  echo "********************************************************************"
  echo "The llvm and llvm-gcc working directories are now in a merged state."
  echo "Before you commit and push, you should build PNaCl and run all tests."
  echo ""
  echo "Expect lots of bugs. You may need to fix and rebuild several times."
  echo "When you are confident all tests are passing, you can commit and push."
  echo "********************************************************************"
}

assert-clean() {
  svn-assert-no-changes "${TC_SRC_LLVM_MASTER}"
  svn-assert-no-changes "${TC_SRC_LLVM_GCC_MASTER}"

  hg-assert-no-changes "${TC_SRC_UPSTREAM}"
  hg-assert-no-outgoing "${TC_SRC_UPSTREAM}"
}

#@ clean                 - Clean/revert mercurial repositories
clean() {
  StepBanner "CLEAN - Cleaning repositories"
  clean-upstream
}

#+ clean-upstream       - Clean the hg/upstream repository
clean-upstream() {
  StepBanner "CLEAN" "Cleaning hg upstream repository"

  if ! [ -d "${TC_SRC_UPSTREAM}" ]; then
    return 0
  fi

  # This is kind of silly, but a lot faster than
  # checking out the repository from scratch again.
  spushd "${TC_SRC_UPSTREAM}"
  if hg-has-outgoing "${TC_SRC_UPSTREAM}"; then
    hg rollback
  fi
  rm -rf llvm
  rm -rf llvm-gcc
  hg update -C ${UPSTREAM_BRANCH}
  spopd

  hg-assert-no-outgoing "${TC_SRC_UPSTREAM}"
  hg-assert-no-changes "${TC_SRC_UPSTREAM}"
}

#+ check-revisions       - Make sure the repostiory revision is set correctly.
check-revisions() {
  local llvm_rev=$(svn-get-revision "${TC_SRC_LLVM_MASTER}")
  local llvm_gcc_rev=$(svn-get-revision "${TC_SRC_LLVM_GCC_MASTER}")
  if [ "${llvm_rev}" -ne "${MERGE_REVISION}" ]; then
    Fatal "llvm-master revision does not match"
  fi
  if [ "${llvm_gcc_rev}" -ne "${MERGE_REVISION}" ]; then
    Fatal "llvm-gcc-master revision does not match"
  fi
}

#+ generate-pre-diff     - Generate vendor:pnacl diff prior to merge
generate-pre-diff() {
  spushd "${TC_SRC_UPSTREAM}"
  hg diff -r vendor:${UPSTREAM_BRANCH} &> "${PREDIFF}"
  spopd
}

#+ generate-post-diff    - Generate vendor:pnacl diff after merge
generate-post-diff() {
  spushd "${TC_SRC_UPSTREAM}"
  hg diff -r vendor &> "${POSTDIFF}"
  spopd
}

#@ commit-vendor         - Apply new commit to vendor branch
commit-vendor() {
  StepBanner "Committing vendor"

  StepBanner "Switching to hg vendor branch"

  hg-update "${TC_SRC_UPSTREAM}" vendor

  StepBanner "Exporting svn to hg"
  rm -rf "${TC_SRC_UPSTREAM}/llvm"
  svn export "${TC_SRC_LLVM_MASTER}" "${TC_SRC_UPSTREAM}/llvm"

  rm -rf "${TC_SRC_UPSTREAM}/llvm-gcc"
  svn export "${TC_SRC_LLVM_GCC_MASTER}" "${TC_SRC_UPSTREAM}/llvm-gcc"

  StepBanner "Updating hg file list"
  spushd "${TC_SRC_UPSTREAM}"
  hg add
  hg remove -A 2>&1 | (grep -v "not removing" || true)
  spopd

  if ! hg-has-changes "${TC_SRC_UPSTREAM}" ; then
    Banner "Trivial merge, no changes recorded"
    exit 55
  fi

  StepBanner "Committing vendor branch"
  spushd "${TC_SRC_UPSTREAM}"
  hg "${HG_CONFIG[@]}" commit -m "Updating vendor to r${MERGE_REVISION}"
  spopd
}

#@ hg-merge              - Merge and resolve conflicts for llvm
hg-merge() {
  StepBanner "hg-merge - Merge and resolve conflicts"

  StepBanner "hg-merge" "Switching to ${UPSTREAM_BRANCH} branch"
  hg-update "${TC_SRC_UPSTREAM}" ${UPSTREAM_BRANCH}
  hg-assert-no-changes "${TC_SRC_UPSTREAM}"

  StepBanner "hg-merge" "Merging vendor into ${UPSTREAM_BRANCH}"
  spushd "${TC_SRC_UPSTREAM}"
  hg "${HG_CONFIG[@]}" -y merge -r vendor 2>&1 | tee "${MERGE_LOG_FILE}"
  local hgret=${PIPESTATUS[0]}
  if [ ${hgret} -ne 0 ] ||
     grep -q "remote deleted\|local deleted\|conflicting flags" \
             "${MERGE_LOG_FILE}" ; then
    Fatal "MERGE FAILED"
  fi
  spopd
}

#@ vim-diff-diff         - Review diff-diff using vim
vim-diff-diff() {
  vimdiff "${PREDIFF}" "${POSTDIFF}"
}

#@ dump-diff-diff        - Review diff-diff
dump-diff-diff() {
  diff "${PREDIFF}" "${POSTDIFF}" || true
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
