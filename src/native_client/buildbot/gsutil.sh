#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Just in case we're run from a batch file, fix pwd.
if [ "x${OSTYPE}" = "xcygwin" ]; then
  cd "$(cygpath "${PWD}")"
fi

readonly SCRIPT_DIR="$(dirname "$0")"
readonly SCRIPT_DIR_ABS="$(cd "${SCRIPT_DIR}" ; pwd)"

set -e
set -u
set -x

# Use the batch file as an entry point if on cygwin.
if [ "x${OSTYPE}" = "xcygwin" ]; then
  # Use extended globbing (cygwin should always have it).
  shopt -s extglob
  # Filter out cygwin python (everything under /usr or /bin, or *cygwin*).
  export PATH=${PATH/#\/bin*([^:])/}
  export PATH=${PATH//:\/bin*([^:])/}
  export PATH=${PATH/#\/usr*([^:])/}
  export PATH=${PATH//:\/usr*([^:])/}
  export PATH=${PATH/#*([^:])cygwin*([^:])/}
  export PATH=${PATH//:*([^:])cygwin*([^:])/}
  gsutil="${SCRIPT_DIR_ABS}/../../../../../scripts/slave/gsutil.bat"
else
  gsutil=/b/build/scripts/slave/gsutil
fi

"${gsutil}" "$@"
