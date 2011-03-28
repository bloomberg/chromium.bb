#!/bin/bash
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

# Turn on/off debugging mode
readonly UTMAN_DEBUG=${UTMAN_DEBUG:-false}

readonly TC_LOG="$(pwd)/toolchain/hg-log"
readonly TC_LOG_ALL="${TC_LOG}/ALL"

readonly TIME_AT_STARTUP=$(date '+%s')

######################################################################
# Mercurial repository tools
######################################################################

#+ hg-pull <dir>
hg-pull() {
  local dir="$1"

  assert-dir "$dir" \
    "Repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  spushd "$dir"
  RunWithLog "hg-pull" hg pull
  spopd
}

hg-checkout() {
  local repo=$1
  local dest=$2
  local rev=$3
  mkdir -p "${TC_SRC}"
  if [ ! -d ${dest} ] ; then
    StepBanner "HG-CHECKOUT" "Checking out new repository for ${repo} @ ${rev}"
    # Use a temporary directory just in case HG has problems
    # with long filenames during checkout.
    local TMPDIR="/tmp/hg-${rev}-$RANDOM"
    hg clone "https://${repo}.googlecode.com/hg/" "${TMPDIR}"
    spushd "${TMPDIR}"
    hg update -C ${rev}
    mv "${TMPDIR}" "${dest}"
    spopd
  else
    StepBanner "HG-CHECKOUT" "Using existing source for ${repo} in ${dest}"
  fi
}

#+ hg-update <dir> [rev]
hg-update() {
  local dir="$1"

  assert-dir "$dir" \
    "HG repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  spushd "${dir}"
  if [ $# == 2 ]; then
    local rev=$2
    RunWithLog "hg-update" hg update ${rev}
  else
    RunWithLog "hg-update" hg update
  fi

  spopd
}

#+ hg-push <dir>
hg-push() {
  local dir="$1"
  spushd "${dir}"
  hg push
  spopd
}

hg-info() {
  local dir="$1"
  local rev="$2"

  spushd "$dir"
  local hg_status=$(hg status -mard)
  if [ ${#hg_status} -gt 0 ]; then
    LOCAL_CHANGES="YES"
  else
    LOCAL_CHANGES="NO"
  fi

  echo ""
  echo "Directory: hg/$(basename ${dir})"
  echo "  Branch         : $(hg branch)"
  echo "  Revision       : $(hg identify)"
  echo "  Local changes  : ${LOCAL_CHANGES}"
  echo "  Stable Revision: ${rev}"
  echo ""
  spopd
}

hg-at-revision() {
  local dir="$1"
  local rev="$2"

  spushd "${dir}"
  local HGREV=($(hg identify | tr -d '+'))
  spopd
  [ "${HGREV[0]}" == "${rev}" ]
  return $?
}

#+ hg-assert-is-merge <dir> - Assert an working directory contains a merge
hg-assert-is-merge() {
  local dir=$1
  spushd "${dir}"

  # When the working directory is a merge, hg identify -i
  # emits "changesetid1+changesetid2+"
  if hg identify -i | egrep -q '^[0-9a-f]+\+[0-9a-f]+\+$'; then
    spopd
    return
  fi
  local REPONAME=$(basename $(pwd))
  spopd
  Banner "ERROR: Working directory of '${REPONAME}' does not have a merge."
  exit -1
}

#+ hg-assert-branch <dir> <branch> - Assert hg repo in <dir> is on <branch>
hg-assert-branch() {
  local dir=$1
  local branch=$2
  spushd "${dir}"
  if hg branch | grep -q "^${branch}\$"; then
    spopd
    return
  fi
  local CURBRANCH=$(hg branch)
  local REPONAME=$(basename $(pwd))
  spopd
  Banner "ERROR: ${REPONAME} is on branch ${CURBRANCH} instead of ${branch}."
  exit -1
}

#+ hg-assert-no-changes <dir> - Assert an hg repo has no local changes
hg-assert-no-changes() {
  local dir=$1
  spushd "${dir}"
  local PLUS=$(hg identify | tr -d -c '+')
  local REPONAME=$(basename $(pwd))
  spopd

  if [ "${PLUS}" != "" ]; then
    Banner "ERROR: Repository ${REPONAME} has local changes"
    exit -1
  fi
}

hg-assert-no-outgoing() {
  local dir=$1
  spushd "${dir}"
  if hg outgoing | grep -q "^no changes found$" ; then
    spopd
    return
  fi
  local REPONAME=$(basename $(pwd))
  spopd
  msg="ERROR: Repository ${REPONAME} has outgoing commits. Clean first."
  Banner "${msg}"
  exit -1
}

#+ hg-commit <dir> <msg>
hg-commit() {
  local dir="$1"
  local msg="$2"
  spushd "${dir}"
  hg commit -m "${msg}"
  spopd
}

######################################################################
# Subversion repository tools
######################################################################

#+ svn-get-revision <dir>
svn-get-revision() {
  local dir="$1"
  spushd "${dir}"
  local rev=$(svn info | grep 'Revision: ' | cut -b 11-)
  if ! [[ "${rev}" =~ ^[0-9]+$ ]]; then
    echo "Invalid revision number '${rev}' or invalid repository '${dir}'" 1>&2
    exit -1
  fi
  spopd
  echo "${rev}"
}

#+ svn-checkout <url> <repodir> - Checkout an SVN repository
svn-checkout() {
  local url="$1"
  local dir="$2"

  if [ ! -d "${dir}" ]; then
    StepBanner "SVN-CHECKOUT" "Checking out ${url}"
    RunWithLog "svn-checkout" svn co "${url}" "${dir}"
  else
    SkipBanner "SVN-CHECKOUT" "Using existing SVN repository for ${url}"
  fi
}

#+ svn-update <dir> <rev> - Update an SVN repository
svn-update() {
  local dir="$1"
  local rev="$2"

  assert-dir "$dir" \
    "SVN repository $(basename "${dir}") doesn't exist."

  spushd "${dir}"
  if [[ "$rev" == "tip" ]]; then
    RunWithLog "svn-update" svn update
  else
    RunWithLog "svn-update" svn update -r ${rev}
  fi
  spopd
}

#+ svn-assert-no-changes <dir> - Assert an svn repo has no local changes
svn-assert-no-changes() {
  local dir=$1
  spushd "${dir}"
  local STATUS=$(svn status)
  local REPONAME=$(basename $(pwd))
  spopd

  if [ "${STATUS}" != "" ]; then
    Banner "ERROR: Repository ${REPONAME} has local changes"
    exit -1
  fi
}

######################################################################
# Logging tools
######################################################################

#@ progress              - Show build progress (open in another window)
progress() {
  StepBanner "PROGRESS WINDOW"

  while true; do
    if [ -f "${TC_LOG_ALL}" ]; then
      if tail --version > /dev/null; then
        # GNU coreutils tail
        tail -s 0.05 --max-unchanged-stats=20 --follow=name "${TC_LOG_ALL}"
      else
        # MacOS tail
        tail -F "${TC_LOG_ALL}"
      fi
    fi
    sleep 1
  done
}

#+ clean-logs            - Clean all logs
clean-logs() {
  rm -rf "${TC_LOG}"
}

# Logged pushd
spushd() {
  LogEcho "-------------------------------------------------------------------"
  LogEcho "ENTERING: $1"
  pushd "$1" > /dev/null
}

# Logged popd
spopd() {
  LogEcho "LEAVING: $(pwd)"
  popd > /dev/null
  LogEcho "-------------------------------------------------------------------"
  LogEcho "ENTERING: $(pwd)"
}

LogEcho() {
  mkdir -p "${TC_LOG}"
  echo "$*" >> "${TC_LOG_ALL}"
}

RunWithLog() {
  local log="${TC_LOG}/$1"

  mkdir -p "${TC_LOG}"

  shift 1
  echo "RUNNING: " "$@" | tee "${log}" >> "${TC_LOG_ALL}"
  "$@" 2>&1 | tee "${log}" >> "${TC_LOG_ALL}"
  if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo
    Banner "ERROR"
    echo -n "COMMAND:"
    PrettyPrint "$@"
    echo
    echo "LOGFILE: ${log}"
    echo
    echo "PWD: $(pwd)"
    echo
    # TODO(pdox): Make this a separate BUILDBOT flag (currently, this is it)
    if ${UTMAN_DEBUG}; then
      echo "BEGIN LOGFILE Contents."
      cat "${log}"
      echo "END LOGFILE Contents."
    fi
    exit -1
  fi
}

PrettyPrint() {
  # Pretty print, respecting parameter grouping
  for I in "$@"; do
    local has_space=$(echo "$I" | grep " ")
    if [ ${#has_space} -gt 0 ]; then
      echo -n ' "'
      echo -n "$I"
      echo -n '"'
    else
      echo -n " $I"
    fi
  done
  echo
}

assert-dir() {
  local dir="$1"
  local msg="$2"

  if [ ! -d "${dir}" ]; then
    Banner "ERROR: ${msg}"
    exit -1
  fi
}

assert-file() {
  local fn="$1"
  local msg="$2"

  if [ ! -f "${fn}" ]; then
    Banner "ERROR: ${fn} does not exist. ${msg}"
    exit -1
  fi
}

Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}

Usage2() {
  egrep "^#(@|\+)" $0 | cut --bytes=3-
}

Banner() {
  echo "**********************************************************************"
  echo "**********************************************************************"
  echo " $@"
  echo "**********************************************************************"
  echo "**********************************************************************"
}

StepBanner() {
  local module="$1"
  if [ $# -eq 1 ]; then
    echo ""
    echo "-------------------------------------------------------------------"
    local padding=$(RepeatStr ' ' 28)
    echo "${padding}${module}"
    echo "-------------------------------------------------------------------"
  else
    shift 1
    local padding=$(RepeatStr ' ' $((20-${#module})) )
    local time_stamp=$(date -d "now - ${TIME_AT_STARTUP}sec" '+%M:%S')
    echo "[${time_stamp}] ${module}${padding}" "$@"
  fi
}


SubBanner() {
  echo "----------------------------------------------------------------------"
  echo " $@"
  echo "----------------------------------------------------------------------"
}

SkipBanner() {
    StepBanner "$1" "Skipping $2, already up to date."
}

RepeatStr() {
  local str="$1"
  local count=$2
  local ret=""

  while [ $count -gt 0 ]; do
    ret="${ret}${str}"
    count=$((count-1))
  done
  echo "$ret"
}


Abort() {
  local module="$1"

  shift 1
  local padding=$(RepeatStr ' ' $((20-${#module})) )
  echo
  echo "${module}${padding} ERROR:" "$@"
  echo
  exit -1
}

PackageCheck() {
  assert-bin "makeinfo" "makeinfo not found. Please install 'texinfo' package."
  assert-bin "bison"    "bison not found. Please install 'bison' package."
  assert-bin "flex"     "flex not found. Please install 'flex' package."
}

assert-bin() {
  local exe="$1"
  local msg="$2"

  if ! which "$exe" > /dev/null; then
    Banner "ERROR: $msg"
    exit -1
  fi
}

is-ELF() {
  local F=$(file "$1")
  [[ "${F}" =~ "ELF" ]]
}

get_dir_size_in_mb() {
  du -msc "$1" | tail -1 | egrep -o "[0-9]+"
}

confirm-yes() {
  local msg="$1"
  while true; do
    echo -n "${msg} [Y/N]? "
    local YESNO
    read YESNO
    if [ "${YESNO}" == "N" ] || [ "${YESNO}" == "n" ]; then
      return 1
    fi
    if [ "${YESNO}" == "Y" ] || [ "${YESNO}" == "y" ]; then
      return 0
    fi
  done
}
