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
##  This file tests the libaom aom_multi_resolution_encoder example. To add new
##  tests to this file, do the following:
##    1. Write a shell function (this is your test).
##    2. Add the function to aom_mre_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
aom_multi_resolution_encoder_verify_environment() {
  if [ "$(aom_config_option_enabled CONFIG_MULTI_RES_ENCODING)" = "yes" ]; then
    if [ ! -e "${YUV_RAW_INPUT}" ]; then
      elog "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
      return 1
    fi
    local readonly app="aom_multi_resolution_encoder"
    if [ -z "$(aom_tool_path "${app}")" ]; then
      elog "${app} not found. It must exist in LIBAOM_BIN_PATH or its parent."
      return 1
    fi
  fi
}

# Runs aom_multi_resolution_encoder. Simply forwards all arguments to
# aom_multi_resolution_encoder after building path to the executable.
aom_mre() {
  local readonly encoder="$(aom_tool_path aom_multi_resolution_encoder)"
  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "$@" ${devnull}
}

aom_multi_resolution_encoder_three_formats() {
  local readonly output_files="${AOM_TEST_OUTPUT_DIR}/aom_mre_0.ivf
                               ${AOM_TEST_OUTPUT_DIR}/aom_mre_1.ivf
                               ${AOM_TEST_OUTPUT_DIR}/aom_mre_2.ivf"

  if [ "$(aom_config_option_enabled CONFIG_MULTI_RES_ENCODING)" = "yes" ]; then
    if [ "$(aom_encode_available)" = "yes" ]; then
      # Param order:
      #  Input width
      #  Input height
      #  Input file path
      #  Output file names
      #  Output PSNR
      aom_mre "${YUV_RAW_INPUT_WIDTH}" \
        "${YUV_RAW_INPUT_HEIGHT}" \
        "${YUV_RAW_INPUT}" \
        ${output_files} \
        0

      for output_file in ${output_files}; do
        if [ ! -e "${output_file}" ]; then
          elog "Missing output file: ${output_file}"
          return 1
        fi
      done
    fi
  fi
}

aom_mre_tests="aom_multi_resolution_encoder_three_formats"
run_tests aom_multi_resolution_encoder_verify_environment "${aom_mre_tests}"

