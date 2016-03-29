#!/bin/sh
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests the libaom aom_temporal_svc_encoder example. To add new
## tests to this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to aom_tsvc_encoder_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
aom_tsvc_encoder_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ "$(aom_config_option_enabled CONFIG_TEMPORAL_DENOISING)" != "yes" ]; then
    elog "Warning: Temporal denoising is disabled! Spatial denoising will be " \
      "used instead, which is probably not what you want for this test."
  fi
}

# Runs aom_temporal_svc_encoder using the codec specified by $1 and output file
# name by $2. Additional positional parameters are passed directly to
# aom_temporal_svc_encoder.
aom_tsvc_encoder() {
  local encoder="${LIBAOM_BIN_PATH}/aom_temporal_svc_encoder"
  encoder="${encoder}${AOM_TEST_EXE_SUFFIX}"
  local codec="$1"
  local output_file_base="$2"
  local output_file="${AOM_TEST_OUTPUT_DIR}/${output_file_base}"
  local timebase_num="1"
  local timebase_den="1000"
  local speed="6"
  local frame_drop_thresh="30"

  shift 2

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${YUV_RAW_INPUT}" "${output_file}" \
      "${codec}" "${YUV_RAW_INPUT_WIDTH}" "${YUV_RAW_INPUT_HEIGHT}" \
      "${timebase_num}" "${timebase_den}" "${speed}" "${frame_drop_thresh}" \
      "$@" \
      ${devnull}
}

# Confirms that all expected output files exist given the output file name
# passed to aom_temporal_svc_encoder.
# The file name passed to aom_temporal_svc_encoder is joined with the stream
# number and the extension .ivf to produce per stream output files.  Here $1 is
# file name, and $2 is expected number of files.
files_exist() {
  local file_name="${AOM_TEST_OUTPUT_DIR}/$1"
  local num_files="$(($2 - 1))"
  for stream_num in $(seq 0 ${num_files}); do
    [ -e "${file_name}_${stream_num}.ivf" ] || return 1
  done
}

# Run aom_temporal_svc_encoder in all supported modes for aom and av1.

aom_tsvc_encoder_aom_mode_0() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 0 200 || return 1
    # Mode 0 produces 1 stream
    files_exist "${FUNCNAME}" 1 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_1() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 1 200 400 || return 1
    # Mode 1 produces 2 streams
    files_exist "${FUNCNAME}" 2 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_2() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 2 200 400 || return 1
    # Mode 2 produces 2 streams
    files_exist "${FUNCNAME}" 2 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_3() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 3 200 400 600 || return 1
    # Mode 3 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_4() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 4 200 400 600 || return 1
    # Mode 4 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_5() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 5 200 400 600 || return 1
    # Mode 5 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_6() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 6 200 400 600 || return 1
    # Mode 6 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_7() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 7 200 400 600 800 1000 || return 1
    # Mode 7 produces 5 streams
    files_exist "${FUNCNAME}" 5 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_8() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 8 200 400 || return 1
    # Mode 8 produces 2 streams
    files_exist "${FUNCNAME}" 2 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_9() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 9 200 400 600 || return 1
    # Mode 9 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_10() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 10 200 400 600 || return 1
    # Mode 10 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_aom_mode_11() {
  if [ "$(aom_encode_available)" = "yes" ]; then
    aom_tsvc_encoder aom "${FUNCNAME}" 11 200 400 600 || return 1
    # Mode 11 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_0() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 0 200 || return 1
    # Mode 0 produces 1 stream
    files_exist "${FUNCNAME}" 1 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 1 200 400 || return 1
    # Mode 1 produces 2 streams
    files_exist "${FUNCNAME}" 2 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_2() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 2 200 400 || return 1
    # Mode 2 produces 2 streams
    files_exist "${FUNCNAME}" 2 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_3() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 3 200 400 600 || return 1
    # Mode 3 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_4() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 4 200 400 600 || return 1
    # Mode 4 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_5() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 5 200 400 600 || return 1
    # Mode 5 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_6() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 6 200 400 600 || return 1
    # Mode 6 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_7() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 7 200 400 600 800 1000 || return 1
    # Mode 7 produces 5 streams
    files_exist "${FUNCNAME}" 5 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_8() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 8 200 400 || return 1
    # Mode 8 produces 2 streams
    files_exist "${FUNCNAME}" 2 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_9() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 9 200 400 600 || return 1
    # Mode 9 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_10() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 10 200 400 600 || return 1
    # Mode 10 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_av1_mode_11() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    aom_tsvc_encoder av1 "${FUNCNAME}" 11 200 400 600 || return 1
    # Mode 11 produces 3 streams
    files_exist "${FUNCNAME}" 3 || return 1
  fi
}

aom_tsvc_encoder_tests="aom_tsvc_encoder_aom_mode_0
                        aom_tsvc_encoder_aom_mode_1
                        aom_tsvc_encoder_aom_mode_2
                        aom_tsvc_encoder_aom_mode_3
                        aom_tsvc_encoder_aom_mode_4
                        aom_tsvc_encoder_aom_mode_5
                        aom_tsvc_encoder_aom_mode_6
                        aom_tsvc_encoder_aom_mode_7
                        aom_tsvc_encoder_aom_mode_8
                        aom_tsvc_encoder_aom_mode_9
                        aom_tsvc_encoder_aom_mode_10
                        aom_tsvc_encoder_aom_mode_11
                        aom_tsvc_encoder_av1_mode_0
                        aom_tsvc_encoder_av1_mode_1
                        aom_tsvc_encoder_av1_mode_2
                        aom_tsvc_encoder_av1_mode_3
                        aom_tsvc_encoder_av1_mode_4
                        aom_tsvc_encoder_av1_mode_5
                        aom_tsvc_encoder_av1_mode_6
                        aom_tsvc_encoder_av1_mode_7
                        aom_tsvc_encoder_av1_mode_8
                        aom_tsvc_encoder_av1_mode_9
                        aom_tsvc_encoder_av1_mode_10
                        aom_tsvc_encoder_av1_mode_11"

run_tests aom_tsvc_encoder_verify_environment "${aom_tsvc_encoder_tests}"
