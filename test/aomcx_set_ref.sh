#!/bin/sh
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
##  This file tests the libaom aomcx_set_ref example. To add new tests to this
##  file, do the following:
##    1. Write a shell function (this is your test).
##    2. Add the function to aomcx_set_ref_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
aomcx_set_ref_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
}

# Runs aomcx_set_ref and updates the reference frame before encoding frame 90.
# $1 is the codec name, which aomcx_set_ref does not support at present: It's
# currently used only to name the output file.
# TODO(tomfinegan): Pass the codec param once the example is updated to support
# AV1.
aom_set_ref() {
  local encoder="${LIBAOM_BIN_PATH}/aomcx_set_ref${AOM_TEST_EXE_SUFFIX}"
  local codec="$1"
  local output_file="${AOM_TEST_OUTPUT_DIR}/aomcx_set_ref_${codec}.ivf"
  local ref_frame_num=90

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${YUV_RAW_INPUT_WIDTH}" \
      "${YUV_RAW_INPUT_HEIGHT}" "${YUV_RAW_INPUT}" "${output_file}" \
      "${ref_frame_num}" ${devnull}

  [ -e "${output_file}" ] || return 1
}

aomcx_set_ref_aom() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_set_ref aom || return 1
  fi
}

aomcx_set_ref_tests="aomcx_set_ref_aom"

run_tests aomcx_set_ref_verify_environment "${aomcx_set_ref_tests}"

