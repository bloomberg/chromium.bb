#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

# Turn on/off debugging mode
readonly PNACL_DEBUG=${PNACL_DEBUG:-false}

# True if the scripts are running on the build bots.
readonly PNACL_BUILDBOT=${PNACL_BUILDBOT:-false}

# Dump all build output to stdout
readonly PNACL_VERBOSE=${PNACL_VERBOSE:-false}

# Mercurial Retry settings
HG_MAX_RETRIES=${HG_MAX_RETRIES:-3}
if ${PNACL_BUILDBOT} ; then
  HG_RETRY_DELAY_SEC=${HG_RETRY_DELAY_SEC:-60}
else
  HG_RETRY_DELAY_SEC=${HG_RETRY_DELAY_SEC:-1}
fi

readonly TIME_AT_STARTUP=$(date '+%s')

SetScriptPath() {
  SCRIPT_PATH="$1"
}

SetLogDirectory() {
  TC_LOG="$1"
  TC_LOG_ALL="${TC_LOG}/ALL"
}

######################################################################
# Detect system type
######################################################################

BUILD_PLATFORM=$(uname | tr '[A-Z]' '[a-z]')
BUILD_PLATFORM_LINUX=false
BUILD_PLATFORM_MAC=false
BUILD_PLATFORM_WIN=false

if [ "${BUILD_PLATFORM}" == "linux" ] ; then
  BUILD_PLATFORM_LINUX=true
  SCONS_BUILD_PLATFORM=linux
  BUILD_ARCH=${BUILD_ARCH:-$(uname -m)}
  EXEC_EXT=
  SO_PREFIX=lib
  SO_EXT=.so
  SO_DIR=lib
elif [[ "${BUILD_PLATFORM}" =~ cygwin_nt ]]; then
  BUILD_PLATFORM=win
  BUILD_PLATFORM_WIN=true
  SCONS_BUILD_PLATFORM=win
  # force 32 bit host because build is also 32 bit on windows.
  HOST_ARCH=${HOST_ARCH:-x86_32}
  BUILD_ARCH=${BUILD_ARCH:-x86_32}
  EXEC_EXT=.exe
  SO_PREFIX=cyg
  SO_EXT=.dll
  SO_DIR=bin  # On Windows, DLLs are placed in bin/
              # because the dynamic loader searches %PATH%
elif [ "${BUILD_PLATFORM}" == "darwin" ] ; then
  BUILD_PLATFORM=mac
  BUILD_PLATFORM_MAC=true
  SCONS_BUILD_PLATFORM=mac
  # On mac, uname -m is a lie. We always do 64 bit host builds, on
  # 64 bit build machines
  HOST_ARCH=${HOST_ARCH:-x86_64}
  BUILD_ARCH=${BUILD_ARCH:-x86_64}
  EXEC_EXT=
  SO_PREFIX=lib
  SO_EXT=.dylib
  SO_DIR=lib
else
  echo "Unknown system '${BUILD_PLATFORM}'"
  exit -1
fi

readonly BUILD_PLATFORM
readonly BUILD_PLATFORM_LINUX
readonly BUILD_PLATFORM_MAC
readonly BUILD_PLATFORM_WIN
readonly SCONS_BUILD_PLATFORM
readonly SO_PREFIX
readonly SO_EXT
readonly SO_DIR

BUILD_ARCH_X8632=false
BUILD_ARCH_X8664=false
BUILD_ARCH_ARM=false
BUILD_ARCH_MIPS=false
if [ "${BUILD_ARCH}" == "x86_32" ] ||
   [ "${BUILD_ARCH}" == "i386" ] ||
   [ "${BUILD_ARCH}" == "i686" ] ; then
  BUILD_ARCH=x86_32
  BUILD_ARCH_X8632=true
elif [ "${BUILD_ARCH}" == "x86_64" ] ; then
  BUILD_ARCH_X8664=true
elif [ "${BUILD_ARCH}" == "armv7l" ] ; then
  BUILD_ARCH_ARM=true
elif [ "${BUILD_ARCH}" == "mips32" ] ||
     [ "${BUILD_ARCH}" == "mips" ] ; then
  BUILD_ARCH_MIPS=true
else
  echo "Unknown build arch '${BUILD_ARCH}'"
  exit -1
fi
readonly BUILD_ARCH
readonly BUILD_ARCH_X8632
readonly BUILD_ARCH_X8664
readonly BUILD_ARCH_ARM
readonly BUILD_ARCH_MIPS


HOST_ARCH=${HOST_ARCH:-${BUILD_ARCH}}
HOST_ARCH_X8632=false
HOST_ARCH_X8664=false
HOST_ARCH_ARM=false
HOST_ARCH_MIPS=false
if [ "${HOST_ARCH}" == "x86_32" ] ||
   [ "${HOST_ARCH}" == "i386" ] ||
   [ "${HOST_ARCH}" == "i686" ] ; then
  HOST_ARCH=x86_32
  HOST_ARCH_X8632=true
elif [ "${HOST_ARCH}" == "x86_64" ] ; then
  HOST_ARCH_X8664=true
elif [ "${HOST_ARCH}" == "armv7l" ] ; then
  HOST_ARCH_ARM=true
elif [ "${HOST_ARCH}" == "mips32" ] ||
     [ "${HOST_ARCH}" == "mips" ] ; then
  HOST_ARCH_MIPS=true
else
  echo "Unknown host arch '${HOST_ARCH}'"
  exit -1
fi
readonly HOST_ARCH
readonly HOST_ARCH_X8632
readonly HOST_ARCH_X8664
readonly HOST_ARCH_ARM
readonly HOST_ARCH_MIPS

if [ "${BUILD_ARCH}" != "${HOST_ARCH}" ]; then
  if ! { ${BUILD_ARCH_X8664} && ${HOST_ARCH_X8632}; }; then
    echo "Cross builds other than build=x86_64 with host=x86_32 not supported"
    exit -1
  fi
fi

if ${BUILD_PLATFORM_WIN}; then
   # TODO(robertm): switch this to svn.bat, hg.bat, git.bat,  gclient.bat
   readonly GCLIENT="gclient"
   readonly GIT="git"
   readonly HG="hg"
   readonly SVN="svn"
else
   readonly GCLIENT="gclient"
   readonly GIT="git"
   readonly HG="hg"
   readonly SVN="svn"
fi

# On Windows, scons expects Windows-style paths (C:\foo\bar)
# This function converts cygwin posix paths to Windows-style paths.
# On all other platforms, this function does nothing to the path.
PosixToSysPath() {
  local path="$1"
  if ${BUILD_PLATFORM_WIN}; then
    cygpath -w "$(GetAbsolutePath "${path}")"
  else
    echo "${path}"
  fi
}

######################################################################
# Mercurial repository tools
######################################################################

#+ hg-pull <dir>
hg-pull() {
  local dir="$1"

  assert-dir "$dir" \
    "Repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  RunWithRetry ${HG_MAX_RETRIES} ${HG_RETRY_DELAY_SEC} \
    hg-pull-try "${dir}"
}

hg-pull-try() {
  local dir="$1"
  local retcode=0

  spushd "${dir}"
  RunWithLog "hg-pull" ${HG} pull || retcode=$?
  spopd
  return ${retcode}
}

#+ hg-revert <dir>
hg-revert() {
  local dir="$1"
  ${HG} revert "${dir}"
}

#+ hg-clone <url> <dir>
hg-clone() {
  local url="$1"
  local dir="$2"
  RunWithRetry ${HG_MAX_RETRIES} ${HG_RETRY_DELAY_SEC} \
    hg-clone-try "${url}" "${dir}"
}

hg-clone-try() {
  local url="$1"
  local dir="$2"
  local retcode=0

  rm -rf "${dir}"
  ${HG} clone "${url}" "${dir}" || retcode=$?

  if [ ${retcode} -ne 0 ] ; then
   # Clean up directory after failure
   rm -rf "${dir}"
  fi
  return ${retcode}
}

hg-checkout() {
  local repo=$1
  local dest=$2
  local rev=$3

  if [ ! -d ${dest} ] ; then
    local repobasedir=$(dirname "${dest}")
    mkdir -p "${repobasedir}"
    StepBanner "HG-CHECKOUT" "Checking out new repository for ${repo} @ ${rev}"
    # Use a temporary directory just in case HG has problems
    # with long filenames during checkout, and to make sure the
    # repo directory only exists if the checkout was successful.
    local TMPDIR="/tmp/hg-${rev}-$RANDOM"
    hg-clone "https://code.google.com/p/${repo}" "${TMPDIR}"
    hg-update "${TMPDIR}" -C ${rev}
    mv "${TMPDIR}" "${dest}"
  else
    StepBanner "HG-CHECKOUT" "Using existing source for ${repo} in ${dest}"
  fi
}

#+ hg-update <dir> [extra_flags] [rev]
hg-update() {
  local dir="$1"
  shift 1

  assert-dir "${dir}" \
    "HG repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  RunWithRetry ${HG_MAX_RETRIES} ${HG_RETRY_DELAY_SEC} \
    hg-update-try "${dir}" "$@"
}

hg-update-try() {
  local dir="$1"
  shift 1
  local retcode=0
  spushd "${dir}"
  RunWithLog "hg-update" ${HG} update "$@" || retcode=$?
  spopd
  return ${retcode}
}

#+ hg-push <dir>
hg-push() {
  local dir="$1"
  spushd "${dir}"
  ${HG} push
  spopd
}

hg-info() {
  local dir="$1"
  local rev="$2"

  spushd "$dir"
  local hg_status=$(${HG} status -mard)
  if [ ${#hg_status} -gt 0 ]; then
    LOCAL_CHANGES="YES"
  else
    LOCAL_CHANGES="NO"
  fi

  echo ""
  echo "Directory: hg/$(basename ${dir})"
  echo "  Branch         : $(${HG} branch)"
  echo "  Revision       : $(${HG} identify)"
  echo "  Local changes  : ${LOCAL_CHANGES}"
  echo "  Stable Revision: ${rev}"
  echo ""
  spopd
}

svn-at-revision() {
  local dir="$1"
  local rev="$2"
  local repo_rev=$(svn-get-revision "${dir}")
  [ "${repo_rev}" == "${rev}" ]
  return $?
}

hg-at-revision() {
  local dir="$1"
  local rev="$2"
  local repo_rev=$(hg-get-revision "${dir}")
  [ "${repo_rev}" == "${rev}" ]
  return $?
}

#+ hg-assert-is-merge <dir> - Assert an working directory contains a merge
hg-assert-is-merge() {
  local dir=$1
  spushd "${dir}"

  # When the working directory is a merge, hg identify -i
  # emits "changesetid1+changesetid2+"
  if ${HG} identify -i | egrep -q '^[0-9a-f]+\+[0-9a-f]+\+$'; then
    spopd
    return
  fi
  local REPONAME=$(basename "${dir}")
  spopd
  Banner "ERROR: Working directory of '${REPONAME}' does not have a merge."
  exit -1
}

hg-on-branch() {
  local dir=$1
  local branch=$2
  spushd "${dir}"
  if ${HG} branch | grep -q "^${branch}\$"; then
    spopd
    return 0
  else
    spopd
    return 1
  fi
}

#+ hg-assert-branch <dir> <branch> - Assert hg repo in <dir> is on <branch>
hg-assert-branch() {
  local dir=$1
  local branch=$2

  if ! hg-on-branch "${dir}" "${branch}" ; then
    local REPONAME=$(basename "${dir}")
    Banner "ERROR: ${REPONAME} is not on branch '${branch}'."
    exit -1
  fi
}

hg-has-changes() {
  local dir=$1
  spushd "${dir}"
  local PLUS=$(${HG} status . | grep -v '^?')
  spopd

  [ "${PLUS}" != "" ]
  return $?
}

#+ hg-assert-no-changes <dir> - Assert an hg repo has no local changes
hg-assert-no-changes() {
  local dir="$1"
  if hg-has-changes "${dir}" ; then
    local REPONAME=$(basename "${dir}")
    Banner "ERROR: Repository ${REPONAME} has local changes"
    exit -1
  fi
}

hg-has-untracked() {
  local dir=$1
  spushd "${dir}"
  local STATUS=$(${HG} status . | grep '^?')
  spopd

  [ "${STATUS}" != "" ]
  return $?
}

hg-assert-no-untracked() {
  local dir=$1
  if hg-has-untracked "${dir}"; then
    local REPONAME=$(basename "${dir}")
    Banner "ERROR: Repository ${REPONAME} has untracked files"
    exit -1
  fi
}

hg-has-outgoing() {
  local dir=$1
  spushd "${dir}"
  if ${HG} outgoing | grep -q "^no changes found$" ; then
    spopd
    return 1
  fi
  spopd
  return 0
}

hg-assert-no-outgoing() {
  local dir=$1
  if hg-has-outgoing "${dir}" ; then
    local REPONAME=$(basename "${dir}")
    msg="ERROR: Repository ${REPONAME} has outgoing commits. Clean first."
    Banner "${msg}"
    exit -1
  fi
}

######################################################################
# Git repository tools
######################################################################

git-has-changes() {
  local dir=$1
  spushd "${dir}"
  local status=$(${GIT} status --porcelain --untracked-files=no)
  spopd
  [[ ${#status} > 0 ]]
  return $?
}

git-assert-no-changes() {
  local dir=$1
  if git-has-changes "${dir}"; then
    Banner "ERROR: Repository ${dir} has local changes"
    exit -1
  fi
}

######################################################################
# Subversion repository tools
######################################################################

#+ svn-get-revision <dir>
svn-get-revision() {
  local dir="$1"
  spushd "${dir}"
  local rev=$(${SVN} info | grep 'Revision: ' | cut -b 11-)
  if ! [[ "${rev}" =~ ^[0-9]+$ ]]; then
    echo "Invalid revision number '${rev}' or invalid repository '${dir}'" 1>&2
    exit -1
  fi
  spopd
  echo "${rev}"
}

#+ hg-get-revision <dir>
hg-get-revision() {
  local dir="$1"
  spushd "${dir}"
  local HGREV=($(${HG} identify | tr -d '+'))
  spopd
  echo "${HGREV[0]}"
}

#+ svn-checkout <url> <repodir> - Checkout an SVN repository
svn-checkout() {
  local url="$1"
  local dir="$2"
  local rev="$3"

  if [ ! -d "${dir}" ]; then
    StepBanner "SVN-CHECKOUT" "Checking out ${url}"
    RunWithLog "svn-checkout" ${SVN} co "${url}" "${dir}" -r "${rev}"
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
    RunWithLog "svn-update" ${SVN} update
  else
    RunWithLog "svn-update" ${SVN} update -r ${rev}
  fi
  spopd
}

svn-has-changes() {
  local dir="$1"
  spushd "${dir}"
  local STATUS=$(${SVN} status)
  spopd
  [ "${STATUS}" != "" ]
  return $?
}

#+ svn-assert-no-changes <dir> - Assert an svn repo has no local changes
svn-assert-no-changes() {
  local dir="$1"
  if svn-has-changes "${dir}" ; then
    local name=$(basename "${dir}")
    Banner "ERROR: Repository ${name} has local changes"
    exit -1
  fi
}


######################################################################
# vcs rev info helper
######################################################################
get-field () {
  cut -d " " -f $1
}

git-one-line-rev-info() {
    local commit=$(${GIT} log -n 1 | head -1 | get-field 2)
    local url=$(egrep "^[^a-z]+url = " .git/config | head -1 | get-field 3)
    # variable bypass does implicit whitespace strip
    echo "[GIT] ${url}: ${commit}"
}

svn-one-line-rev-info() {
    local url=$(${SVN} info | egrep 'URL:' | get-field 2)
    local rev=$(${SVN} info | egrep 'Revision:' | get-field 2)
    echo "[SVN] ${url}: ${rev}"
}

hg-one-line-rev-info() {
    local num=$(${HG} identify -n)
    local id=$(${HG} identify -i)
    local url=$(grep default .hg/hgrc | get-field 3)
    echo "[HG]  ${url}: ${id} (${num})"
}

#+ one-line-rev-info <dir> - show one line summmary for
one-line-rev-info() {
  spushd $1
  if [ -d .svn ]; then
    svn-one-line-rev-info
  elif [ -d .hg ]; then
    hg-one-line-rev-info
  elif [ -d .git ]; then
    # we currently only
    git-one-line-rev-info
  else
    echo "[$1] Unknown version control system"
  fi
  spopd
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
  local ret=1
  if ${PNACL_VERBOSE}; then
    echo "RUNNING: " "$@" | tee -a "${log}" "${TC_LOG_ALL}"
    "$@" 2>&1 | tee -a "${log}" "${TC_LOG_ALL}"
    ret=${PIPESTATUS[0]}
  else
    echo "RUNNING: " "$@" | tee -a "${log}" "${TC_LOG_ALL}" &> /dev/null
    "$@" 2>&1 | tee -a "${log}" "${TC_LOG_ALL}" &> /dev/null
    ret=${PIPESTATUS[0]}
  fi
  if [ ${ret} -ne 0 ]; then
    echo
    Banner "ERROR"
    echo -n "COMMAND:"
    PrettyPrint "$@"
    echo
    echo "LOGFILE: ${log}"
    echo
    echo "PWD: $(pwd)"
    echo
    if ${PNACL_BUILDBOT}; then
      echo "BEGIN LOGFILE Contents."
      cat "${log}"
      echo "END LOGFILE Contents."
    fi
    return 1
  fi
  return 0
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
  egrep "^#@" "${SCRIPT_PATH}" | cut -b 3-
}

Usage2() {
  egrep "^#(@|\+)" "${SCRIPT_PATH}" | cut -b 3-
}

Banner() {
  echo ""
  echo " *********************************************************************"
  echo " | "
  for arg in "$@" ; do
    echo " | ${arg}"
  done
  echo " | "
  echo " *********************************************************************"
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
    echo "[$(TimeStamp)] ${module}${padding}" "$@"
  fi
}

TimeStamp() {
  if date --version &> /dev/null ; then
    # GNU 'date'
    date -d "now - ${TIME_AT_STARTUP}sec" '+%M:%S'
  else
    # Other 'date' (assuming BSD for now)
    local time_now=$(date '+%s')
    local time_delta=$[ ${time_now} - ${TIME_AT_STARTUP} ]
    date -j -f "%s" "${time_delta}" "+%M:%S"
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


Fatal() {
  echo 1>&2
  echo "$@" 1>&2
  echo 1>&2
  exit -1
}

PackageCheck() {
  assert-bin "makeinfo" "makeinfo not found. Please install 'texinfo' package."
  assert-bin "bison"    "bison not found. Please install 'bison' package."
  assert-bin "flex"     "flex not found. Please install 'flex' package."
  assert-bin "gclient"  "gclient not found in PATH. Please install depot_tools."
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
  local F=$(file -b "$1")
  [[ "${F}" =~ "ELF" ]]
}

is-Mach() {
  local F=$(file -b "$1")
  [[ "${F}" =~ "Mach-O" ]]
}

is-shell-script() {
  local F=$(file -b "$1")
  [[ "${F}" =~ "shell" ]]
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

# On Linux with GNU readlink, "readlink -f" would be a quick way
# of getting an absolute path, but MacOS has BSD readlink.
GetAbsolutePath() {
  local relpath=$1
  local reldir
  local relname
  if [ -d "${relpath}" ]; then
    reldir="${relpath}"
    relname=""
  else
    reldir="$(dirname "${relpath}")"
    relname="/$(basename "${relpath}")"
  fi

  local absdir="$(cd "${reldir}" && pwd)"
  echo "${absdir}${relname}"
}


# The Queue* functions provide a simple way of keeping track
# of multiple background processes in order to ensure that:
# 1) they all finish successfully (with return value 0), and
# 2) they terminate if the master process is killed/interrupted.
#    (This is done using a bash "trap" handler)
#
# Example Usage:
#     command1 &
#     QueueLastProcess
#     command2 &
#     QueueLastProcess
#     command3 &
#     QueueLastProcess
#     echo "Waiting for commands finish..."
#     QueueWait
#
# TODO(pdox): Right now, this abstraction is only used for
# paralellizing translations in the self-build. If we're going
# to use this for anything more complex, then throttling the
# number of active processes to exactly PNACL_CONCURRENCY would
# be a useful feature.
CT_WAIT_QUEUE=""
QueueLastProcess() {
  local pid=$!
  CT_WAIT_QUEUE+=" ${pid}"
  if ! QueueConcurrent ; then
    QueueWait
  fi
}

QueueConcurrent() {
  [ ${PNACL_CONCURRENCY} -gt 1 ]
}

QueueWait() {
  for pid in ${CT_WAIT_QUEUE} ; do
    wait ${pid}
  done
  CT_WAIT_QUEUE=""
}

# Add a trap so that if the user Ctrl-C's or kills
# this script while background processes are running,
# the background processes don't keep going.
trap 'QueueKill' SIGINT SIGTERM SIGHUP
QueueKill() {
  echo
  echo "Killing queued processes: ${CT_WAIT_QUEUE}"
  for pid in ${CT_WAIT_QUEUE} ; do
    kill ${pid} &> /dev/null || true
  done
  echo
  CT_WAIT_QUEUE=""
}

QueueEmpty() {
  [ "${CT_WAIT_QUEUE}" == "" ]
}

#+ RunWithRetry <max_retries> <delay_sec> cmd [args]
RunWithRetry() {
  local max_retries="$1"
  local delay_sec="$2"
  local cmdname=$(basename "$3")
  shift 2

  local retries=0
  while true; do
    if "$@" ; then
      return 0
    fi

    StepBanner "RUN-WITH-RETRY" "Execution of '${cmdname}' failed."
    retries=$[retries+1]
    if [ ${retries} -lt ${max_retries} ]; then
      StepBanner "RUN-WITH-RETRY" "Retrying in ${delay_sec} seconds."
      sleep ${delay_sec}
    else
      StepBanner "RUN-WITH-RETRY" \
        "'${cmdname}' failed ${max_retries} times. Aborting."
      return 1
    fi
  done

}
