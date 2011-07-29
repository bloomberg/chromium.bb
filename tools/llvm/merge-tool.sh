#!/bin/bash
# Copyright 2011 The Native Client Authors.  All rights reserved.
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
######################################################################

# Location of the mercurial repositories
# These should match the values in utman.sh
readonly TC_SRC="$(pwd)/hg"
readonly TC_SRC_UPSTREAM="${TC_SRC}/upstream"

readonly PREDIFF="${TC_SRC}/prediff"
readonly POSTDIFF="${TC_SRC}/postdiff"


# These variables should be set to the directories containing svn checkouts
# of the upstream LLVM and LLVM-GCC repositories, respectively. These variables
# must be specified in the environment to this script.
readonly MASTER_LLVM=${MASTER_LLVM:-}
readonly MASTER_LLVM_GCC=${MASTER_LLVM_GCC:-}

# TODO(pdox): Refactor repository checkout into a separate script
#             so that we don't need to invoke utman.
utman() {
  UTMAN_UPSTREAM=true "${NACL_ROOT}"/tools/llvm/utman.sh "$@"
}
hg-checkout-upstream() { utman hg-checkout-upstream "$@" ; }

check-svn-repos() {
  if [ -z "${MASTER_LLVM}" ] ||
     [ -z "${MASTER_LLVM_GCC}" ]; then
    Fatal "You must set environmental variables MASTER_LLVM and MASTER_LLVM_GCC"
  fi
  MERGE_REVISION=$(get-merge-revision)
  Banner "MERGE REVISION: ${MERGE_REVISION}"
}

#@ auto                  - Non-interactive merge
auto() {
  INTERACTIVE_MERGE=false
  export DISPLAY=""
  merge-all
}

#@ manual                - Interactive merge
manual() {
  INTERACTIVE_MERGE=true
  merge-all
}

#+ merge-all             - Merge all the things
merge-all() {
  hg-checkout-upstream
  assert-clean
  setup-hgrc

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
  echo "When you are confident all tests are passing, you can commit and push"
  echo "the merged working directory with:"
  echo "  tools/llvm/merge-tool.sh final-commit"
  echo "********************************************************************"
}

setup-hgrc() {
  cp "${NACL_ROOT}/tools/llvm/hgrc" \
     "${TC_SRC_UPSTREAM}/.hg/hgrc"
}

assert-clean() {
  svn-assert-no-changes "${MASTER_LLVM}"
  svn-assert-no-changes "${MASTER_LLVM_GCC}"

  hg-assert-no-changes "${TC_SRC_UPSTREAM}"
  hg-assert-no-outgoing "${TC_SRC_UPSTREAM}"
}

#@ clean                 - Clean/revert mercurial repositories
clean() {
  StepBanner "CLEAN - Cleaning repositories"
  clean-upstream
}

#+ clean-upstream
clean-upstream() {
  StepBanner "CLEAN" "Cleaning hg upstream repository"
  rm -rf "${TC_SRC_UPSTREAM}"
}

#+ get-merge-revision    - Get the current SVN revision
get-merge-revision() {
  local llvm_rev=$(svn-get-revision "${MASTER_LLVM}")
  local llvm_gcc_rev=$(svn-get-revision "${MASTER_LLVM_GCC}")

  if [ "${llvm_rev}" -ne "${llvm_gcc_rev}" ]; then
    echo -n "Error: Unexpected mismatch between " 1>&2
    echo    "SVN revisions of llvm and llvm-gcc" 1>&2
    exit -1
  fi
  echo "${llvm_rev}"
}

#+ generate-pre-diff     - Generate vendor:pnacl-sfi diff prior to merge
generate-pre-diff() {
  spushd "${TC_SRC_UPSTREAM}"
  hg diff -r vendor:pnacl-sfi &> "${PREDIFF}"
  spopd
}

#+ generate-post-diff    - Generate vendor:pnacl-sfi diff after merge
generate-post-diff() {
  spushd "${TC_SRC_UPSTREAM}"
  hg diff -r vendor &> "${POSTDIFF}"
  spopd
}

#@ commit-vendor         - Apply new commit to vendor branch
commit-vendor() {
  local stepid="commit-vendor"
  StepBanner "Committing vendor"

  StepBanner "${stepid}" "Switching to hg vendor branch"

  hg-update "${TC_SRC_UPSTREAM}" vendor

  StepBanner "${stepid}" "Exporting svn to hg"
  rm -rf "${TC_SRC_UPSTREAM}/llvm"
  svn export "${MASTER_LLVM}" "${TC_SRC_UPSTREAM}/llvm"

  rm -rf "${TC_SRC_UPSTREAM}/llvm-gcc"
  svn export "${MASTER_LLVM_GCC}" "${TC_SRC_UPSTREAM}/llvm-gcc"

  StepBanner "${stepid}" "Updating hg file list"
  spushd "${TC_SRC_UPSTREAM}"
  hg add
  hg remove -A
  spopd

  StepBanner "${stepid}" "Committing vendor branch"
  hg-commit "${TC_SRC_UPSTREAM}" "Updating vendor to r${MERGE_REVISION}"
}

#@ hg-merge              - Merge and resolve conflicts for llvm
hg-merge() {
  StepBanner "hg-merge - Merge and resolve conflicts"

  StepBanner "hg-merge" "Switching to pnacl-sfi branch"
  hg-update "${TC_SRC_UPSTREAM}" pnacl-sfi
  hg-assert-no-changes "${TC_SRC_UPSTREAM}"

  StepBanner "hg-merge" "Merging vendor into pnacl-sfi"
  spushd "${TC_SRC_UPSTREAM}"
  hg merge -r vendor
  spopd
}

#@ vim-diff-diff         - Review diff-diff using vim
vim-diff-diff() {
  vimdiff "${PREDIFF}" "${POSTDIFF}"
}

#@ dump-diff-diff        - Review diff-diff
dump-diff-diff() {
  diff "${PREDIFF}" "${POSTDIFF}"
}

final-commit() {
  StepBanner "final-commit" "Committing and pushing merge"
  hg-assert-is-merge "${TC_SRC_UPSTREAM}"
  hg-assert-branch "${TC_SRC_UPSTREAM}" pnacl-sfi

  if ${INTERACTIVE_MERGE}; then
    Banner "CAUTION: This step will COMMIT and PUSH changes to the repository."
    echo
    if ! confirm-yes "Is the merged working directory passing all tests?" ; then
      echo "Cancelled"
      exit -1
    fi
    if ! confirm-yes "Are you really sure you want to do this?" ; then
      echo "Cancelled"
      exit -1
    fi
  fi

  StepBanner "final-commit" "Committing pnacl-sfi branch"
  hg-commit "${TC_SRC_UPSTREAM}" "Merged up to r${MERGE_REVISION}"

  StepBanner "final-commit" "Pushing to hg/upstream"
  hg-push "${TC_SRC_UPSTREAM}"
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

check-svn-repos
"$@"
