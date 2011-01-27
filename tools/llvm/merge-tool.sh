#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
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
######################################################################

# Location of the mercurial repositories
# These should match the values in utman.sh
readonly TC_SRC="$(pwd)/hg"
readonly TC_SRC_LLVM="${TC_SRC}/llvm"
readonly TC_SRC_LLVM_GCC="${TC_SRC}/llvm-gcc"

# Location of the upstream LLVM repository
readonly MASTER_LLVM_BASE="http://llvm.org/svn/llvm-project"
readonly MASTER_LLVM_URL="${MASTER_LLVM_BASE}/llvm/trunk"
readonly MASTER_LLVM_GCC_URL="${MASTER_LLVM_BASE}/llvm-gcc-4.2/trunk"

readonly MASTER_LLVM="${TC_SRC}/master-llvm"
readonly MASTER_LLVM_GCC="${TC_SRC}/master-llvm-gcc"


BASEDIR="$(pwd)"
utman() {
  pushd "${BASEDIR}" > /dev/null
  tools/llvm/utman.sh "$@"
  popd > /dev/null
}
hg-pull-llvm() { utman hg-pull-llvm "$@" ; }
hg-pull-llvm-gcc() { utman hg-pull-llvm-gcc "$@" ; }
hg-checkout-llvm() { utman hg-checkout-llvm "$@" ; }
hg-checkout-llvm-gcc() { utman hg-checkout-llvm-gcc "$@" ; }

#@ all                   - Do LLVM and LLVM-GCC merge (all steps)
all() {
  assert-clean

  checkout-all
  choose-revision

  update-llvm-gcc-vendor
  merge-llvm-gcc
  diff-diff-llvm-gcc

  update-llvm-vendor
  merge-llvm
  diff-diff-llvm

  echo "********************************************************************"
  echo "The llvm and llvm-gcc working directories are now in a merged state."
  echo "Before you commit and push, you should build PNaCl and run all tests."
  echo "For example:"
  echo "  tools/llvm/utman.sh clean"
  echo "  tools/llvm/utman.sh all"
  echo "  tools/llvm/utman.sh test-all"
  echo "  tests/spec2k/bot_spec.sh 2 <spec-dir>"
  echo "  tests/spec2k/bot_spec.sh 3 <spec-dir>"
  echo ""
  echo "Expect lots of bugs. You may need to fix and rebuild several times."
  echo "When you are confident all tests are passing, you can commit and push"
  echo "the merged working directories with:"
  echo "  tools/llvm/merge-tool.sh final-commit"
  echo "********************************************************************"
}

assert-clean() {
  if [ -d "${MASTER_LLVM}" ]; then
    svn-assert-no-changes "${MASTER_LLVM}"
  fi

  if [ -d "${MASTER_LLVM_GCC}" ]; then
    svn-assert-no-changes "${MASTER_LLVM_GCC}"
  fi

  if [ -d "${TC_SRC_LLVM}" ]; then
    hg-assert-no-changes "${TC_SRC_LLVM}"
    hg-assert-no-outgoing "${TC_SRC_LLVM}"
  fi

  if [ -d "${TC_SRC_LLVM_GCC}" ]; then
    hg-assert-no-changes "${TC_SRC_LLVM_GCC}"
    hg-assert-no-outgoing "${TC_SRC_LLVM_GCC}"
  fi
}

#@ clean                 - Clean/revert mercurial repositories
clean() {
  StepBanner "CLEAN - Cleaning repositories"
  Banner "WARNING: All local changes to hg/llvm and hg/llvm-gcc will be erased"
  if ! confirm-yes "Are you sure you want to do this?" ; then
    echo "Cancelled"
    exit -1
  fi
  if ! confirm-yes "Are you really, really sure you want do this?" ; then
    echo "Cancelled"
    exit -1
  fi
  clean-llvm
  clean-llvm-gcc
}

#+ clean-llvm
clean-llvm() {
  StepBanner "CLEAN" "Cleaning hg llvm repository"
  rm -rf "${TC_SRC_LLVM}"
}

#+ clean-llvm-gcc
clean-llvm-gcc() {
  StepBanner "CLEAN" "Cleaning hg llvm-gcc repository"
  rm -rf "${TC_SRC_LLVM_GCC}"
}

#@ checkout-all          - Checkout repositories
checkout-all() {
  StepBanner "checkout-all - Checkout all repositories"

  # Checkout LLVM repositories
  StepBanner "checkout-all" "Checking out SVN repositories"
  svn-checkout "${MASTER_LLVM_URL}" "${MASTER_LLVM}"
  svn-checkout "${MASTER_LLVM_GCC_URL}" "${MASTER_LLVM_GCC}"

  # Checkout the hg repositories
  StepBanner "checkout-all" "Checking out HG repositories"
  hg-checkout-llvm
  hg-checkout-llvm-gcc
}

#+ get-revision       -  Get the current SVN revision
get-revision() {
  local llvm_rev=$(svn-get-revision "${MASTER_LLVM}")
  local llvm_gcc_rev=$(svn-get-revision "${MASTER_LLVM_GCC}")

  if [ "${llvm_rev}" -ne "${llvm_gcc_rev}" ]; then
    echo -n "Error: Unexpected mismatch between " 1>&2
    echo    "SVN revisions of llvm and llvm-gcc" 1>&2
    exit -1
  fi
  echo "${llvm_rev}"
}

#@ choose-revision       - Choose LLVM revision
choose-revision() {
  StepBanner "choose-revision - Choose LLVM Revision"
  echo
  echo "Go to http://google1.osuosl.org:8011/ for LLVM build status"
  echo
  local rev
  while true; do
    echo -n "Please enter an LLVM revision (or 'tip'): "
    read rev
    if [[ "$rev" == "tip" ]]; then
      break
    fi
    if [[ "$rev" =~ ^[0-9]+$ ]]; then
      break
    fi
    echo "Invalid input."
  done

  # Update the SVN repositories to ${rev}
  StepBanner "choose-revision" "Updating LLVM repository to ${rev}"
  svn-update "${MASTER_LLVM}" "${rev}"
  svn-update "${MASTER_LLVM_GCC}" "${rev}"
}

#@ update-llvm-vendor    - Apply update to vendor branch for llvm
update-llvm-vendor() {
  local fnid="update-llvm-vendor"
  StepBanner "${fnid} - Freshen hg 'vendor' branch"

  StepBanner "${fnid}" "Verifying repository state"
  svn-assert-no-changes "${MASTER_LLVM}"
  hg-assert-no-changes "${TC_SRC_LLVM}"
  hg-assert-no-outgoing "${TC_SRC_LLVM}"

  StepBanner "${fnid}" "hg pull (llvm)"
  hg-pull-llvm

  StepBanner "${fnid}" "switch to hg branch vendor (llvm)"
  hg-update "${TC_SRC_LLVM}" vendor
  StepBanner "${fnid}" "Delete existing vendor source"
  rm -rf "${TC_SRC_LLVM}/llvm-trunk"

  StepBanner "${fnid}" "Exporting svn to hg (llvm)"
  RunWithLog "${fnid}" \
    svn export "${MASTER_LLVM}" "${TC_SRC_LLVM}/llvm-trunk"

  StepBanner "${fnid}" "Updating hg file list (llvm)"
  spushd "${TC_SRC_LLVM}"
  RunWithLog "${fnid}" hg add
  RunWithLog "${fnid}" hg remove -A
  spopd

  hg-status-check LLVM "${TC_SRC_LLVM}"

  local rev=$(get-revision)
  StepBanner "${fnid} - Commit hg 'vendor' branch (llvm)"
  hg-commit "${TC_SRC_LLVM}" "Updating vendor to r${rev}"
}

#@ update-llvm-gcc-vendor - Apply update to vendor branch for llvm-gcc
update-llvm-gcc-vendor() {
  local fnid="update-llvm-gcc-vendor"
  StepBanner "${fnid}" "hg pull (llvm-gcc)"
  hg-pull-llvm-gcc

  StepBanner "${fnid}" "Verifying repository state"
  svn-assert-no-changes "${MASTER_LLVM_GCC}"
  hg-assert-no-changes "${TC_SRC_LLVM_GCC}"
  hg-assert-no-outgoing "${TC_SRC_LLVM_GCC}"

  StepBanner "${fnid}" "switch to hg branch vendor (llvm-gcc)"
  hg-update "${TC_SRC_LLVM_GCC}" vendor

  StepBanner "${fnid}" "Delete existing vendor source"
  rm -rf "${TC_SRC_LLVM_GCC}/llvm-gcc-4.2"

  StepBanner "${fnid}" "Exporting svn to hg (llvm-gcc)"
  RunWithLog "${fnid}" \
    svn export "${MASTER_LLVM_GCC}" "${TC_SRC_LLVM_GCC}/llvm-gcc-4.2"

  StepBanner "${fnid}" "Updating hg file list (llvm-gcc)"
  spushd "${TC_SRC_LLVM_GCC}"
  RunWithLog "${fnid}" hg add
  RunWithLog "${fnid}" hg remove -A
  spopd

  hg-status-check LLVM-GCC "${TC_SRC_LLVM_GCC}"

  local rev=$(get-revision)
  StepBanner "${fnid} - Commit hg 'vendor' branch (llvm-gcc)"
  hg-commit "${TC_SRC_LLVM_GCC}" "Updating vendor to r${rev}"
}

#@ merge-llvm-gcc        - Merge and resolve conflicts for llvm-gcc
merge-llvm-gcc() {
  StepBanner "merge-llvm-gcc - Merge and resolve conflicts"

  StepBanner "merge-llvm-gcc" "Switch to pnacl-sfi branch"
  hg-assert-no-changes "${TC_SRC_LLVM_GCC}"
  hg-update "${TC_SRC_LLVM_GCC}" pnacl-sfi

  StepBanner "merge-llvm-gcc" "Merging vendor into pnacl-sfi"
  spushd "${TC_SRC_LLVM_GCC}"
  hg merge -r vendor
  spopd
}

#@ merge-llvm            - Merge and resolve conflicts for llvm
merge-llvm() {
  StepBanner "merge-llvm - Merge and resolve conflicts"

  StepBanner "merge-llvm" "Switch to pnacl-sfi branch"
  hg-assert-no-changes "${TC_SRC_LLVM}"
  hg-update "${TC_SRC_LLVM}" pnacl-sfi

  StepBanner "merge-llvm" "Merging vendor into pnacl-sfi"
  spushd "${TC_SRC_LLVM}"
  hg merge -r vendor
  spopd
}

#@ diff-diff-llvm        - Review diff-diff
diff-diff-llvm() {
  (
    echo "Type 'q' to exit less"
    echo "---------------------------------------------------"
    tools/llvm/diff-diff.py "${TC_SRC_LLVM}"
  ) 2>&1 | less

  if ! confirm-yes "Does the diff-diff for LLVM look correct"; then
    echo "Cancelling."
    echo "hg repositories remain in uncommitted state."
    echo "Use 'tools/llvm/merge-tool.sh clean' to clean them"
    exit -1
  fi
}

#@ diff-diff-llvm-gcc    - Review diff-diff
diff-diff-llvm-gcc() {
  (
    echo "Type 'q' to exit less"
    echo "---------------------------------------------------"
    tools/llvm/diff-diff.py "${TC_SRC_LLVM_GCC}"
  ) 2>&1 | less

  if ! confirm-yes "Does the diff-diff for LLVM-GCC look correct"; then
    echo "Cancelling."
    echo "hg repositories remain in uncommitted state."
    echo "Use 'tools/llvm/merge-tool.sh clean' to clean them"
    exit -1
  fi
}

final-commit() {
  StepBanner "final-commit" "Committing and pushing merge"
  hg-assert-is-merge "${TC_SRC_LLVM}"
  hg-assert-is-merge "${TC_SRC_LLVM_GCC}"
  hg-assert-branch "${TC_SRC_LLVM}" pnacl-sfi
  hg-assert-branch "${TC_SRC_LLVM_GCC}" pnacl-sfi

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

  local rev=$(get-revision)

  StepBanner "final-commit - Committing hg pnacl-sfi branch (llvm)"
  hg-commit "${TC_SRC_LLVM}" "Merged up to r${rev}"

  StepBanner "final-commit - Committing hg pnacl-sfi branch (llvm-gcc)"
  hg-commit "${TC_SRC_LLVM_GCC}" "Merged up to r${rev}"

  StepBanner "final-commit - Pushing (llvm)"
  hg-push "${TC_SRC_LLVM}"

  StepBanner "final-commit - Pushing (llvm-gcc)"
  hg-push "${TC_SRC_LLVM_GCC}"
}

#+ hg-status-check <name> <repo>   - Allow the user to check hg status for problems
hg-status-check() {
  local name="$1"
  local repo="$2"
  spushd "${repo}"
  (
    echo "Please verify hg status for ${name}:"
    echo "Hit q to exit 'less'"
    echo "---------------------------------------------------"
    hg status
    echo "---------------------------------------------------"
  ) 2>&1 | less
  spopd

  if ! confirm-yes "Does the status for ${name} look correct"; then
    echo "Cancelling vendor update."
    echo "hg repositories remain in uncommitted state."
    echo "Use 'tools/llvm/merge-tool.sh clean' to clean them"
    exit -1
  fi
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
