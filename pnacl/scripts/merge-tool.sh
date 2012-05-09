#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
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

# The script is located in "pnacl/scripts".
# Set pwd to pnacl/
cd "$(dirname "$0")"/..
if [[ $(basename "$(pwd)") != "pnacl" ]] ; then
  echo "ERROR: cannot find pnacl/ directory"
  exit -1
fi

source scripts/common-tools.sh
readonly PNACL_ROOT="$(pwd)"

SetScriptPath "${PNACL_ROOT}/scripts/merge-tool.sh"
SetLogDirectory "${PNACL_ROOT}/build/log"
readonly SCRIPT_PATH="$0"
readonly MERGE_LOG_FILE="${PNACL_ROOT}/merge.log"
######################################################################

# Location of the sources
# These should match the values in build.sh
readonly TC_SRC="${PNACL_ROOT}/src"
readonly TC_SRC_UPSTREAM="${TC_SRC}/upstream"
readonly TC_SRC_LLVM_MASTER="${TC_SRC}/llvm-master"

readonly PREDIFF="${TC_SRC}/prediff"
readonly POSTDIFF="${TC_SRC}/postdiff"

readonly UPSTREAM_BRANCH=pnacl-sfi

readonly HG_CONFIG_AUTO=(--config ui.merge=internal:merge
                         --config ui.username=chromebot1@gmail.com)

readonly HG_CONFIG_MANUAL=(
  --config kdiff3.executable=/usr/bin/diff3
  --config kdiff3.args="--auto --base \$base \$local \$other -o \$output")

# TODO(pdox): Refactor repository checkout into a separate script
#             so that we don't need to invoke build.sh.
pnacl-build() {
  "${PNACL_ROOT}"/build.sh "$@"
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

  # Set environmental variable for build.sh
  export LLVM_PROJECT_REV=${MERGE_REVISION}
  pnacl-build svn-checkout-llvm-master
  pnacl-build svn-update-llvm-master
}

get-upstream() {
  echo "@@@BUILD_STEP Get mercurial source@@@"
  export UPSTREAM_REV=${UPSTREAM_BRANCH}
  pnacl-build hg-checkout-upstream
  pnacl-build hg-pull-upstream
  pnacl-build hg-update-upstream
}

#+ merge-all             - Merge everything
merge-all() {
  if [ $# -ne 1 ]; then
    Fatal "Please specify revision or 'tip'"
  fi
  if [ "$1" == "tip" ]; then
    MERGE_REVISION=$(get-tip-revision)
  else
    MERGE_REVISION=$1
  fi

  set-master-revision
  get-upstream

  check-revisions

  assert-clean

  echo "@@@BUILD_STEP Merge@@@"
  generate-pre-diff

  commit-vendor
  hg-merge

  generate-post-diff

  if ${INTERACTIVE_MERGE}; then
    vim-diff-diff
  else
    dump-diff-diff
  fi

  final-banner
}

final-banner() {
  echo "********************************************************************"
  echo "The pnacl/src/upstream working directory is now in a merged state."
  echo "Before you commit and push, you should build PNaCl and run all tests."
  echo ""
  echo "1) Set the default LLVM_PROJECT_REV to ${MERGE_REVISION} (in build.sh)"
  echo ""
  echo "2) Build PNaCl:"
  echo "     PNACL_MERGE_TESTING=true pnacl/build.sh everything-translator"
  echo ""
  echo "3) Run all tests:"
  echo "     pnacl/test.sh test-all"
  echo ""
  echo "Depending on the size of the merge, there may be lots of bugs. You may"
  echo "need to fix and rebuild several times. When you are confident all tests"
  echo "are passing, you can commit and push."
  echo "********************************************************************"
}

assert-clean() {
  svn-assert-no-changes "${TC_SRC_LLVM_MASTER}"

  hg-assert-no-changes "${TC_SRC_UPSTREAM}"
  hg-assert-no-outgoing "${TC_SRC_UPSTREAM}"
}

#@ clean                 - Clean/revert mercurial repositories
clean() {
  echo "@@@BUILD_STEP Clean src/upstream@@@"
  StepBanner "CLEAN - Cleaning repositories"
  clean-upstream
}

#+ clean-upstream       - Clean the src/upstream repository
clean-upstream() {
  StepBanner "CLEAN" "Cleaning src/upstream repository"

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
  hg update -C ${UPSTREAM_BRANCH}
  spopd

  hg-assert-no-outgoing "${TC_SRC_UPSTREAM}"
  hg-assert-no-changes "${TC_SRC_UPSTREAM}"
}

#+ check-revisions       - Make sure the repostiory revision is set correctly.
check-revisions() {
  local llvm_rev=$(svn-get-revision "${TC_SRC_LLVM_MASTER}")
  if [ "${llvm_rev}" -ne "${MERGE_REVISION}" ]; then
    Fatal "llvm-master revision does not match"
  fi

  # Make sure MERGE_REVISION >= upstream_vendor_rev
  local upstream_vendor_rev="$(get-upstream-vendor-revision)"
  if [ "${#upstream_vendor_rev}" -lt 6 ] ||
     [ "${#upstream_vendor_rev}" -gt 7 ]; then
    Fatal "Invalid upstream revision '${upstream_vendor_rev}'"
  fi
  if [ "${MERGE_REVISION}" -lt "${upstream_vendor_rev}" ]; then
    Fatal "Trying to merge with an prior revision of LLVM"
  fi
}

get-upstream-vendor-revision() {
  spushd "${TC_SRC_UPSTREAM}"
  # This must match the commit message in commit-vendor()
  hg head vendor |
    grep 'Updating vendor' |
    sed 's/.*Updating vendor to r\([0-9]\+\)$/\1/g'
  spopd
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
  # This commit message must match the regex in get-upstream-vendor-revision()
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
  hg parent
  hg "${HG_CONFIG[@]}" -y merge -r vendor 2>&1 | tee "${MERGE_LOG_FILE}"
  local hgret=${PIPESTATUS[0]}
  if [ ${hgret} -ne 0 ]; then
    echo "==== REMAINING MERGE CONFLICTS (file deletes, etc.) ===="
    grep "remote deleted\|local deleted\|conflicting flags" \
      "${MERGE_LOG_FILE}"
    echo "==== END grep of ${MERGE_LOG_FILE} ===="
    echo "Please handle these file changes manually."
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
